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
#include "vas.h"

static int run;

static pthread_mutex_t	mutex;
static unsigned		occ;
static unsigned		sent;		/* Sent successfully to MQ */
static unsigned		failed;		/* MQ send fails */
static unsigned		reconnects;	/* Reconnects to MQ */
static unsigned		restarts;	/* Worker thread restarts */
static unsigned		occ_hi;		/* Occupancy high water mark */ 
static unsigned		occ_hi_this;	/* Occupancy high water mark
                                           this reporting interval*/

static void
log_output(void)
{
    int wrk_running = WRK_Running();

    LOG_Log(LOG_INFO, "Data table: len=%u occ=%u occ_hi=%u occ_hi_this=%u "
            "global_free=%u",
            dtbl.len, occ, occ_hi, occ_hi_this, dtbl.nfree);

    /* Eliminate the dependency of trackrdrd.o for unit tests */
#ifndef TEST_DRIVER
    RDR_Stats();
#endif
    
    if (wrk_running < config.nworkers)
        LOG_Log(LOG_WARNING, "%d of %d workers running", wrk_running,
                config.nworkers);
    /* XXX: seen, bytes sent */
    LOG_Log(LOG_INFO, "Workers: running=%d sent=%lu failed=%u reconnects=%u "
            "restarts=%u abandoned=%u",
            wrk_running, sent, failed, reconnects, restarts, failed, abandoned);

    /* locking would be overkill */
    occ_hi_this = 0;

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
MON_StatsUpdate(stats_update_t update)
{
    AZ(pthread_mutex_lock(&mutex));
    switch(update) {
        
    case STATS_SENT:
        sent++;
        occ--;
        break;
        
    case STATS_FAILED:
        failed++;
        occ--;
        break;
        
    case STATS_RECONNECT:
        reconnects++;
        break;

    case STATS_OCCUPANCY:
        occ++;
        if (occ > occ_hi)
            occ_hi = occ;
        if (occ > occ_hi_this)
            occ_hi_this = occ;
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
