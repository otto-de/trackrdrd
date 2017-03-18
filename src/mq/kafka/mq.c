/*-
 * Copyright (c) 2014 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2014 Otto Gmbh & Co KG
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
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <ctype.h>
#include <signal.h>

#include <zookeeper/zookeeper_version.h>
#include <pcre.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "mq.h"
#include "mq_kafka.h"
#include "config_common.h"
#include "miniobj.h"

#define xstr(X) #X
#define str(X) xstr(X)

#if defined(CURRENT) && defined(REVISION) && defined(AGE)
#define SO_VERSION (str(CURRENT) "." str(REVISION) "." str(AGE))
#elif defined(VERSION)
#define SO_VERSION VERSION
#else
#define SO_VERSION "unknown version"
#endif

static char errmsg[LINE_MAX];
static char _version[LINE_MAX];

static int saved_lvl = LOG_INFO;
static int debug_toggle = 0;
struct sigaction toggle_action;

static void
toggle_debug(int sig)
{
    (void) sig;

    if (debug_toggle) {
        /* Toggle from debug back to saved level */
        loglvl = saved_lvl;
        debug_toggle = 0;
        MQ_LOG_Log(LOG_INFO, "Debug toggle switched off");
    }
    else {
        saved_lvl = loglvl;
        loglvl = LOG_DEBUG;
        debug_toggle = 1;
        MQ_LOG_Log(LOG_INFO, "Debug toggle switched on");
    }
    MQ_LOG_SetLevel(loglvl);
    MQ_ZOO_SetLogLevel(loglvl);
    for (int i = 0; i < nwrk; i++)
        if (workers[i] != NULL) {
            CHECK_OBJ(workers[i], KAFKA_WRK_MAGIC);
            rd_kafka_set_log_level(workers[i]->kafka, loglvl);
        }
}

const char *
MQ_GlobalInit(unsigned nworkers, const char *config_fname)
{
    CONF_Init();
    nwrk = nworkers;

    if (CONF_ReadFile(config_fname, CONF_Add) != 0)
        return "Error reading config file for Kafka";

    if (logpath[0] != '\0') {
        int err;
        if ((err = MQ_LOG_Open(logpath)) != 0) {
            snprintf(errmsg, LINE_MAX, "Cannot open %s: %s", logpath,
                     strerror(err));
            return errmsg;
        }
        MQ_LOG_SetLevel(loglvl);
    }
    snprintf(_version, LINE_MAX,
             "libtrackrdr-kafka %s, rdkafka %s, zookeeper %d.%d.%d, "
             "pcre %s", SO_VERSION, rd_kafka_version_str(),
             ZOO_MAJOR_VERSION, ZOO_MINOR_VERSION, ZOO_PATCH_VERSION,
             pcre_version());
    MQ_LOG_Log(LOG_INFO, "initializing (%s)", _version);

    if (zookeeper[0] == '\0' && brokerlist[0] == '\0') {
        snprintf(errmsg, LINE_MAX,
                 "zookeeper.connect and metadata.broker.list not set in %s",
                 config_fname);
        MQ_LOG_Log(LOG_ERR, errmsg);
        return errmsg;
    }

    if (topic[0] == '\0') {
        snprintf(errmsg, LINE_MAX, "topic not set in %s", config_fname);
        MQ_LOG_Log(LOG_ERR, errmsg);
        return errmsg;
    }

    workers = (kafka_wrk_t **) calloc(sizeof (kafka_wrk_t *), nworkers);
    if (workers == NULL) {
        snprintf(errmsg, LINE_MAX, "Cannot allocate worker table: %s",
                 strerror(errno));
        MQ_LOG_Log(LOG_ERR, errmsg);
        return errmsg;
    }

    toggle_action.sa_handler = toggle_debug;
    AZ(sigemptyset(&toggle_action.sa_mask));
    toggle_action.sa_flags |= SA_RESTART;
    if (sigaction(SIGUSR2, &toggle_action, NULL) != 0) {
        snprintf(errmsg, LINE_MAX, "Cannot install signal handler for USR2: %s",
                 strerror(errno));
        MQ_LOG_Log(LOG_ERR, errmsg);
        return errmsg;
    }

    if (zoolog[0] != '\0') {
        const char *err = MQ_ZOO_OpenLog();
        if (err != NULL) {
            snprintf(errmsg, LINE_MAX, "Cannot open zookeeper.log %s: %s",
                     zoolog, err);
            MQ_LOG_Log(LOG_ERR, errmsg);
            return errmsg;
        }
    }

    if (stats_interval != 0) {
        int err = MQ_MON_Init();
        if (err != 0) {
            snprintf(errmsg, LINE_MAX, "Cannot start monitoring thread: %s",
                     strerror(err));
            MQ_LOG_Log(LOG_ERR, errmsg);
            return errmsg;
        }
    }

    rd_kafka_conf_set_dr_cb(conf, CB_DeliveryReport);
    rd_kafka_conf_set_error_cb(conf, CB_Error);
    rd_kafka_conf_set_log_cb(conf, CB_Log);
    rd_kafka_conf_set_stats_cb(conf, CB_Stats);
    rd_kafka_topic_conf_set_partitioner_cb(topic_conf, CB_Partitioner);

    if (loglvl == LOG_DEBUG) {
        size_t cfglen;
        const char **cfg;

        /* Dump config */
        CONF_Dump();
        cfg = rd_kafka_conf_dump(conf, &cfglen);
        if (cfg != NULL && cfglen > 0)
            for (int i = 0; i < cfglen >> 1; i++) {
                if (cfg[2*i] == NULL)
                    break;
                MQ_LOG_Log(LOG_DEBUG, "%s = %s", cfg[2*i], cfg[2*i + 1]);
            }
        rd_kafka_conf_dump_free(cfg, cfglen);
        cfg = rd_kafka_topic_conf_dump(topic_conf, &cfglen);
        if (cfg != NULL && cfglen > 0)
            for (int i = 0; i < cfglen >> 1; i++)
                MQ_LOG_Log(LOG_DEBUG, "%s = %s", cfg[2*i], cfg[2*i + 1]);
        rd_kafka_conf_dump_free(cfg, cfglen);

        MQ_ZOO_SetLogLevel(LOG_DEBUG);
    }

    return NULL;
}

const char *
MQ_InitConnections(void)
{
    AN(conf);
    AN(topic_conf);
    assert(zookeeper[0] != '\0' || brokerlist[0] != '\0');

    if (zookeeper[0] != '\0') {
        char zbrokerlist[LINE_MAX];
        const char *err;

        if ((err = MQ_ZOO_Init(zbrokerlist, LINE_MAX)) != NULL) {
            snprintf(errmsg, LINE_MAX,
                     "Failed to init/connect to zookeeper [%s]: %s",
                     zookeeper, err);
            MQ_LOG_Log(LOG_ERR, errmsg);
            return errmsg;
        }
        if (zbrokerlist[0] == '\0')
            if (brokerlist[0] == '\0') {
                snprintf(errmsg, LINE_MAX,
                         "Zookeeper at %s returned no brokers, and "
                         "metadata.broker.list not configured", zookeeper);
                MQ_LOG_Log(LOG_ERR, errmsg);
                return errmsg;
            }
            else
                MQ_LOG_Log(LOG_WARNING, "Zookeeper at %s returned no brokers, "
                           "using value of metadata.broker.list instead",
                           zookeeper);
        else {
            strcpy(brokerlist, zbrokerlist);
            MQ_LOG_Log(LOG_DEBUG, "Zookeeper %s broker list %s", zookeeper,
                       brokerlist);
        }
    }
    if (rd_kafka_conf_set(conf, "metadata.broker.list", brokerlist, errmsg,
                          LINE_MAX) != RD_KAFKA_CONF_OK) {
        MQ_LOG_Log(LOG_ERR,
                   "rdkafka config error [metadata.broker.list = %s]: %s",
                   brokerlist, errmsg);
        return errmsg;
    }

    for (int i = 0; i < nwrk; i++) {
        const char *err = WRK_Init(i);
        if (err != NULL)
            return err;
    }

    return NULL;
}

const char *
MQ_WorkerInit(void **priv, int wrk_num)
{
    kafka_wrk_t *wrk;

    assert(wrk_num >= 1 && wrk_num <= nwrk);
    wrk = workers[wrk_num - 1];
    CHECK_OBJ_NOTNULL(wrk, KAFKA_WRK_MAGIC);
    *priv = (void *) wrk;
    return NULL;
}

int
MQ_Send(void *priv, const char *data, unsigned len, const char *key,
        unsigned keylen, const char **error)
{
    kafka_wrk_t *wrk;
    void *payload = NULL;

    if (priv == NULL) {
        MQ_LOG_Log(LOG_ERR, "MQ_Send() called with NULL worker object");
        *error = "MQ_Send() called with NULL worker object";
        return -1;
    }
    CAST_OBJ(wrk, priv, KAFKA_WRK_MAGIC);
    wrk->seen++;

    /* XXX: error? */
    if (len == 0) {
        wrk->nodata++;
        return 0;
    }

    rd_kafka_poll(wrk->kafka, 0);

    if (key == NULL || keylen == 0) {
        snprintf(wrk->errmsg, LINE_MAX, "%s message shard key is missing",
                 rd_kafka_name(wrk->kafka));
        if (log_error_data) {
            MQ_LOG_Log(LOG_ERR, "%s: data=[%.*s] key=[]", wrk->errmsg, len,
                       data);
        }
        else {
            MQ_LOG_Log(LOG_ERR, wrk->errmsg);
            MQ_LOG_Log(LOG_DEBUG, "%s data=[%.*s] key=[]",
                       rd_kafka_name(wrk->kafka), len, data);
        }
        wrk->nokey++;
        *error = wrk->errmsg;
        return 1;
    }
    if (data == NULL) {
        snprintf(wrk->errmsg, LINE_MAX, "%s message payload is NULL",
                 rd_kafka_name(wrk->kafka));
        if (log_error_data) {
            MQ_LOG_Log(LOG_ERR, "%s: data=[] key=[%.*s]", wrk->errmsg, keylen,
                       key);
        }
        else {
            MQ_LOG_Log(LOG_ERR, wrk->errmsg);
            MQ_LOG_Log(LOG_DEBUG, "%s data=[] key=[%.*s]",
                       rd_kafka_name(wrk->kafka), keylen, key);
        }
        wrk->nodata++;
        *error = wrk->errmsg;
        return 1;
    }

    if (keylen > 8)
        keylen = 8;
    for (int i = 0; i < keylen; i++)
        if (!isxdigit(key[i])) {
            snprintf(wrk->errmsg, LINE_MAX, "%s message shard key is not hex",
                     rd_kafka_name(wrk->kafka));
            if (log_error_data) {
                MQ_LOG_Log(LOG_ERR, "%s: data=[%.*s] key=[%.*s]", wrk->errmsg,
                           len, data, keylen, key);
            }
            else {
                MQ_LOG_Log(LOG_ERR, wrk->errmsg);
                MQ_LOG_Log(LOG_DEBUG, "%s data=[%.*s] key=[%.*s]",
                           rd_kafka_name(wrk->kafka), len, data, keylen, key);

            }
            *error = wrk->errmsg;
            wrk->badkey++;
            return 1;
        }

    REPLACE(payload, data);
    if (rd_kafka_produce(wrk->topic, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_FREE,
                         payload, len, key, keylen, NULL) == -1) {
        snprintf(wrk->errmsg, LINE_MAX, "%s",
                 rd_kafka_err2str(rd_kafka_errno2err(errno)));
        MQ_LOG_Log(LOG_ERR, "%s message send failure (%d): %s",
                   rd_kafka_name(wrk->kafka), errno, wrk->errmsg);
        *error = wrk->errmsg;
        return -1;
    }

    wrk->produced++;
    rd_kafka_poll(wrk->kafka, 0);
    return 0;
}

const char *
MQ_Reconnect(void **priv)
{
    kafka_wrk_t *wrk;
    int wrk_num;
    const char *err;

    CAST_OBJ_NOTNULL(wrk, *priv, KAFKA_WRK_MAGIC);
    wrk_num = wrk->n;
    assert(wrk_num >= 0 && wrk_num < nwrk);
    WRK_Fini(wrk);

    err = WRK_Init(wrk_num);
    if (err != NULL)
        return err;
    *priv = workers[wrk_num];
    return NULL;
}

const char *
MQ_Version(void *priv, char *version, size_t len)
{
    (void) priv;
    strncpy(version, _version, len);
    return NULL;
}

const char *
MQ_ClientID(void *priv, char *clientID, size_t len)
{
    kafka_wrk_t *wrk;
    CAST_OBJ_NOTNULL(wrk, priv, KAFKA_WRK_MAGIC);
    strncpy(clientID, rd_kafka_name(wrk->kafka), len);
    return NULL;
}

const char *
MQ_WorkerShutdown(void **priv, int wrk_num)
{
    kafka_wrk_t *wrk;

    (void) wrk_num;
    CAST_OBJ_NOTNULL(wrk, *priv, KAFKA_WRK_MAGIC);
    WRK_Fini(wrk);
    *priv = NULL;

    return NULL;
}

const char *
MQ_GlobalShutdown(void)
{
    const char *err = NULL;

    MQ_MON_Fini();
    for (int i = 0; i < nwrk; i++)
        if (workers[i] != NULL)
            WRK_Fini(workers[i]);
    free(workers);

    if (wrk_shutdown_timeout
        && rd_kafka_wait_destroyed(wrk_shutdown_timeout) != 0)
        MQ_LOG_Log(LOG_WARNING, "timeout (%u ms) waiting for "
                   "rdkafka clients to shut down", wrk_shutdown_timeout);

    rd_kafka_conf_destroy(conf);
    rd_kafka_topic_conf_destroy(topic_conf);

    err = MQ_ZOO_Fini();
    if (err != NULL) {
        snprintf(errmsg, LINE_MAX, "Error closing zookeeper: %s", err);
        MQ_LOG_Log(LOG_ERR, errmsg);
    }

    MQ_LOG_Log(LOG_INFO, "shutting down");
    MQ_LOG_Close();
    if (err != NULL)
        return errmsg;
    return NULL;
}
