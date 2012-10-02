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

void
*MON_StatusThread(void *arg)
{
    struct timespec t;
    double *interval = (double *) arg;

    t.tv_sec = (time_t) *interval;
    t.tv_nsec = (long)(t.tv_sec - *interval) * 10e9;
    LOG_Log(LOG_INFO, "Monitor thread running every %.2f secs",
            t.tv_sec + ((float) t.tv_nsec * 10e-9));
    
    while (1) {
        int err;
        if (nanosleep(&t, NULL) != 0) {
            if (errno == EINTR) {
                /* XXX: Terminate on TERM, INT, etc. */
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
        LOG_Log(LOG_INFO,
            "Data table: len=%d collisions=%d insert_probes=%d find_probes=%d "
            "open=%d done=%d load=%.2f occ_hi=%d seen=%d submitted=%d data_hi=%d",
            tbl.len, tbl.collisions, tbl.insert_probes, tbl.find_probes,
            tbl.open, tbl.done, 100.0 * ((float) tbl.open + tbl.done) / tbl.len,
            tbl.occ_hi, tbl.seen, tbl.submitted, tbl.data_hi);
    }
}