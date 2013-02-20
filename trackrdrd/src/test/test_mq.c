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

/* Automake exit code for "skipped" in make check */
#define EXIT_SKIPPED 77

int tests_run = 0;
static char errmsg[BUFSIZ];
void *worker;

/* N.B.: Always run the tests in this order */
static char
*test_global_init(void)
{
    const char *err;

    printf("... testing MQ global initialization\n");

    err = MQ_GlobalInit();
    sprintf(errmsg, "MQ_GlobalInit: %s", err);
    mu_assert(errmsg, err == NULL);

    return NULL;
}

static char
*test_init_connection(void)
{
    const char *err;

    printf("... testing MQ connection initialization\n");

    config.n_mq_uris = 1;
    config.mq_uri = (char **) malloc(sizeof(char **));
    AN(config.mq_uri);
    config.mq_uri[0] = (char *) malloc(strlen("tcp://localhost:61616") + 1);
    AN(config.mq_uri[0]);
    strcpy(config.mq_uri[0], "tcp://localhost:61616");
    
    strcpy(config.mq_qname, "lhoste/tracking/test");
    config.mq_pool_size = 1;
    err = MQ_InitConnections();
    if (err != NULL && strstr(err, "Connection refused") != NULL) {
        printf("Connection refused, ActiveMQ assumed not running\n");
        exit(EXIT_SKIPPED);
    }
    sprintf(errmsg, "MQ_InitConnections: %s", err);
    mu_assert(errmsg, err == NULL);

    return NULL;
}

static const char
*test_worker_init(void)
{
    const char *err;

    printf("... test worker init\n");

    err = MQ_WorkerInit(&worker);
    sprintf(errmsg, "MQ_WorkerInit: %s", err);
    mu_assert(errmsg, err == NULL);

    mu_assert("Worker is NULL after MQ_WorkerInit", worker != NULL);

    return NULL;
}

static const char
*test_version(void)
{
    const char *err;
    char version[BUFSIZ];

    printf("... testing version info\n");

    mu_assert("MQ_Version: worker is NULL before call", worker != NULL);
    err = MQ_Version(worker, version);
    sprintf(errmsg, "MQ_Version: %s", err);
    mu_assert(errmsg, err == NULL);
    mu_assert("MQ_Version: version is empty", version[0] != '\0');

    return NULL;
}

static const char
*test_clientID(void)
{
    const char *err;
    char clientID[BUFSIZ];

    printf("... testing client ID info\n");

    mu_assert("MQ_ClientID: worker is NULL before call", worker != NULL);
    err = MQ_ClientID(worker, clientID);
    sprintf(errmsg, "MQ_Version: %s", err);
    mu_assert(errmsg, err == NULL);
    mu_assert("MQ_Version: client ID is empty", clientID[0] != '\0');

    return NULL;
}

static const char
*test_send(void)
{
    const char *err;

    printf("... testing message send\n");

    mu_assert("MQ_Send: worker is NULL before call", worker != NULL);
    err = MQ_Send(worker, "foo bar baz quux", 16);
    sprintf(errmsg, "MQ_Send: %s", err);
    mu_assert(errmsg, err == NULL);

    return NULL;
}

static const char
*test_worker_shutdown(void)
{
    const char *err;

    printf("... testing worker shutdown\n");

    mu_assert("MQ_WorkerShutdown: worker is NULL before call", worker != NULL);
    err = MQ_WorkerShutdown(&worker);
    sprintf(errmsg, "MQ_WorkerShutdown: %s", err);
    mu_assert(errmsg, err == NULL);

    mu_assert("Worker not NULL after shutdown", worker == NULL);
    
    err = MQ_Send(worker, "foo bar baz quux", 16);
    mu_assert("No failure on MQ_Send after worker shutdown", err != NULL);

    return NULL;
}

static const char
*test_global_shutdown(void)
{
    const char *err;

    printf("... testing global shutdown\n");
    err = MQ_GlobalShutdown();
    sprintf(errmsg, "MQ_GlobalShutdown: %s", err);
    mu_assert(errmsg, err == NULL);

    return NULL;
}

static const char
*all_tests(void)
{
    mu_run_test(test_global_init);
    mu_run_test(test_init_connection);
    mu_run_test(test_worker_init);
    mu_run_test(test_version);
    mu_run_test(test_clientID);
    mu_run_test(test_send);
    mu_run_test(test_worker_shutdown);
    mu_run_test(test_global_shutdown);
    return NULL;
}

TEST_RUNNER
