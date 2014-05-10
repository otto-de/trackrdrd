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
#include "vas.h"
#include "miniobj.h"
#include "libvarnish.h"

#define DEBUG 0
#define debug_print(fmt, ...) \
    do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while(0)

/* Automake exit code for "skipped" in make check */
#define EXIT_SKIPPED 77

#define NWORKERS 5

#define MQ_MODULE "../mq/activemq/.libs/libtrackrdr-activemq.so"
#define MQ_CONFIG "activemq2.conf"

int tests_run = 0;
static char errmsg[BUFSIZ];
static void *mqh;

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

    config.maxopen_scale = 10;
    config.maxdone = 1024;
    config.maxdata = 1024;
    config.nworkers = NWORKERS;
    strcpy(config.mq_config_file, MQ_CONFIG);

    error = mqf.global_init(config.nworkers, config.mq_config_file);
    sprintf(errmsg, "MQ_GlobalInit failed: %s", error);
    mu_assert(errmsg, error == NULL);
    
    error = mqf.init_connections();
    if (error != NULL && strstr(error, "Connection refused") != NULL) {
        printf("Connection refused, ActiveMQ assumed not running\n");
        exit(EXIT_SKIPPED);
    }
    sprintf(errmsg, "MQ_InitConnections failed: %s", error);
    mu_assert(errmsg, error == NULL);
    
    err = WRK_Init();
    sprintf(errmsg, "WRK_Init: %s", strerror(err));
    mu_assert(errmsg, err == 0);

    AZ(LOG_Open("test_worker"));
    AZ(DATA_Init());
    AZ(SPMCQ_Init());

    return NULL;
}

static const char
*test_worker_run(void)
{
    dataentry *entry;

    printf("... testing run of %d workers\n", NWORKERS);

    srand48(time(NULL));
    unsigned xid = (unsigned int) lrand48();

    WRK_Start();
    int wrk_running, wrk_wait = 0;
    while ((wrk_running = WRK_Running()) < NWORKERS) {
        if (wrk_wait++ > 10)
            break;
        TIM_sleep(1);
    }
    sprintf(errmsg, "%d of %d worker threads running", wrk_running, NWORKERS);
    mu_assert(errmsg, wrk_running == NWORKERS);

    for (int i = 0; i < 1024; i++) {
        entry = &dtbl.entry[i];
        CHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
        entry->xid = xid;
        sprintf(entry->data, "XID=%d&foo=bar&baz=quux&record=%d", xid, i+1);
        entry->end = strlen(entry->data);
        entry->state = DATA_DONE;
        SPMCQ_Enq(entry);
    }
    
    WRK_Halt();
    WRK_Shutdown();

    AZ(mqf.global_shutdown());
    LOG_Close();

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
