/*-
 * Copyright (c) 2014 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2014 Otto Gmbh & Co KG
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

#include "mq_kafka.h"
#include "miniobj.h"

static pthread_t monitor;
static int run = 0;
static unsigned seen, produced, delivered, failed, nokey, badkey, nodata;

/* Call rd_kafka_poll() for each worker to provoke callbacks */
static void
poll_workers(void)
{
    for (int i = 0; i < nwrk; i++)
        if (workers[i] != NULL) {
            kafka_wrk_t *wrk = workers[i];
            CHECK_OBJ(wrk, KAFKA_WRK_MAGIC);
            rd_kafka_poll(wrk->kafka, 0);
            seen += wrk->seen;
            produced += wrk->produced;
            delivered += wrk->delivered;
            failed += wrk->failed;
            nokey += wrk->nokey;
            badkey += wrk->badkey;
            nodata += wrk->nodata;
        }
}

static void
monitor_cleanup(void *arg)
{
    (void) arg;

    poll_workers();
    MQ_LOG_Log(LOG_INFO, "libtrackrdr-kafka monitoring thread exiting");
}

static void
*monitor_thread(void *arg)
{
    struct timespec t;
    unsigned interval = *((unsigned *) arg);

    /* Convert ms -> struct timespec */
    t.tv_sec = (time_t) interval / 1e3;
    t.tv_nsec = (interval % (unsigned) 1e3) * 1e6;
    MQ_LOG_Log(LOG_INFO,
               "libtrackrdr-kafka monitor thread running every %u.%03lu secs",
               t.tv_sec, t.tv_nsec / (unsigned long) 1e6);
    run = 1;

    pthread_cleanup_push(monitor_cleanup, arg);

    while (run) {
        int err;
        if (nanosleep(&t, NULL) != 0) {
            if (errno == EINTR) {
                if (run == 0)
                    break;
                MQ_LOG_Log(LOG_INFO,
                           "libtrackrdr-kafka monitoring thread interrupted");
                continue;
            }
            else {
                MQ_LOG_Log(LOG_ERR, "libtrackrdr-kafka monitoring thread: %s",
                           strerror(errno));
                err = errno;
                pthread_exit(&err);
            }
        }
        seen = produced = delivered = failed = nokey = badkey = nodata = 0;
        poll_workers();
        MQ_LOG_Log(LOG_INFO, "mq stats summary: seen=%u produced=%u "
                   "delivered=%u failed=%u nokey=%u badkey=%u nodata=%u",
                   seen, produced, delivered, failed, nokey, badkey, nodata);
    }

    pthread_cleanup_pop(0);
    MQ_LOG_Log(LOG_INFO, "libtrackrdr-kafka monitoring thread exiting");
    pthread_exit((void *) NULL);
}

int
MQ_MON_Init(unsigned interval)
{
    if (interval == 0)
        return 0;
    return pthread_create(&monitor, NULL, monitor_thread, (void *) &interval);
}

void
MQ_MON_Fini(void)
{
    if (run) {
        run = 0;
        AZ(pthread_cancel(monitor));
        /* XXX: read and return an error status */
        AZ(pthread_join(monitor, NULL));
    }
}
