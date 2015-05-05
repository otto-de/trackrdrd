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
#include <dlfcn.h>

#include "minunit.h"

#include "../trackrdrd.h"
#include "vas.h"

#define MQ_MODULE "../mq/file/.libs/libtrackrdr-file.so"
#define MQ_CONFIG "file_mq.conf"

#define NWORKERS 1

int tests_run = 0;
static char errmsg[BUFSIZ];
static void *mqh;
void *worker;

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

/* N.B.: Always run the tests in this order */
static char
*test_global_init(void)
{
    const char *err;

    printf("... testing MQ global initialization\n");

    config.nworkers = NWORKERS;
    strcpy(config.mq_config_file, MQ_CONFIG);
    err = mqf.global_init(config.nworkers, config.mq_config_file);
    VMASSERT(err == NULL, "MQ_GlobalInit: %s", err);

    return NULL;
}

static char
*test_init_connection(void)
{
    const char *err;

    printf("... testing MQ connection initialization\n");

    err = mqf.init_connections();
    VMASSERT(err == NULL, "MQ_InitConnections: %s", err);

    return NULL;
}

static const char
*test_worker_init(void)
{
    const char *err;

    printf("... test worker init\n");

    err = mqf.worker_init(&worker, NWORKERS);
    VMASSERT(err == NULL, "MQ_WorkerInit: %s", err);
    MASSERT0(worker != NULL, "Worker is NULL after MQ_WorkerInit");

    return NULL;
}

static const char
*test_version(void)
{
    const char *err;
    char version[BUFSIZ];

    printf("... testing version info\n");

    MASSERT0(worker != NULL, "MQ_Version: worker is NULL before call");
    err = mqf.version(worker, version, BUFSIZ);
    VMASSERT(err == NULL, "MQ_Version: %s", err);
    MASSERT0(version[0] != '\0', "MQ_Version: version is empty");

    return NULL;
}

static const char
*test_clientID(void)
{
    const char *err;
    char clientID[BUFSIZ];

    printf("... testing client ID info\n");

    MASSERT0(worker != NULL, "MQ_ClientID: worker is NULL before call");
    err = mqf.client_id(worker, clientID, BUFSIZ);
    VMASSERT(err == NULL, "MQ_ClientID: %s", err);
    MASSERT0(clientID[0] != '\0', "MQ_ClientID: client ID is empty");

    return NULL;
}

static const char
*test_send(void)
{
    const char *err;
    int ret;

    printf("... testing message send\n");

    MASSERT0(worker != NULL, "MQ_Send: worker is NULL before call");
    ret = mqf.send(worker, "foo bar baz quux", 16, "key", 3, &err);
    VMASSERT(ret == 0, "MQ_Send: %s", err);

    return NULL;
}

static const char
*test_reconnect(void)
{
    const char *err;
    int ret;

    printf("... testing MQ reconnect\n");

    MASSERT0(worker != NULL, "MQ_Reconnect: worker is NULL before call");
    err = mqf.reconnect(&worker);
    VMASSERT(err == NULL, "MQ_Reconnect: %s", err);
    MASSERT0(worker != NULL, "MQ_Reconnect: worker is NULL after call");
    ret = mqf.send(worker, "send after reconnect", 20, "key", 3, &err);
    VMASSERT(ret == 0, "MQ_Send() fails after reconnect: %s", err);

    return NULL;
}

static const char
*test_worker_shutdown(void)
{
    const char *err;
    int ret;

    printf("... testing worker shutdown\n");

    mu_assert("MQ_WorkerShutdown: worker is NULL before call", worker != NULL);
    err = mqf.worker_shutdown(&worker, NWORKERS);
    sprintf(errmsg, "MQ_WorkerShutdown: %s", err);
    mu_assert(errmsg, err == NULL);

    mu_assert("Worker not NULL after shutdown", worker == NULL);
    
    ret = mqf.send(worker, "foo bar baz quux", 16, "key", 3, &err);
    mu_assert("No failure on MQ_Send after worker shutdown", ret != 0);

    return NULL;
}

static const char
*test_global_shutdown(void)
{
    const char *err;

    printf("... testing global shutdown\n");
    err = mqf.global_shutdown();
    sprintf(errmsg, "MQ_GlobalShutdown: %s", err);
    mu_assert(errmsg, err == NULL);

    return NULL;
}

static const char
*all_tests(void)
{
    init();
    mu_run_test(test_global_init);
    mu_run_test(test_init_connection);
    mu_run_test(test_worker_init);
    mu_run_test(test_version);
    mu_run_test(test_clientID);
    mu_run_test(test_send);
    mu_run_test(test_reconnect);
    mu_run_test(test_worker_shutdown);
    mu_run_test(test_global_shutdown);
    fini();
    return NULL;
}

TEST_RUNNER
