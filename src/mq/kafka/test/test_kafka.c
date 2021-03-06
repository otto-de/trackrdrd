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

#include "mq.h"
#include "../../../test/minunit.h"

/* Automake exit code for "skipped" in make check */
#define EXIT_SKIPPED 77

#ifndef TESTDIR
#	define TESTDIR "./"
#endif

#define KAFKA_CONFIG "kafka.conf"
#define NWORKERS 1

int tests_run = 0;
void *worker;

/* N.B.: Always run the tests in this order */
static char
*test_global_init(void)
{
    const char *err;

    printf("... testing Kafka global initialization\n");

    err = MQ_GlobalInit(NWORKERS, TESTDIR KAFKA_CONFIG);
    VMASSERT(err == NULL, "MQ_GlobalInit: %s", err);

    return NULL;
}

static char
*test_init_connection(void)
{
    const char *err;

    printf("... testing Kafka connection initialization\n");

    err = MQ_InitConnections();
    if (err != NULL && strstr(err, "connection loss") != NULL) {
        printf("No connection, Kafka/Zookeeper assumed not running\n");
        exit(EXIT_SKIPPED);
    }
    VMASSERT(err == NULL, "MQ_InitConnections: %s", err);

    return NULL;
}

static const char
*test_worker_init(void)
{
    const char *err;

    printf("... testing Kafka worker init\n");

    err = MQ_WorkerInit(&worker, NWORKERS);
    VMASSERT(err == NULL, "MQ_WorkerInit: %s", err);

    MASSERT0(worker != NULL, "Worker is NULL after MQ_WorkerInit");

    return NULL;
}

static const char
*test_version(void)
{
    const char *err;
    char version[BUFSIZ];

    printf("... testing Kafka version info\n");

    MASSERT0(worker != NULL, "MQ_Version: worker is NULL before call");
    err = MQ_Version(worker, version, BUFSIZ);
    VMASSERT(err == NULL, "MQ_Version: %s", err);
    MASSERT0(version[0] != '\0', "MQ_Version: version is empty");

    return NULL;
}

static const char
*test_clientID(void)
{
    const char *err;
    char clientID[BUFSIZ];

    printf("... testing Kafka client ID info\n");

    MASSERT0(worker != NULL, "MQ_ClientID: worker is NULL before call");
    err = MQ_ClientID(worker, clientID, BUFSIZ);
    VMASSERT(err == NULL, "MQ_ClientID: %s", err);
    MASSERT0(clientID[0] != '\0', "MQ_ClientID: client ID is empty");

    return NULL;
}

static const char
*test_send(void)
{
    const char *err;
    int ret;

    printf("... testing Kafka message send\n");

    MASSERT0(worker != NULL, "MQ_Send: worker is NULL before call");
    ret = MQ_Send(worker, "foo bar baz quux", 16, "12345678", 8, &err);
    VMASSERT(ret == 0, "MQ_Send: %s", err);

    /* Keys shorter and longer than 8 hex digits */
    ret = MQ_Send(worker, "the quick brown fox", 19, "abcdef", 6, &err);
    VMASSERT(ret == 0, "MQ_Send: %s", err);
    ret = MQ_Send(worker, "jumps over the lazy dog", 23,
                  "fedcba9876543210", 16, &err);
    VMASSERT(ret == 0, "MQ_Send: %s", err);

    /* No error if message is empty (silently discarded) */
    ret = MQ_Send(worker, "", 0, "12345678", 8, &err);
    VMASSERT(ret == 0, "MQ_Send: %s", err);

    /* Non-recoverable error if the worker object is null */
    ret = MQ_Send(NULL, "foo bar baz quux", 16, "12345678", 8, &err);
    MASSERT(ret < 0);

    /* Recoverable error if the key is empty */
    ret = MQ_Send(worker, "foo bar baz quux", 16, "", 0 ,&err);
    MASSERT(ret > 0);
    VMASSERT(strstr("shard key is missing", err) == 0,
             "MQ_Send unexpected error message: %s", err);

    /* Recoverable error if the key is NULL */
    ret = MQ_Send(worker, "foo bar baz quux", 16, NULL, 0, &err);
    MASSERT(ret > 0);
    VMASSERT(strstr("shard key is missing", err) == 0,
             "MQ_Send unexpected error message: %s", err);

    /* Recoverable error if the key contains non-hex characters */
    ret = MQ_Send(worker, "foo bar baz quux", 16, "key", 3, &err);
    MASSERT(ret > 0);
    VMASSERT(strstr("shard key is not hex", err) == 0,
             "MQ_Send unexpected error message: %s", err);

    /* Recoverable error if the message is NULL */
    ret = MQ_Send(worker, NULL, 16, "12345678", 8, &err);
    MASSERT(ret > 0);
    VMASSERT(strstr("message payload is NULL", err) == 0,
             "MQ_Send unexpected error message: %s", err);

    return NULL;
}

static const char
*test_reconnect(void)
{
    const char *err;
    int ret;

    printf("... testing Kafka reconnect\n");

    MASSERT0(worker != NULL, "MQ_Reconnect: worker is NULL before call");
    err = MQ_Reconnect(&worker);
    VMASSERT(err == NULL, "MQ_Reconnect: %s", err);
    MASSERT0(worker != NULL, "MQ_Reconnect: worker is NULL after call");
    ret = MQ_Send(worker, "send after reconnect", 20, "12345678", 8, &err);
    VMASSERT(ret == 0, "MQ_Send() fails after reconnect: %s", err);

    return NULL;
}

static const char
*test_worker_shutdown(void)
{
    const char *err;
    int ret;

    printf("... testing Kafka worker shutdown\n");

    MASSERT0(worker != NULL, "MQ_Send: worker is NULL before call");
    err = MQ_WorkerShutdown(&worker, NWORKERS);
    VMASSERT(err == NULL, "MQ_WorkerShutdown: %s", err);

    MASSERT0(worker == NULL, "Worker not NULL after shutdown");
    
    ret = MQ_Send(worker, "foo bar baz quux", 16, "12345678", 8, &err);
    MASSERT0(ret != 0, "No failure on MQ_Send after worker shutdown");

    return NULL;
}

static const char
*test_global_shutdown(void)
{
    const char *err;

    printf("... testing Kafka global shutdown\n");

    err = MQ_GlobalShutdown();
    VMASSERT(err == NULL, "MQ_GlobalShutdown: %s", err);

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
    mu_run_test(test_reconnect);
    mu_run_test(test_worker_shutdown);
    mu_run_test(test_global_shutdown);
    return NULL;
}

TEST_RUNNER
