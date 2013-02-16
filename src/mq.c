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

#include "trackrdrd.h"
#include "activemq/amq.h"

const char *
MQ_GlobalInit(void)
{
    return AMQ_GlobalInit(config.mq_uri);
}

const char *
MQ_WorkerInit(void **priv)
{
    return AMQ_WorkerInit((AMQ_Worker **) priv, config.mq_qname);
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
    return AMQ_GlobalShutdown();
}

