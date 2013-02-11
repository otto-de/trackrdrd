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

#include <string.h>

#include "minunit.h"

#include "../trackrdrd.h"
#include "vas.h"
#include "miniobj.h"
#include "libvarnish.h"

#define DEBUG 0
#define debug_print(fmt, ...) \
    do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while(0)

/* Automake exit code for "skipped" in make check */
#define EXIT_SKIPPED 77

#define NWORKERS 5

int tests_run = 0;
static char errmsg[BUFSIZ];

/* N.B.: Always run this test first */
static char
*test_worker_init(void)
{
    int err;
    const char *error;

    printf("... testing worker initialization\n");

    config.maxopen_scale = 10;
    config.maxdone_scale = 10;
    config.nworkers = NWORKERS;
    strcpy(config.mq_uri, "tcp://localhost:61616");
    strcpy(config.mq_qname, "lhoste/tracking/test");
    
    error = MQ_GlobalInit();
    sprintf(errmsg, "MQ_GlobalInit failed: %s", error);
    mu_assert(errmsg, error == NULL);
    
    err = WRK_Init();
    sprintf(errmsg, "WRK_Init: %s", strerror(err));
    mu_assert(errmsg, err == 0);

    AZ(LOG_Open("test_worker"));
    AZ(HASH_Init());
    AZ(DATA_Init());
    AZ(SPMCQ_Init());

    return NULL;
}

static char
*test_worker_run(void)
{
    dataentry *entry;
    hashentry *he;

    printf("... testing run of %d workers\n", NWORKERS);

    srand48(time(NULL));
    unsigned xid = (unsigned int) lrand48();

    WRK_Start();
    DATA_noMT_Register();
    for (int i = 0; i < 1024; i++) {
        entry = DATA_noMT_Get();
        CHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
        he = HASH_Insert(++xid, entry, TIM_mono());
        CHECK_OBJ_NOTNULL(he, HASH_MAGIC);
        entry->xid = xid;
        sprintf(entry->data, "XID=%d&foo=bar&baz=quux&record=%d", xid, i+1);
        entry->end = strlen(entry->data);
        entry->state = DATA_DONE;
        HASH_Submit(he);
    }
    
    WRK_Halt();
    WRK_Shutdown();

    AZ(MQ_GlobalShutdown());
    LOG_Close();

    return NULL;
}

static const char
*all_tests(void)
{
    mu_run_test(test_worker_init);
    mu_run_test(test_worker_run);
    return NULL;
}

TEST_RUNNER
