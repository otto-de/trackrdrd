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
 */

#include <string.h>
#include <stdbool.h>
#include <dlfcn.h>

#include "minunit.h"

#include "../trackrdrd.h"
#include "../data.h"
#include "../vtim.h"

#define DEBUG 0
#define debug_print(fmt, ...) \
    do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while(0)

#define NWORKERS 5

#define MQ_MODULE "../mq/file/.libs/libtrackrdr-file.so"
#define MQ_CONFIG "file_mq.conf"

int tests_run = 0;
static void *mqh;

/* Called from worker.c, but we don't want to pull in all of monitor.c's
   dependecies. */
void
MON_StatsUpdate(stats_update_t update, unsigned n)
{
    (void) update;
    (void) n;
}

static void
init(void)
{
    char *err;

    dlerror(); // to clear errors
    mqh = dlopen(MQ_MODULE, RTLD_NOW);
    if ((err = dlerror()) != NULL) {
        fprintf(stderr, "error reading mq module %s: %s", MQ_MODULE, err);
        exit(EXIT_FAILURE);
    }

#define METHOD(instm, intfm)                                            \
    mqf.instm = dlsym(mqh, #intfm);                                     \
    if ((err = dlerror()) != NULL) {                                    \
        fprintf(stderr, "error loading mq method %s: %s", #intfm, err); \
        exit(EXIT_FAILURE);                                             \
    }
#include "../methods.h"
#undef METHOD
}

static void
fini(void)
{
    if (dlclose(mqh) != 0) {
        fprintf(stderr, "Error closing mq module %s: %s", MQ_MODULE, dlerror());
        exit(EXIT_FAILURE);
    }
}

/* N.B.: Always run this test first */
static char
*test_worker_init(void)
{
    int err;
    const char *error;

    printf("... testing worker initialization\n");

    config.max_records = DEF_MAX_RECORDS;
    config.max_reclen = DEF_MAX_RECLEN;
    config.maxkeylen = DEF_MAXKEYLEN;
    config.nworkers = NWORKERS;
    strcpy(config.mq_config_file, MQ_CONFIG);

    error = mqf.global_init(config.nworkers, config.mq_config_file);
    VMASSERT(error == NULL, "MQ_GlobalInit failed: %s", error);
    
    error = mqf.init_connections();
    VMASSERT(error == NULL, "MQ_InitConnections failed: %s", error);
    
    err = WRK_Init();
    VMASSERT(err == 0, "WRK_Init: %s", strerror(err));

    MAZ(LOG_Open("test_worker"));
    MAZ(DATA_Init());
    MAZ(SPMCQ_Init());

    return NULL;
}

static const char
*test_worker_run(void)
{
    dataentry *entry;

    printf("... testing run of %d workers\n", NWORKERS);

    WRK_Start();
    int wrk_running, wrk_wait = 0;
    while ((wrk_running = WRK_Running()) < NWORKERS) {
        if (wrk_wait++ > 10)
            break;
        VTIM_sleep(1);
    }
    VMASSERT(wrk_running == NWORKERS,
             "%d of %d worker threads running", wrk_running, NWORKERS);

    for (int i = 0; i < config.max_records; i++) {
        entry = &entrytbl[i];
        MCHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
        sprintf(entry->data, "foo=bar&baz=quux&record=%d", i+1);
        entry->end = strlen(entry->data);
        entry->occupied = 1;
        SPMCQ_Enq(entry);
    }
    
    WRK_Halt();
    WRK_Shutdown();

    MAZ(mqf.global_shutdown());
    LOG_Close();

    /*
     * Verify DATA_Reset() by checking that all data entry fields are in
     * empty states after worker threads are shut down.
     */
    for (int i = 0; i < config.max_records; i++) {
        entry = &entrytbl[i];
        MCHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
        MASSERT(!OCCUPIED(entry));
        MAZ(entry->end);
        MAZ(*entry->data);
        MAZ(entry->keylen);
        MAZ(*entry->key);
        MAZ(entry->hasdata);
        MAZ(entry->reqend_t.tv_sec);
        MAZ(entry->reqend_t.tv_usec);
    }

    return NULL;
}

static const char
*all_tests(void)
{
    init();
    mu_run_test(test_worker_init);
    mu_run_test(test_worker_run);
    fini();
    return NULL;
}

TEST_RUNNER
