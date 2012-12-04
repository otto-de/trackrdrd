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

#include <pthread.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>

#include "trackrdrd.h"
#include "vas.h"
#include "miniobj.h"
#include "vmb.h"

typedef enum {
    WRK_NOTSTARTED = 0,
    WRK_INITIALIZING,
    WRK_RUNNING,
    WRK_SHUTTINGDOWN,
    WRK_EXITED
} wrk_state_e;

typedef struct {
    unsigned magic;
#define WORKER_DATA_MAGIC 0xd8eef137
    unsigned id;
    unsigned status;  /* exit status */
    wrk_state_e state;
    unsigned deqs;
    unsigned waits;
    unsigned sends;
    unsigned fails;
} worker_data_t;

typedef struct {
    pthread_t worker;
    worker_data_t *wrk_data;
} thread_data_t;
    
static unsigned run, cleaned = 0;
static thread_data_t *thread_data;
static const char* statename[5] = { "not started", "initializing", "running",
                                    "shutting down", "exited" };

static inline void
wrk_send(void *amq_worker, dataentry *entry, worker_data_t *wrk)
{
    const char *err;
    
    CHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
    assert(entry->state == DATA_DONE);
    AN(amq_worker);

    err = MQ_Send(amq_worker, entry->data, entry->end);
    if (err != NULL) {
        /* XXX: error recovery? reconnect? preserve the data? */
        wrk->fails++;
        LOG_Log(LOG_ALERT, "Worker %d: Failed to send data: %s", wrk->id, err);
        LOG_Log(LOG_ERR, "Worker %d: Data DISCARDED [%.*s]", wrk->id,
            entry->end, entry->data);
        MON_StatsUpdate(STATS_FAILED);
    }
    else {
        wrk->sends++;
        MON_StatsUpdate(STATS_SENT);
        LOG_Log(LOG_DEBUG, "Worker %d: Successfully sent data [%.*s]", wrk->id,
            entry->end, entry->data);
    }
    entry->state = DATA_EMPTY;
    /* From Varnish vmb.h -- platform-independent write memory barrier */
    VWMB();
    AZ(pthread_cond_signal(&spmcq_nonfull_cond));
}

static void
*wrk_main(void *arg)
{
    worker_data_t *wrk = (worker_data_t *) arg;
    void *amq_worker;
    dataentry *entry;
    const char *err;

    LOG_Log(LOG_INFO, "Worker %d: starting", wrk->id);
    CHECK_OBJ_NOTNULL(wrk, WORKER_DATA_MAGIC);
    wrk->state = WRK_INITIALIZING;
    
    err = MQ_WorkerInit(&amq_worker);
    if (err != NULL) {
        LOG_Log(LOG_ALERT, "Worker %d: Cannot initialize queue connection: %s",
            wrk->id, err);
        wrk->status = EXIT_FAILURE;
        wrk->state = WRK_EXITED;
        pthread_exit((void *) wrk);
    }

    wrk->state = WRK_RUNNING;
    
    while (run) {
	entry = (dataentry *) SPMCQ_Deq();
	if (entry != NULL) {
	    wrk->deqs++;
            wrk_send(amq_worker, entry, wrk);
            continue;
        }
        /* Queue is empty, wait until data are available, or quit is
           signaled.
           Grab the CV lock, which also constitutes an implicit memory
           barrier */
        AZ(pthread_mutex_lock(&spmcq_nonempty_lock));
        /* run is guaranteed to be fresh here */
        if (run) {
            wrk->waits++;
            AZ(pthread_cond_wait(&spmcq_nonempty_cond,
                    &spmcq_nonempty_lock));
        }
        AZ(pthread_mutex_unlock(&spmcq_nonempty_lock));
    }

    wrk->state = WRK_SHUTTINGDOWN;
    
    /* Prepare to exit, drain the queue */
    while ((entry = (dataentry *) SPMCQ_Deq()) != NULL) {
        wrk->deqs++;
        wrk_send(amq_worker, entry, wrk);
    }

    wrk->status = EXIT_SUCCESS;
    err = MQ_WorkerShutdown(&amq_worker);
    if (err != NULL) {
        LOG_Log(LOG_ALERT, "Worker %d: MQ worker shutdown failed: %s",
            wrk->id, err);
        wrk->status = EXIT_FAILURE;
    }
    
    LOG_Log(LOG_INFO, "Worker %d: exiting", wrk->id);
    wrk->state = WRK_EXITED;
    pthread_exit((void *) wrk);
}

static void wrk_cleanup(void)
{
    if (cleaned) return;
    
    for (int i = 0; i < config.nworkers; i++)
        free(thread_data[i].wrk_data);
    free(thread_data);
    AZ(pthread_mutex_destroy(&spmcq_nonempty_lock));
    AZ(pthread_cond_destroy(&spmcq_nonempty_cond));
    AZ(pthread_mutex_destroy(&spmcq_nonfull_lock));
    AZ(pthread_cond_destroy(&spmcq_nonfull_cond));
    cleaned = 1;
}

int
WRK_Init(void)
{
    thread_data
        = (thread_data_t *) malloc(config.nworkers * sizeof(thread_data_t));
    if (thread_data == NULL) {
        LOG_Log(LOG_ALERT, "Cannot allocate thread data: %s", strerror(errno));
        return(errno);
    }
    
    run = 1;
    for (int i = 0; i < config.nworkers; i++) {
        thread_data[i].wrk_data
            = (worker_data_t *) malloc(sizeof(worker_data_t));
        if (thread_data[i].wrk_data == NULL) {
            LOG_Log(LOG_ALERT, "Cannot allocate worker data for worker %d: %s",
                i+1, strerror(errno));
            return(errno);
        }
        
        worker_data_t *wrk = thread_data[i].wrk_data;
        wrk->magic = WORKER_DATA_MAGIC;
        wrk->id = i + 1;
        wrk->deqs = wrk->waits = wrk->sends = wrk->fails = 0;
        wrk->state = WRK_NOTSTARTED;
    }
    
    AZ(pthread_mutex_init(&spmcq_nonempty_lock, NULL));
    AZ(pthread_cond_init(&spmcq_nonempty_cond, NULL));
    AZ(pthread_mutex_init(&spmcq_nonfull_lock, NULL));
    AZ(pthread_cond_init(&spmcq_nonfull_cond, NULL));
    atexit(wrk_cleanup);
    return 0;
}

void
WRK_Start(void)
{
    run = 1;
    for (int i = 0; i < config.nworkers; i++)
        AZ(pthread_create(&thread_data[i].worker, NULL, wrk_main,
                thread_data[i].wrk_data));
}

void
WRK_Stats(void)
{
    worker_data_t *wrk;

    if (!run) return;
    
    for (int i = 0; i < config.nworkers; i++) {
        wrk = thread_data[i].wrk_data;
        LOG_Log(LOG_INFO, "Worker %d (%s): seen=%d waits=%d sent=%d failed=%d",
            wrk->id, statename[wrk->state], wrk->deqs, wrk->waits, wrk->sends,
            wrk->fails);
    }
}

int
WRK_Running(void)
{
    worker_data_t *wrk;

    while (1) {
        int initialized = 0, running = 0;
        for (int i = 0; i < config.nworkers; i++) {
            wrk = thread_data[i].wrk_data;
            if (wrk->state > WRK_INITIALIZING)
                initialized++;
            if (wrk->state == WRK_RUNNING || wrk->state == WRK_SHUTTINGDOWN)
                running++;
        }
        if (initialized == config.nworkers)
            return running;
    }
}

void
WRK_Halt(void)
{
    /*
     * must only modify run under spmcq_nonempty_lock to ensure that
     * we signal all waiting consumers (otherwise a consumer could go
     * waiting _after_ we have broadcasted and so miss the event.
     */
    AZ(pthread_mutex_lock(&spmcq_nonempty_lock));
    run = 0;
    AZ(pthread_cond_broadcast(&spmcq_nonempty_cond));
    AZ(pthread_mutex_unlock(&spmcq_nonempty_lock));

    for(int i = 0; i < config.nworkers; i++) {
        AZ(pthread_join(thread_data[i].worker,
                        (void **) &thread_data[i].wrk_data));
        CHECK_OBJ_NOTNULL(thread_data[i].wrk_data, WORKER_DATA_MAGIC);
        if (thread_data[i].wrk_data->status != EXIT_SUCCESS)
            LOG_Log(LOG_ERR, "Worker %d returned failure status", i+1);
    }
}

void
WRK_Shutdown(void)
{
    /* XXX: error if run=1? */
    wrk_cleanup();
}
