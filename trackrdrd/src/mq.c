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

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "trackrdrd.h"
#include "activemq/amq.h"
#include "activemq/amq_connection.h"
#include "vas.h"

static AMQ_Connection **connections;
static unsigned nconnections;
static pthread_mutex_t connection_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned connection = 0;

const char *
MQ_GlobalInit(void)
{
    return AMQ_GlobalInit();
}

const char *
MQ_InitConnections(void)
{
    AMQ_Connection *conn;
    const char *err;

    if (config.n_mq_uris == 0)
        return NULL;
    
    nconnections = config.n_mq_uris * config.mq_pool_size;
    connections = (AMQ_Connection **) calloc(sizeof(AMQ_Connection *),
                                             nconnections);
    if (connections == NULL)
        return strerror(errno);
    for (int i = 0; i < config.n_mq_uris; i++)
        for (int j = 0; j < config.mq_pool_size; j++) {
            err = AMQ_ConnectionInit(&conn, config.mq_uri[i]);
            if (err != NULL)
                return err;
            connections[i*config.mq_pool_size + j] = conn;
        }
    return NULL;
}

const char *
MQ_WorkerInit(void **priv)
{
    AN(nconnections);
    AZ(pthread_mutex_lock(&connection_lock));
    AMQ_Connection *conn = connections[connection++ % nconnections];
    AZ(pthread_mutex_unlock(&connection_lock));
    return AMQ_WorkerInit((AMQ_Worker **) priv, conn, config.mq_qname);
}

const char *
MQ_Send(void *priv, const char *data, unsigned len)
{
    return AMQ_Send((AMQ_Worker *) priv, data, len);
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

    for (int i; i < config.n_mq_uris; i++)
        if ((err = AMQ_ConnectionShutdown(connections[i])) != NULL)
            return err;
    return AMQ_GlobalShutdown();
}

