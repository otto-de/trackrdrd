/*-
 * Copyright (c) 2012 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012 Otto Gmbh & Co KG
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
 */

#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include <errno.h>
#include <string.h>

#include "trackrdrd.h"
#include "vas.h"

static pthread_mutex_t spmcq_deq_lock;

static inline unsigned
spmcq_len(void)
{
    if (spmcq.tail < spmcq.head)
        return UINT_MAX - spmcq.head - 1 - spmcq.tail;
    else
        return spmcq.tail - spmcq.head;
}

static void
spmcq_cleanup(void)
{
    free(spmcq.data);
    AZ(pthread_mutex_destroy(&spmcq_deq_lock));
}

int
SPMCQ_Init(void)
{
    void *buf;
    
    size_t n = 1 << (MIN_TABLE_SCALE + config.maxopen_scale);
    buf = calloc(n, sizeof(void *));
    if (buf == NULL)
        return(errno);

    if (pthread_mutex_init(&spmcq_deq_lock, NULL) != 0)
        return(errno);
    
    spmcq_t q =
        { .magic = SPMCQ_MAGIC, .mask = n - 1, .data = buf, .head = 0,
          .tail = 0 };
    memcpy(&spmcq, &q, sizeof(spmcq_t));
    atexit(spmcq_cleanup);
    return(0);
}

bool
SPMCQ_Enq(void *ptr)
{
    if (spmcq_len() > spmcq.mask)
        return false;
    spmcq.data[spmcq.tail++ & spmcq.mask] = ptr;
    return true;
}

void
*SPMCQ_Deq(void)
{
    void *ptr;

    AZ(pthread_mutex_lock(&spmcq_deq_lock));
    if (spmcq_len() == 0)
        ptr = NULL;
    else
        ptr = spmcq.data[spmcq.head++ & spmcq.mask];
    AZ(pthread_mutex_unlock(&spmcq_deq_lock));
    return ptr;
}
