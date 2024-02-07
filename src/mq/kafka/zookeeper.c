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

#define PCRE2_CODE_UNIT_WIDTH 8

#include <zookeeper/zookeeper.h>
#include <pcre2.h>

#include "mq_kafka.h"
#include "miniobj.h"

#define BROKER_PATH "/brokers/ids"

static zhandle_t *zh = NULL;
static pcre2_code *host_regex = NULL, *port_regex = NULL;
static pcre2_match_data *host_data = NULL, *port_data = NULL;
static pcre2_match_context *mctx = NULL;
static char errmsg[LINE_MAX];
static FILE *zoologf = NULL;

static const char
*setBrokerList(char *brokers, int max)
{
    struct String_vector broker_ids;
    int result;
    PCRE2_SIZE offset;
    char *brokerptr = brokers;

    AN(zh);

    if ((result = zoo_get_children(zh, BROKER_PATH, 1, &broker_ids)) != ZOK) {
        snprintf(errmsg, LINE_MAX, "Cannot get broker ids from zookeeper: %s",
                 zerror(result));
        return errmsg;
    }

    if (host_regex == NULL) {
        host_regex = pcre2_compile("\"host\"\\s*:\\s*\"([^\"]+)\"",
                                   PCRE2_ZERO_TERMINATED, 0, &result, &offset,
                                   NULL);
        AN(host_regex);
        host_data = pcre2_match_data_create_from_pattern(host_regex, NULL);
        AN(host_data);
    }
    if (port_regex == NULL) {
        port_regex = pcre2_compile("\"port\"\\s*:\\s*(\\d+)",
                                   PCRE2_ZERO_TERMINATED, 0, &result, &offset,
                                   NULL);
        AN(port_regex);
        port_data = pcre2_match_data_create_from_pattern(port_regex, NULL);
        AN(port_data);
    }
    if (mctx == NULL) {
        mctx = pcre2_match_context_create(NULL);
        AN(mctx);
    }

    memset(brokers, 0, max);
    for (int i = 0; i < broker_ids.count; i++) {
        char path[PATH_MAX], broker[LINE_MAX];
        int len = LINE_MAX;
        PCRE2_SIZE ll;

        snprintf(path, PATH_MAX, "/brokers/ids/%s", broker_ids.data[i]);
        if ((result = zoo_get(zh, path, 0, broker, &len, NULL)) != ZOK) {
            snprintf(errmsg, LINE_MAX,
                     "Cannot get config for broker id %s: %s",
                     broker_ids.data[i], zerror(result));
            return errmsg;
        }
        if (len > 0) {
            int r;
            PCRE2_UCHAR *host = NULL, *port = NULL;

            broker[len] = '\0';
            MQ_LOG_Log(LOG_DEBUG, "Zookeeper broker id %s config: %s",
                       broker_ids.data[i], broker);

            r = pcre2_match(host_regex, broker, len, 0, 0, host_data, mctx);
            if (r <= PCRE2_ERROR_NOMATCH) {
                snprintf(errmsg, LINE_MAX,
                         "Host not found in config for broker id %s [%s]",
                         broker_ids.data[i], broker);
                return errmsg;
            }
            (void)pcre2_substring_get_bynumber(host_data, 1, &host, &ll);
            AN(host);
            AN(ll);
            r = pcre2_match(port_regex, broker, len, 0, 0, port_data, mctx);
            if (r <= PCRE2_ERROR_NOMATCH) {
                snprintf(errmsg, LINE_MAX,
                         "Port not found in config for broker id %s [%s]",
                         broker_ids.data[i], broker);
                return errmsg;
            }
            (void)pcre2_substring_get_bynumber(port_data, 1, &port, &ll);
            AN(port);
            AN(ll);

            if (strlen(brokers) + strlen(host) + strlen(port) + 2 > max) {
                snprintf(errmsg, LINE_MAX,
                         "Broker list length exceeds max %d [%s%s:%s]",
                         max, brokers, host, port);
                return errmsg;
            }
            sprintf(brokerptr, "%s:%s", host, port);
            pcre2_substring_free(host);
            pcre2_substring_free(port);
            brokerptr += strlen(brokerptr);
            if (i < broker_ids.count - 1)
                *brokerptr++ = ',';
        }
        else
            MQ_LOG_Log(LOG_WARNING, "Empty config returned for broker id %s",
                       broker_ids.data[i]);
    }
    deallocate_String_vector(&broker_ids);
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
            WRK_AddBrokers((const char *) brokers);
    }
}

const char
*MQ_ZOO_Init(char *brokers, int max)
{
    AN(zookeeper[0]);

    /* XXX: wait for ZOO_CONNECTED_STATE */
    errno = 0;
    zh = zookeeper_init(zookeeper, watcher, zoo_timeout, 0, 0, 0);
    if (zh == NULL) {
        snprintf(errmsg, LINE_MAX, "init/connect failure: %s", strerror(errno));
        return errmsg;
    }
    return setBrokerList(brokers, max);
}

const char
*MQ_ZOO_OpenLog(void)
{
    AN(zoolog);
    AN(zoolog[0]);

    zoologf = fopen(zoolog, "a");
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
