/*-
 * Copyright (c) 2014 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2014 Otto Gmbh & Co KG
 * All rights reserved
 * Use only with permission
 *
 * Author: Geoffrey Simmons <geoffrey.simmons@uplex.de>
 *
 * Portions adapted from rdkafka_zookeeper_example.c from librdkafka
 * https://github.com/edenhill/librdkafka
 * Copyright (c) 2012, Magnus Edenhill
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

/*
 * Encapsulate interaction of the Kafka MQ plugin with Apache ZooKeeper
 * servers
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>

#include <zookeeper/zookeeper.h>
#include <pcre.h>

#include "mq_kafka.h"
#include "miniobj.h"

#define BROKER_PATH "/brokers/ids"

static zhandle_t *zh = NULL;
static pcre *host_regex = NULL, *port_regex = NULL;
static char errmsg[LINE_MAX];
static FILE *zoologf = NULL;

static const char
*setBrokerList(char *brokerlist, int max)
{
    struct String_vector brokers;
    int result;
    char *brokerptr = brokerlist;
    const char *pcre_err;

    AN(zh);

    if ((result = zoo_get_children(zh, BROKER_PATH, 1, &brokers)) != ZOK) {
        snprintf(errmsg, LINE_MAX, "Cannot get broker ids from zookeeper: %s",
                 zerror(result));
        return errmsg;
    }

    if (host_regex == NULL) {
        host_regex = pcre_compile("\"host\"\\s*:\\s*\"([^\"]+)\"", 0,
                                  &pcre_err, &result, NULL);
        AN(host_regex);
    }
    if (port_regex == NULL) {
        port_regex = pcre_compile("\"port\"\\s*:\\s*(\\d+)", 0,
                                  &pcre_err, &result, NULL);
        AN(port_regex);
    }

    memset(brokerlist, 0, max);
    for (int i = 0; i < brokers.count; i++) {
        char path[PATH_MAX], broker[LINE_MAX];
        int len = LINE_MAX;

        snprintf(path, PATH_MAX, "/brokers/ids/%s", brokers.data[i]);
        if ((result = zoo_get(zh, path, 0, broker, &len, NULL)) != ZOK) {
            snprintf(errmsg, LINE_MAX,
                     "Cannot get config for broker id %s: %s",
                     brokers.data[i], zerror(result));
            return errmsg;
        }
        if (len > 0) {
            int ovector[6], r;
            const char *host = NULL, *port = NULL;

            broker[len] = '\0';
            MQ_LOG_Log(LOG_DEBUG, "Zookeeper broker id %s config: %s",
                       brokers.data[i], broker);

            r = pcre_exec(host_regex, NULL, broker, len, 0, 0, ovector, 6);
            if (r <= PCRE_ERROR_NOMATCH) {
                snprintf(errmsg, LINE_MAX,
                         "Host not found in config for broker id %s [%s]",
                         brokers.data[i], broker);
                return errmsg;
            }
            pcre_get_substring(broker, ovector, r, 1, &host);
            AN(host);
            r = pcre_exec(port_regex, NULL, broker, len, 0, 0, ovector, 6);
            if (r <= PCRE_ERROR_NOMATCH) {
                snprintf(errmsg, LINE_MAX,
                         "Port not found in config for broker id %s [%s]",
                         brokers.data[i], broker);
                return errmsg;
            }
            pcre_get_substring(broker, ovector, r, 1, &port);
            AN(port);

            if (strlen(brokerlist) + strlen(host) + strlen(port) + 2 > max) {
                snprintf(errmsg, LINE_MAX,
                         "Broker list length exceeds max %d [%s%s:%s]",
                         max, brokerlist, host, port);
                return errmsg;
            }
            sprintf(brokerptr, "%s:%s", host, port);
            pcre_free_substring(host);
            pcre_free_substring(port);
            brokerptr += strlen(brokerptr);
            if (i < brokers.count - 1)
                *brokerptr++ = ',';
        }
        else
            MQ_LOG_Log(LOG_WARNING, "Empty config returned for broker id %s",
                       brokers.data[i]);
    }
    deallocate_String_vector(&brokers);
    return NULL;
}

/* cf. rdkafka_zookeeper_example.c */
static void
watcher(zhandle_t *zzh, int type, int state, const char *path, void *watcherCtx)
{
    char brokers[LINE_MAX] = "";
    (void) state;
    (void) watcherCtx;

    assert(zzh == zh);

    if (type == ZOO_CHILD_EVENT
        && strncmp(path, BROKER_PATH, sizeof(BROKER_PATH) - 1) == 0) {
        const char *err = setBrokerList(brokers, LINE_MAX);
        if (err != NULL) {
            MQ_LOG_Log(LOG_ERR, "Error obtaining broker list from watcher: %s",
                       err);
            return;
        }
        if (brokers[0] != '\0')
            /* XXX: encapsulate */
            for (int i = 0; i < nwrk; i++)
                if (workers[i] != NULL) {
                    int nbrokers;

                    CHECK_OBJ(workers[i], KAFKA_WRK_MAGIC);
                    nbrokers = rd_kafka_brokers_add(workers[i]->kafka, brokers);
                    /* XXX: poll timeout configurable? */
                    rd_kafka_poll(workers[i]->kafka, 10);
                    MQ_LOG_Log(LOG_INFO, "%s: added %d brokers [%s]",
                               rd_kafka_name(workers[i]->kafka), nbrokers,
                               brokers);
                }
    }
}

const char
*MQ_ZOO_Init(char *zooservers, unsigned timeout, char *brokerlist, int max)
{
    /* XXX: wait for ZOO_CONNECTED_STATE */
    errno = 0;
    zh = zookeeper_init(zooservers, watcher, timeout, 0, 0, 0);
    if (zh == NULL) {
        snprintf(errmsg, LINE_MAX, "init/connect failure: %s", strerror(errno));
        return errmsg;
    }
    return setBrokerList(brokerlist, max);
}

const char
*MQ_ZOO_SetLog(const char *path)
{
    AN(path);
    AN(path[0]);

    zoologf = fopen(path, "a");
    if (zoologf == NULL) {
        strncpy(errmsg, strerror(errno), LINE_MAX);
        return errmsg;
    }
    zoo_set_log_stream(zoologf);
    return NULL;
}

void
MQ_ZOO_SetLogLevel(int level)
{
    if (zh == NULL)
        return;

    if (zoologf != NULL)
        /* level must be a syslog level */
        switch(level) {
        case LOG_INFO:
        case LOG_NOTICE:
            zoo_set_debug_level(ZOO_LOG_LEVEL_INFO);
            break;
        case LOG_DEBUG:
            zoo_set_debug_level(ZOO_LOG_LEVEL_DEBUG);
            break;
        case LOG_WARNING:
            zoo_set_debug_level(ZOO_LOG_LEVEL_WARN);
            break;
        case LOG_ERR:
        case LOG_CRIT:
        case LOG_ALERT:
        case LOG_EMERG:
            zoo_set_debug_level(ZOO_LOG_LEVEL_ERROR);
            break;
        default:
            MQ_LOG_Log(LOG_ERR, "Unknown log level %d", level);
            AN(0);
            break;
        }
}

const char
*MQ_ZOO_Fini(void)
{
    int zerr;

    if (zh == NULL)
        return NULL;

    errno = 0;
    if ((zerr = zookeeper_close(zh)) != ZOK) {
        const char *err = zerror(zerr);
        if (zerr == ZSYSTEMERROR)
            snprintf(errmsg, LINE_MAX, "%s (%s)", err, strerror(errno));
        else
            strncpy(errmsg, err, LINE_MAX);
        return errmsg;
    }
    if (zoologf != NULL)
        fclose(zoologf);
    return NULL;
}
