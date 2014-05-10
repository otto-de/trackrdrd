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
#include <pthread.h>
#include <assert.h>

#include "mq.h"
#include "config_common.h"
#include "amq.h"
#include "amq_connection.h"

static AMQ_Connection **connections;
static AMQ_Worker **workers;
static pthread_mutex_t connection_lock = PTHREAD_MUTEX_INITIALIZER;
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
MQ_WorkerInit(void **priv)
{
    int i, ret;
    const char *err = NULL;
    
    ret = pthread_mutex_lock(&connection_lock);
    assert(ret == 0);
    i = connection++ % nwrk;
    ret = pthread_mutex_unlock(&connection_lock);
    assert(ret == 0);
    AMQ_Connection *conn = connections[i];
    if (conn == NULL)
        err = AMQ_ConnectionInit(&conn, uri[i % n_uris]);
    if (err != NULL)
        return err;
    connections[i] = conn;
    err = AMQ_WorkerInit((AMQ_Worker **) priv, conn, qname);
    if (err == NULL)
        workers[i] = (AMQ_Worker *) *priv;
    return err;
}

const char *
MQ_Send(void *priv, const char *data, unsigned len)
{
    return AMQ_Send((AMQ_Worker *) priv, data, len);
}

const char *
MQ_Reconnect(void **priv)
{
    const char *err;
    AMQ_Connection *conn;
    int wrk_num;

    err = AMQ_WorkerShutdown((AMQ_Worker **) priv);
    if (err != NULL)
        return err;
    for (int i = 0; i < nwrk; i++)
        if (workers[i] == (AMQ_Worker *) *priv) {
            wrk_num = i;
            break;
        }
    err = AMQ_ConnectionInit(&conn,
                             uri[connection++ % n_uris]);
    if (err != NULL) {
        connections[wrk_num] = NULL;
        return err;
    }
    else
        connections[wrk_num] = conn;
    return AMQ_WorkerInit((AMQ_Worker **) priv, conn, qname);
}

const char *
MQ_Version(void *priv, char *version)
{
    return AMQ_Version((AMQ_Worker *) priv, version);
}

const char *
MQ_ClientID(void *priv, char *clientID)
{
    return AMQ_ClientID((AMQ_Worker *) priv, clientID);
}

const char *
MQ_WorkerShutdown(void **priv)
{
    const char *err = AMQ_WorkerShutdown((AMQ_Worker **) priv);
    if (err != NULL)
        return err;
    *priv = NULL;
    return NULL;
}

const char *
MQ_GlobalShutdown(void)
{
    const char *err;

    for (int i; i < n_uris; i++)
        if (connections[i] != NULL
            && (err = AMQ_ConnectionShutdown(connections[i])) != NULL)
            return err;
    return AMQ_GlobalShutdown();
}
