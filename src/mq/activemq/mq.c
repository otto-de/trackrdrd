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
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "mq.h"
#include "config_common.h"
#include "amq.h"
#include "amq_connection.h"

static AMQ_Connection **connections;
static AMQ_Worker **workers;
static unsigned connection = 0;
static unsigned nwrk = 0;

static unsigned n_uris = 0;
static char **uri;
static char qname[BUFSIZ];

static int
conf_add(const char *lval, const char *rval)
{
    if (strcmp(lval, "mq.qname") == 0) {
        strcpy(qname, rval);
        return(0);
    }

    if (strcmp(lval, "mq.uri") == 0) {
        int n = n_uris++;
        uri = (char **) realloc(uri, n_uris * sizeof(char **));
        if (uri == NULL)
            return(errno);
        uri[n] = (char *) malloc(strlen(rval) + 1);
        if (uri[n] == NULL)
            return(errno);
        strcpy(uri[n], rval);
        return(0);
    }

    return EINVAL;
}

const char *
MQ_GlobalInit(unsigned nworkers, const char *config_fname)
{
    workers = (AMQ_Worker **) calloc(sizeof (AMQ_Worker *), nworkers);
    if (workers == NULL)
        return strerror(errno);
    nwrk = nworkers;

    uri = (char **) malloc (sizeof(char **));
    if (uri == NULL)
        return strerror(errno);
    if (CONF_ReadFile(config_fname, conf_add) != 0)
        return "Error reading config file for ActiveMQ";

    return AMQ_GlobalInit();
}

const char *
MQ_InitConnections(void)
{
    AMQ_Connection *conn;
    const char *err;

    if (n_uris == 0)
        return NULL;
    
    connections = (AMQ_Connection **) calloc(sizeof(AMQ_Connection *), nwrk);
    if (connections == NULL)
        return strerror(errno);
    for (int i = 0; i < nwrk; i++) {
        err = AMQ_ConnectionInit(&conn, uri[i % n_uris]);
        if (err != NULL)
            return err;
        connections[i] = conn;
    }
    return NULL;
}

const char *
MQ_WorkerInit(void **priv, int wrk_num)
{
    const char *err = NULL;
    
    assert(wrk_num >= 1 && wrk_num <= nwrk);
    wrk_num--;
    AMQ_Connection *conn = connections[wrk_num];
    if (conn == NULL) {
        err = AMQ_ConnectionInit(&conn, uri[wrk_num % n_uris]);
        if (err != NULL)
            return err;
        else
            connections[wrk_num] = conn;
    }
    err = AMQ_WorkerInit((AMQ_Worker **) priv, conn, qname, wrk_num);
    if (err == NULL)
        workers[wrk_num] = (AMQ_Worker *) *priv;
    else
        workers[wrk_num] = NULL;
    return err;
}

int
MQ_Send(void *priv, const char *data, unsigned len, const char *key,
        unsigned keylen, const char **error)
{
    /* The ActiveMQ implementation does not use sharding. */
    (void) key;
    (void) keylen;

    *error = AMQ_Send((AMQ_Worker *) priv, data, len);
    if (*error != NULL)
        return -1;
    return 0;
}

const char *
MQ_Reconnect(void **priv)
{
    const char *err;
    AMQ_Connection *conn;
    int wrk_num;

    err = AMQ_GetNum((AMQ_Worker *) priv, &wrk_num);
    if (err != NULL)
        return err;
    assert(wrk_num >= 0 && wrk_num < nwrk);
    err = AMQ_WorkerShutdown((AMQ_Worker **) priv);
    if (err != NULL)
        return err;
    else
        workers[wrk_num] = NULL;
    if (connections[wrk_num] != NULL) {
        err = AMQ_ConnectionShutdown(connections[wrk_num]);
        if (err != NULL)
            return err;
        connections[wrk_num] = NULL;
    }
    err = AMQ_ConnectionInit(&conn, uri[connection++ % n_uris]);
    if (err != NULL)
        return err;
    else
        connections[wrk_num] = conn;
    err = AMQ_WorkerInit((AMQ_Worker **) priv, conn, qname, wrk_num);
    if (err != NULL)
        workers[wrk_num] = NULL;
    else
        workers[wrk_num] = (AMQ_Worker *) *priv;
    return err;
}

const char *
MQ_Version(void *priv, char *version, size_t len)
{
    return AMQ_Version((AMQ_Worker *) priv, version, len);
}

const char *
MQ_ClientID(void *priv, char *clientID, size_t len)
{
    return AMQ_ClientID((AMQ_Worker *) priv, clientID, len);
}

const char *
MQ_WorkerShutdown(void **priv, int wrk_num)
{
    const char *err;

    wrk_num--;
    assert(wrk_num >= 0 && wrk_num < nwrk);
    if (connections[wrk_num] != NULL) {
        err = AMQ_ConnectionShutdown(connections[wrk_num]);
        if (err != NULL)
            return err;
        connections[wrk_num] = NULL;
    }
    if (workers[wrk_num] != (AMQ_Worker *) *priv)
        return "AMQ worker handle not found in worker table";
    AMQ_WorkerShutdown((AMQ_Worker **) priv);
    if (err != NULL)
        return err;
    *priv = NULL;
    workers[wrk_num] = NULL;
    return NULL;
}

const char *
MQ_GlobalShutdown(void)
{
    const char *err;

    if (n_uris > 0)
        free(uri);

    for (int i = 0; i < nwrk; i++)
        if (connections[i] != NULL
            && (err = AMQ_ConnectionShutdown(connections[i])) != NULL)
            return err;

    free(connections);
    free(workers);

    return AMQ_GlobalShutdown();
}
