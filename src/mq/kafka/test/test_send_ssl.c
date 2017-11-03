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

#include <stdio.h>
#include <string.h>

#include "mq.h"
#include "../../../test/minunit.h"

/* Automake exit code for "skipped" in make check */
#define EXIT_SKIPPED 77

#define KAFKA_CONFIG "kafka_ssl.conf"
#define NWORKERS 20
#define DATAMAX 8192
#define IDLEN 96
#define MAXLEN (DATAMAX + IDLEN + 1)

int tests_run = 0;
void *worker[NWORKERS];

static char
*test_send(void)
{
    const char *err;
    char *line;
    size_t n = MAXLEN;
    int wrk_num = 0, i, ret;

    printf("... testing message send from stdin\n");

    line = malloc(MAXLEN);
    MAN(line);
    
    err = MQ_GlobalInit(NWORKERS, KAFKA_CONFIG);
    VMASSERT(err == NULL, "MQ_GlobalInit: %s", err);

    err = MQ_InitConnections();
    if (err != NULL && strstr(err, "connection loss") != NULL) {
        printf("No connection, Kafka/Zookeeper assumed not running\n");
        exit(EXIT_SKIPPED);
    }
    VMASSERT(err == NULL, "MQ_InitConnections: %s", err);

    for (i = 0; i < NWORKERS; i++) {
        err = MQ_WorkerInit(&worker[i], i + 1);
        VMASSERT(err == NULL, "MQ_WorkerInit: %s", err);
        MASSERT0(worker[i] != NULL, "Worker is NULL after MQ_WorkerInit");
    }

    while (getline(&line, &n, stdin) > 1) {
        char *key, *data;

        key = strtok(line, ":");
        MAN(key);
        data = strtok(NULL, ":");
        MAN(data);

        VMASSERT(worker[wrk_num] != NULL,
                 "MQ_Send: worker %d is NULL before call", wrk_num + 1);
        ret = MQ_Send(worker[wrk_num], data, strlen(data) - 1, key, 8,
                      &err);
        VMASSERT(ret == 0, "MQ_Send: %s", err);
        wrk_num++;
        wrk_num %= NWORKERS;
        MASSERT(wrk_num < NWORKERS);
    }

    for (i = 0; i < NWORKERS; i++) {
        MASSERT0(worker[i] != NULL, "worker is NULL before shutdown");
        err = MQ_WorkerShutdown(&worker[i], i + 1);
        VMASSERT(err == NULL, "MQ_WorkerShutdown: %s", err);
        MASSERT0(worker[i] == NULL, "Worker not NULL after shutdown");
    }

    err = MQ_GlobalShutdown();
    VMASSERT(err == NULL, "MQ_GlobalShutdown: %s", err);

    return NULL;
}

static const char
*all_tests(void)
{
    mu_run_test(test_send);
    return NULL;
}

TEST_RUNNER
