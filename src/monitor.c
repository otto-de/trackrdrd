/*-
 * Copyright (c) 2012-2015 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012-2015 Otto Gmbh & Co KG
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

#include <syslog.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "trackrdrd.h"
#include "vdef.h"
#include "vas.h"

static int run;

static pthread_mutex_t	mutex;
static unsigned		occ = 0;
static unsigned		occ_chunk = 0;
static unsigned	long	sent = 0;	/* Sent successfully to MQ */
static unsigned	long	bytes = 0;	/* Total bytes successfully sent */
static unsigned	long	failed = 0;	/* MQ send fails */
static unsigned	long	reconnects = 0;	/* Reconnects to MQ */
static unsigned	long	restarts = 0;	/* Worker thread restarts */
static unsigned		occ_hi = 0;	/* Occupancy high water mark */ 
static unsigned		occ_hi_this = 0;/* Occupancy high water mark
                                           this reporting interval */
static unsigned		occ_chunk_hi = 0;
static unsigned		occ_chunk_hi_this = 0;

static void
log_output(void)
{
    static int wrk_running_hi = 0;
    int wrk_active = WRK_Running();
    int wrk_running = wrk_active - spmcq_datawaiter;

    if (wrk_running > wrk_running_hi)
        wrk_running_hi = wrk_running;

    LOG_Log(LOG_INFO, "Data table: len=%u occ_rec=%u occ_rec_hi=%u "
            "occ_rec_hi_this=%u occ_chunk=%u occ_chunk_hi=%u "
            "occ_chunk_hi_this=%u global_free_rec=%u global_free_chunk=%u",
            config.max_records, occ, occ_hi, occ_hi_this, occ_chunk,
            occ_chunk_hi, occ_chunk_hi_this, global_nfree_rec,
            global_nfree_chunk);

    /* Eliminate the dependency of trackrdrd.o for unit tests */
#ifndef TEST_DRIVER
    RDR_Stats();
#endif
    
    if (wrk_active < config.nworkers)
        LOG_Log(LOG_WARNING, "%d of %d workers active", wrk_running,
                config.nworkers);
    /* XXX: seen, bytes sent */
    LOG_Log(LOG_INFO, "Workers: active=%d running=%d waiting=%d running_hi=%d "
            "exited=%d abandoned=%u reconnects=%lu restarts=%lu sent=%lu "
            "failed=%lu bytes=%lu",
            wrk_active, wrk_running, spmcq_datawaiter, wrk_running_hi,
            WRK_Exited(), abandoned, reconnects, restarts, sent, failed, bytes);

    /* locking would be overkill */
    occ_hi_this = 0;
    occ_chunk_hi_this = 0;

    if (config.monitor_workers)
        WRK_Stats();
}

static void
monitor_cleanup(void *arg)
{
    (void) arg;

    log_output();
    LOG_Log0(LOG_INFO, "Monitoring thread exiting");
}

void
*MON_StatusThread(void *arg)
{
    struct timespec t;
    unsigned *interval = (unsigned *) arg;

    t.tv_sec = (time_t) *interval;
    t.tv_nsec = 0;
    LOG_Log(LOG_INFO, "Monitor thread running every %u secs", t.tv_sec);
    run = 1;

    pthread_cleanup_push(monitor_cleanup, arg);

    while (run) {
        int err;
        if (nanosleep(&t, NULL) != 0) {
            if (errno == EINTR) {
                if (run == 0)
                    break;
                LOG_Log0(LOG_WARNING, "Monitoring thread interrupted");
                continue;
            }
            else {
                LOG_Log(LOG_ERR, "Monitoring thread: %s\n", strerror(errno));
                err = errno;
                pthread_exit(&err);
            }
        }
        log_output();
    }

    pthread_cleanup_pop(0);
    LOG_Log0(LOG_INFO, "Monitoring thread exiting");
    pthread_exit((void *) NULL);
}

void
MON_Output(void)
{
    log_output();
}

void
MON_StatusShutdown(pthread_t monitor)
{
    run = 0;
    AZ(pthread_cancel(monitor));
    AZ(pthread_join(monitor, NULL));
    AZ(pthread_mutex_destroy(&mutex));
}

void
MON_StatsInit(void)
{
    AZ(pthread_mutex_init(&mutex, NULL));
}

void
MON_StatsUpdate(stats_update_t update, unsigned nchunks, unsigned nbytes)
{
    AZ(pthread_mutex_lock(&mutex));
    switch(update) {
        
    case STATS_SENT:
        sent++;
        bytes += nbytes;
        occ--;
        occ_chunk -= nchunks;
        break;
        
    case STATS_FAILED:
        failed++;
        occ--;
        occ_chunk -= nchunks;
        break;
        
    case STATS_RECONNECT:
        reconnects++;
        break;

    case STATS_OCCUPANCY:
        occ++;
        occ_chunk += nchunks;
        if (occ > occ_hi)
            occ_hi = occ;
        if (occ > occ_hi_this)
            occ_hi_this = occ;
        if (occ_chunk > occ_chunk_hi)
            occ_chunk_hi = occ_chunk;
        if (occ_chunk > occ_chunk_hi_this)
            occ_chunk_hi_this = occ_chunk;
        break;

    case STATS_RESTART:
        restarts++;
        break;
        
    default:
        /* Unreachable */
        AN(NULL);
    }
    AZ(pthread_mutex_unlock(&mutex));
}
