/*-
 * Copyright (c) 2012-2014 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012-2014 Otto Gmbh & Co KG
 * All rights reserved
 * Use only with permission
 *
 * Authors: Geoffrey Simmons <geoffrey.simmons@uplex.de>
 *	    Nils Goroll <nils.goroll@uplex.de>
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
#include <errno.h>

#include "trackrdrd.h"
#include "vdef.h"
#include "vas.h"
#include "miniobj.h"
#include "vsb.h"

#define VERSION_LEN 80
#define CLIENT_ID_LEN 80

struct mqf mqf;
unsigned abandoned;

static int running = 0, exited = 0;

typedef enum {
    WRK_NOTSTARTED = 0,
    WRK_INITIALIZING,
    WRK_RUNNING,
    WRK_WAITING,
    WRK_SHUTTINGDOWN,
    WRK_EXITED,
    WRK_ABANDONED,
    WRK_STATE_E_LIMIT
} wrk_state_e;

static const char* statename[WRK_STATE_E_LIMIT] = { 
    [WRK_NOTSTARTED]    = "not started",
    [WRK_INITIALIZING]	= "initializing",
    [WRK_RUNNING]	= "running",
    [WRK_WAITING]	= "waiting",
    [WRK_SHUTTINGDOWN]	= "shutting down",
    [WRK_EXITED]	= "exited",
    [WRK_ABANDONED]	= "abandoned"
};

struct worker_data_s {
    unsigned magic;
#define WORKER_DATA_MAGIC 0xd8eef137
    unsigned id;
    unsigned status;  /* exit status */
    wrk_state_e state;
    struct vsb *sb;

    /* per-worker freelists */
    struct rechead_s	freerec;
    unsigned		nfree_rec;
    chunkhead_t		freechunk;
    unsigned		nfree_chunk;

    /* stats */
    unsigned long deqs;
    unsigned long waits;
    unsigned long sends;
    unsigned long bytes;
    unsigned long fails;
    unsigned long recoverables;
    unsigned long reconnects;
    unsigned long restarts;
};

typedef struct worker_data_s worker_data_t;

typedef struct {
    pthread_t worker;
    worker_data_t *wrk_data;
} thread_data_t;
    
static unsigned run, cleaned = 0, rec_thresh, chunk_thresh;
static thread_data_t *thread_data;

static pthread_mutex_t running_lock;

static char empty[1] = "";

static void
wrk_log_connection(void *mq_worker, unsigned id)
{
    const char *err;
    char version[VERSION_LEN], clientID[CLIENT_ID_LEN];

    err = mqf.version(mq_worker, version, VERSION_LEN);
    if (err != NULL) {
        LOG_Log(LOG_ERR, "Worker %d: Failed to get MQ version", id, err);
        version[0] = '\0';
    }
    err = mqf.client_id(mq_worker, clientID, CLIENT_ID_LEN);
    if (err != NULL) {
        LOG_Log(LOG_ERR, "Worker %d: Failed to get MQ client ID", id, err);
        clientID[0] = '\0';
    }
    LOG_Log(LOG_INFO, "Worker %d: connected (%s, id = %s)", id, version,
            clientID);
}

static char *
wrk_get_data(dataentry *entry, worker_data_t *wrk) {
    CHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
    assert(OCCUPIED(entry));

    if (entry->end == 0)
        return empty;

    chunk_t *chunk = VSTAILQ_FIRST(&entry->chunks);
    CHECK_OBJ_NOTNULL(chunk, CHUNK_MAGIC);
    assert(OCCUPIED(chunk));
    if (entry->end <= config.chunk_size)
        return chunk->data;

    VSB_clear(wrk->sb);
    int n = entry->end;
    while (n > 0) {
        CHECK_OBJ_NOTNULL(chunk, CHUNK_MAGIC);
        int cp = n;
        if (cp > config.chunk_size)
            cp = config.chunk_size;
        VSB_bcat(wrk->sb, chunk->data, cp);
        n -= cp;
        chunk = VSTAILQ_NEXT(chunk, chunklist);
    }
    assert(VSB_len(wrk->sb) == entry->end);
    VSB_finish(wrk->sb);
    return VSB_data(wrk->sb);
}

static inline void
wrk_return_freelist(worker_data_t *wrk)
{
    if (wrk->nfree_rec > 0) {
        DATA_Return_Freerec(&wrk->freerec, wrk->nfree_rec);
        LOG_Log(LOG_DEBUG, "Worker %d: returned %u records to free list",
                wrk->id, wrk->nfree_rec);
        wrk->nfree_rec = 0;
        assert(VSTAILQ_EMPTY(&wrk->freerec));
    }
    if (wrk->nfree_chunk > 0) {
        DATA_Return_Freechunk(&wrk->freechunk, wrk->nfree_chunk);
        LOG_Log(LOG_DEBUG, "Worker %d: returned %u chunks to free list",
                wrk->id, wrk->nfree_chunk);
        wrk->nfree_chunk = 0;
        assert(VSTAILQ_EMPTY(&wrk->freechunk));
    }
}

static inline void
wrk_send(void **mq_worker, dataentry *entry, worker_data_t *wrk)
{
    char *data;
    const char *err;
    int errnum;
    stats_update_t stat = STATS_FAILED;
    unsigned bytes = 0;
    
    CHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
    assert(OCCUPIED(entry));
    AN(mq_worker);

    data = wrk_get_data(entry, wrk);
    AZ(memchr(data, '\0', entry->end));
    errnum = mqf.send(*mq_worker, data, entry->end,
                      entry->key, entry->keylen, &err);
    if (errnum != 0) {
        LOG_Log(LOG_WARNING, "Worker %d: Failed to send data: %s",
                wrk->id, err);
        if (errnum > 0)
            wrk->recoverables++;
        else {
            /* Non-recoverable error */
            LOG_Log(LOG_INFO, "Worker %d: Reconnecting", wrk->id);
            err = mqf.reconnect(mq_worker);
            if (err != NULL) {
                wrk->status = EXIT_FAILURE;
                LOG_Log(LOG_ALERT, "Worker %d: Reconnect failed (%s)", wrk->id,
                        err);
                LOG_Log(LOG_ERR, "Worker %d: Data DISCARDED [%.*s]", wrk->id,
                        entry->end, data);
            }
            else {
                wrk->reconnects++;
                wrk_log_connection(*mq_worker, wrk->id);
                MON_StatsUpdate(STATS_RECONNECT, 0, 0);
                errnum = mqf.send(*mq_worker, data, entry->end,
                                  entry->key, entry->keylen, &err);
                if (errnum != 0) {
                    LOG_Log(LOG_WARNING, "Worker %d: Failed to send data "
                            "after reconnect: %s", wrk->id, err);
                    if (errnum > 0)
                        wrk->recoverables++;
                    else {
                        /* Fail after reconnect, give up */
                        wrk->fails++;
                        wrk->status = EXIT_FAILURE;
                        LOG_Log(LOG_ERR, "Worker %d: Data DISCARDED [%.*s]",
                                wrk->id, entry->end, data);
                    }
                }
            }
        }
    }
    if (errnum == 0) {
        wrk->sends++;
        wrk->bytes += entry->end;
        stat = STATS_SENT;
        bytes = entry->end;
        LOG_Log(LOG_DEBUG, "Worker %d: Successfully sent data [%.*s]", wrk->id,
                entry->end, data);
    }
    unsigned chunks = DATA_Reset(entry, &wrk->freechunk);
    MON_StatsUpdate(stat, chunks, bytes);
    VSTAILQ_INSERT_HEAD(&wrk->freerec, entry, freelist);
    wrk->nfree_rec++;
    wrk->nfree_chunk += chunks;

    if (RDR_Exhausted() || wrk->nfree_rec > rec_thresh
        || wrk->nfree_chunk > chunk_thresh)
        wrk_return_freelist(wrk);
}

static void
*wrk_main(void *arg)
{
    worker_data_t *wrk = (worker_data_t *) arg;
    void *mq_worker;
    dataentry *entry;
    const char *err;

    CHECK_OBJ_NOTNULL(wrk, WORKER_DATA_MAGIC);
    LOG_Log(LOG_INFO, "Worker %d: starting", wrk->id);
    wrk->state = WRK_INITIALIZING;

    err = mqf.worker_init(&mq_worker, wrk->id);
    if (err != NULL) {
        LOG_Log(LOG_ALERT, "Worker %d: Cannot initialize queue connection: %s",
            wrk->id, err);
        wrk->status = EXIT_FAILURE;
        wrk->state = WRK_EXITED;
        AZ(pthread_mutex_lock(&running_lock));
        exited++;
        AZ(pthread_mutex_unlock(&running_lock));
        pthread_exit((void *) wrk);
    }

    wrk_log_connection(mq_worker, wrk->id);

    wrk->state = WRK_RUNNING;
    AZ(pthread_mutex_lock(&running_lock));
    running++;
    AZ(pthread_mutex_unlock(&running_lock));

    while (run) {
        entry = SPMCQ_Deq();
        if (entry != NULL) {
            wrk->deqs++;
            wrk_send(&mq_worker, entry, wrk);

            if (wrk->status == EXIT_FAILURE)
                break;
            continue;
        }

        /* return space before sleeping */
        wrk_return_freelist(wrk);

        /*
         * Queue is empty, wait until data are available, or quit is
         * signaled.
         *
         * Grab the CV lock, which also constitutes an implicit memory
         * barrier 
         */
        AZ(pthread_mutex_lock(&spmcq_datawaiter_lock));
        /*
         * run is guaranteed to be fresh here
         */
        SPMCQ_Drain();
        if (run) {
            wrk->waits++;
            spmcq_datawaiter++;
            wrk->state = WRK_WAITING;
            AZ(pthread_cond_wait(&spmcq_datawaiter_cond,
                                 &spmcq_datawaiter_lock));
            spmcq_datawaiter--;
            wrk->state = WRK_RUNNING;
        }
        AZ(pthread_mutex_unlock(&spmcq_datawaiter_lock));
    }

    wrk->state = WRK_SHUTTINGDOWN;

    if (wrk->status != EXIT_FAILURE) {
        /* Prepare to exit, drain the queue */
        while ((entry = SPMCQ_Deq()) != NULL) {
            wrk->deqs++;
            wrk_send(&mq_worker, entry, wrk);
        }
        wrk->status = EXIT_SUCCESS;
    }
    
    err = mqf.worker_shutdown(&mq_worker, wrk->id);
    if (err != NULL) {
        LOG_Log(LOG_ALERT, "Worker %d: MQ worker shutdown failed: %s",
                wrk->id, err);
        wrk->status = EXIT_FAILURE;
    }

    AZ(pthread_mutex_lock(&running_lock));
    running--;
    exited++;
    AZ(pthread_mutex_unlock(&running_lock));
    free(wrk->sb->s_buf);
    VSB_fini(wrk->sb);
    free(wrk->sb);
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
    AZ(pthread_mutex_destroy(&spmcq_datawaiter_lock));
    AZ(pthread_cond_destroy(&spmcq_datawaiter_cond));
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
        char *buf;

        thread_data[i].wrk_data
            = (worker_data_t *) malloc(sizeof(worker_data_t));
        if (thread_data[i].wrk_data == NULL) {
            LOG_Log(LOG_ALERT, "Cannot allocate worker data for worker %d: %s",
                i+1, strerror(errno));
            return(errno);
        }
        
        worker_data_t *wrk = thread_data[i].wrk_data;
        wrk->magic = WORKER_DATA_MAGIC;
        wrk->sb = (struct vsb *) malloc(sizeof(struct vsb));
        AN(wrk->sb);
        buf = malloc(config.max_reclen + 1);
        AN(VSB_init(wrk->sb, buf, config.max_reclen + 1));
        VSTAILQ_INIT(&wrk->freerec);
        wrk->nfree_rec = 0;
        VSTAILQ_INIT(&wrk->freechunk);
        wrk->nfree_chunk = 0;
        wrk->id = i + 1;
        wrk->deqs = wrk->waits = wrk->sends = wrk->fails = wrk->reconnects
            = wrk->restarts = wrk->recoverables = wrk->bytes = 0;
        wrk->state = WRK_NOTSTARTED;
    }

    spmcq_datawaiter = 0;
    AZ(pthread_mutex_init(&spmcq_datawaiter_lock, NULL));
    AZ(pthread_cond_init(&spmcq_datawaiter_cond, NULL));

    rec_thresh = (config.max_records >> 1) / config.nworkers;
    chunk_thresh = rec_thresh *
        ((config.max_reclen + config.chunk_size - 1) / config.chunk_size);

    atexit(wrk_cleanup);
    return 0;
}

static void
wrk_pthread_attr_init(pthread_attr_t *attr)
{
    AZ(pthread_attr_init(attr));
    AZ(pthread_attr_setstacksize(attr, config.worker_stack));
}

void
WRK_Start(void)
{
    pthread_attr_t attr;

    wrk_pthread_attr_init(&attr);
    run = 1;
    for (int i = 0; i < config.nworkers; i++) {
        CHECK_OBJ_NOTNULL(thread_data[i].wrk_data, WORKER_DATA_MAGIC);
        AZ(pthread_create(&thread_data[i].worker, &attr, wrk_main,
                          thread_data[i].wrk_data));
    }
    AZ(pthread_attr_destroy(&attr));
}

int
WRK_Restart(void)
{
    int err = 0;
    worker_data_t *wrk;
    pthread_attr_t attr;

    wrk_pthread_attr_init(&attr);

    for (int i = 0; i < config.nworkers; i++) {
        CHECK_OBJ_NOTNULL(thread_data[i].wrk_data, WORKER_DATA_MAGIC);
        wrk = thread_data[i].wrk_data;
        if (wrk->state == WRK_EXITED) {
            if (config.thread_restarts != 0
                && wrk->restarts == config.thread_restarts) {
                LOG_Log(LOG_ALERT, "Worker %d: too many restarts, abandoning",
                    wrk->id);
                abandoned++;
                wrk->state = WRK_ABANDONED;
                continue;
            }
            AZ(pthread_mutex_lock(&running_lock));
            exited--;
            AZ(pthread_mutex_unlock(&running_lock));
            AZ(pthread_detach(thread_data[i].worker));
            wrk->deqs = wrk->waits = wrk->sends = wrk->fails = wrk->reconnects
                = 0;
            wrk->restarts++;
            MON_StatsUpdate(STATS_RESTART, 0, 0);
            wrk->state = WRK_NOTSTARTED;
            if (pthread_create(&thread_data[i].worker, &attr, wrk_main, wrk)
                != 0) {
                /* EAGAIN means we've hit a system limit trying to restart
                   threads, so it's time to give up. Any other errno is a
                   programming error.
                */
                assert(errno == EAGAIN);
                err = errno;
                break;
            }
        }
    }
    AZ(pthread_attr_destroy(&attr));
    return err;
}

void
WRK_Stats(void)
{
    worker_data_t *wrk;

    if (!run) return;
    
    for (int i = 0; i < config.nworkers; i++) {
        wrk = thread_data[i].wrk_data;
        LOG_Log(LOG_INFO,
                "Worker %d (%s): seen=%lu waits=%lu sent=%lu bytes=%lu "
                "free_rec=%u free_chunk=%u reconnects=%lu restarts=%lu "
                "failed_recoverable=%lu failed=%lu",
                wrk->id, statename[wrk->state], wrk->deqs, wrk->waits,
                wrk->sends, wrk->bytes, wrk->nfree_rec, wrk->nfree_chunk,
                wrk->reconnects, wrk->restarts, wrk->recoverables, wrk->fails);
    }
}

int
WRK_Running(void)
{
    return running;
}

int
WRK_Exited(void)
{
    return exited;
}

void
WRK_Halt(void)
{
    /*
     * must only modify run under spmcq_datawaiter_lock to ensure that
     * we signal all waiting consumers (otherwise a consumer could go
     * waiting _after_ we have broadcasted and so miss the event.
     */
    AZ(pthread_mutex_lock(&spmcq_datawaiter_lock));
    SPMCQ_Drain();
    run = 0;
    AZ(pthread_cond_broadcast(&spmcq_datawaiter_cond));
    AZ(pthread_mutex_unlock(&spmcq_datawaiter_lock));

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
