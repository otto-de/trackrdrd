/*-
 * Copyright (c) 2012-2014 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012-2014 Otto Gmbh & Co KG
 * All rights reserved
 * Use only with permission
 *
 * Author: Geoffrey Simmons <geoffrey.simmons@uplex.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Single producer multiple consumer bounded FIFO queue
 */

#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include <errno.h>
#include <string.h>

#include "trackrdrd.h"
#include "vdef.h"
#include "vas.h"
#include "vqueue.h"

pthread_cond_t  spmcq_datawaiter_cond;
pthread_mutex_t spmcq_datawaiter_lock;
int		spmcq_datawaiter;

static volatile unsigned long enqs = 0, deqs = 0;
static pthread_mutex_t spmcq_lock;
static pthread_mutex_t spmcq_deq_lock;
static unsigned qlen_goal;

VSTAILQ_HEAD(spmcq_s, dataentry_s);
struct spmcq_s spmcq_head = VSTAILQ_HEAD_INITIALIZER(spmcq_head);
struct spmcq_s enq_head = VSTAILQ_HEAD_INITIALIZER(enq_head);
struct spmcq_s deq_head = VSTAILQ_HEAD_INITIALIZER(deq_head);

static inline unsigned
spmcq_len(void)
{
    return enqs - deqs;
}

static void
spmcq_cleanup(void)
{
    AZ(pthread_mutex_destroy(&spmcq_lock));
    AZ(pthread_mutex_destroy(&spmcq_deq_lock));
}

int
SPMCQ_Init(void)
{
    if (pthread_mutex_init(&spmcq_lock, NULL) != 0)
        return(errno);
    if (pthread_mutex_init(&spmcq_deq_lock, NULL) != 0)
        return(errno);
    
    qlen_goal = config.qlen_goal;
    
    atexit(spmcq_cleanup);
    return(0);
}

void
SPMCQ_Enq(dataentry *ptr)
{
    AZ(pthread_mutex_lock(&spmcq_lock));
#if 0
    assert(enqs - deqs < config.max_records);
#endif
    enqs++;
    VSTAILQ_INSERT_TAIL(&enq_head, ptr, spmcq);
    if (VSTAILQ_EMPTY(&spmcq_head))
        VSTAILQ_CONCAT(&spmcq_head, &enq_head);
    AZ(pthread_mutex_unlock(&spmcq_lock));
}

dataentry
*SPMCQ_Deq(void)
{
    void *ptr;

    AZ(pthread_mutex_lock(&spmcq_deq_lock));
    if (VSTAILQ_EMPTY(&deq_head)) {
        AZ(pthread_mutex_lock(&spmcq_lock));
        VSTAILQ_CONCAT(&deq_head, &spmcq_head);
        AZ(pthread_mutex_unlock(&spmcq_lock));
    }
    if (VSTAILQ_EMPTY(&deq_head))        
        ptr = NULL;
    else {
        ptr = VSTAILQ_FIRST(&deq_head);
        VSTAILQ_REMOVE_HEAD(&deq_head, spmcq);
        deqs++;
    }
    AZ(pthread_mutex_unlock(&spmcq_deq_lock));
    return ptr;
}

void
SPMCQ_Drain(void)
{
    AZ(pthread_mutex_lock(&spmcq_lock));
    VSTAILQ_CONCAT(&spmcq_head, &enq_head);
    AZ(pthread_mutex_unlock(&spmcq_lock));
}

/*
 * should we wake up another worker?
 *
 * M = l / (u x p)
 *
 * l: arrival rate
 * u: service rate
 * p: utilization
 *
 * to get an optimal M, we would need to measure l and u, so to
 * simplify, we just try to keep the number of workers proportional to
 * the queue length
 *
 * wake up another worker if queue is sufficiently full
 * Q_Len > working * qlen_goal / max_workers
 */

static inline int
spmcq_wrk_len_ratio(int working, int running)
{
    return working * qlen_goal / running;
}

unsigned
SPMCQ_NeedWorker(int running)
{
    if (running == 0)
        return 0;
    return spmcq_len() > spmcq_wrk_len_ratio(running - spmcq_datawaiter,
                                             running);
}
