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

#include <syslog.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "trackrdrd.h"
#include "vas.h"

static int run;

static void
log_output(void)
{

    /* Eliminate the dependency of trackrdrd.o for unit tests */
#ifndef TEST_DRIVER
    HASH_Stats();
#endif
    
    LOG_Log(LOG_INFO,
        "Data table: "
        "len=%u "
        "nodata=%u "
        "submitted=%u "
        "wait_room=%u "
        "data_hi=%u "
        "data_overflows=%u "
        "done=%u "
        "open=%u "
        "load=%.2f "
        "sent=%u "
        "reconnects=%u "
        "failed=%u "
        "occ_hi=%u "
        "occ_hi_this=%u ",
        dtbl.len,
        dtbl.w_stats.nodata,
        dtbl.w_stats.submitted,
        dtbl.w_stats.wait_room,
        dtbl.w_stats.data_hi,
        dtbl.w_stats.data_overflows,
        dtbl.r_stats.done,
        dtbl.r_stats.open,
        (100.0 * (1.0 * dtbl.r_stats.done + 1.0 * dtbl.r_stats.open) / dtbl.len),
        dtbl.r_stats.sent,
        dtbl.r_stats.reconnects,
        dtbl.r_stats.failed,
        dtbl.r_stats.occ_hi,
        dtbl.r_stats.occ_hi_this
            );

    /* locking would be overkill */
    dtbl.r_stats.occ_hi_this = 0;

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
                LOG_Log0(LOG_INFO, "Monitoring thread interrupted");
                continue;
            }
            else {
                LOG_Log(LOG_WARNING, "Monitoring thread: %s\n",
                        strerror(errno));
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
    AZ(pthread_mutex_destroy(&dtbl.r_stats.mutex));
}

void
MON_StatsInit(void)
{
    AZ(pthread_mutex_init(&dtbl.r_stats.mutex, &attr_lock));
}

void
MON_StatsUpdate(stats_update_t update)
{
    AZ(pthread_mutex_lock(&dtbl.r_stats.mutex));
    switch(update) {
        
    case STATS_SENT:
        dtbl.r_stats.sent++;
        dtbl.r_stats.done--;
        break;
        
    case STATS_FAILED:
        dtbl.r_stats.failed++;
        dtbl.r_stats.done--;
        break;
        
    case STATS_DONE:
        dtbl.r_stats.done++;
        dtbl.r_stats.open--;
        break;

    case STATS_RECONNECT:
        dtbl.r_stats.reconnects++;
        break;

    case STATS_OCCUPANCY:
        dtbl.r_stats.open++;
        if (dtbl.r_stats.open + dtbl.r_stats.done > dtbl.r_stats.occ_hi)
            dtbl.r_stats.occ_hi = dtbl.r_stats.open + dtbl.r_stats.done;
        if (dtbl.r_stats.open + dtbl.r_stats.done > dtbl.r_stats.occ_hi_this)
            dtbl.r_stats.occ_hi_this = dtbl.r_stats.open + dtbl.r_stats.done;
        break;

    case STATS_NODATA:
        dtbl.w_stats.nodata++;
        dtbl.r_stats.done--;
        break;
        
    default:
        /* Unreachable */
        AN(NULL);
    }
    AZ(pthread_mutex_unlock(&dtbl.r_stats.mutex));
}
