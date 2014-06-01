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
#include <stdlib.h>
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

static char logpath[PATH_MAX] = "";

static char zookeeper[LINE_MAX] = "";
static char brokerlist[LINE_MAX] = "";
static char zoolog[PATH_MAX] = "";
static unsigned zoo_timeout = 0;

static char topic[LINE_MAX] = "";
static rd_kafka_topic_conf_t *topic_conf;
static rd_kafka_conf_t *conf;

static unsigned stats_interval = 0;

static char errmsg[LINE_MAX];
static char _version[LINE_MAX];

static int loglvl = LOG_INFO;
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

static void
log_cb(const rd_kafka_t *rk, int level, const char *fac, const char *buf)
{
    (void) fac;
    MQ_LOG_Log(level, "rdkafka %s: %s", rd_kafka_name(rk), buf);
}

static void
dr_cb(rd_kafka_t *rk, void *payload, size_t len, rd_kafka_resp_err_t err,
      void *opaque, void *msg_opaque)
{
    (void) msg_opaque;

    if (err != RD_KAFKA_RESP_ERR_NO_ERROR) {
        kafka_wrk_t *wrk = (kafka_wrk_t *) opaque;
        CHECK_OBJ_NOTNULL(wrk, KAFKA_WRK_MAGIC);
        strncpy(wrk->reason, rd_kafka_err2str(err), LINE_MAX);
        MQ_LOG_Log(LOG_ERR, "Delivery error (client ID = %s, msg = [%.*s]): %s",
                   rd_kafka_name(rk), (int) len, (char *) payload, wrk->reason);
        wrk->err = (int) err;
    }
    else if (loglvl == LOG_DEBUG)
        MQ_LOG_Log(LOG_DEBUG, "Delivered (client ID = %s): msg = [%.*s]",
                   rd_kafka_name(rk), (int) len, (char *) payload);
}

static void
error_cb(rd_kafka_t *rk, int err, const char *reason, void *opaque)
{
    kafka_wrk_t *wrk = (kafka_wrk_t *) opaque;
    CHECK_OBJ_NOTNULL(wrk, KAFKA_WRK_MAGIC);
    MQ_LOG_Log(LOG_ERR, "Client error (ID = %s) %d: %s", rd_kafka_name(rk), err,
               reason);
    wrk->err = err;
    strncpy(wrk->reason, reason, LINE_MAX);
}

static int
stats_cb(rd_kafka_t *rk, char *json, size_t json_len, void *opaque)
{
    kafka_wrk_t *wrk = (kafka_wrk_t *) opaque;
    CHECK_OBJ_NOTNULL(wrk, KAFKA_WRK_MAGIC);
    MQ_LOG_Log(LOG_INFO, "rdkafka stats (ID = %s): %.*s", rd_kafka_name(rk),
               (int) json_len, json);
    MQ_LOG_Log(LOG_INFO, "mq stats (ID = %s): nokey=%u badkey=%u nodata=%u",
               rd_kafka_name(rk), wrk->nokey, wrk->badkey, wrk->nodata);
    return 0;
}

/*
 * Partitioner assumes that the key string is an unsigned 32-bit
 * hexadecimal.
 */
static int32_t
partitioner_cb(const rd_kafka_topic_t *rkt, const void *keydata, size_t keylen,
               int32_t partition_cnt, void *rkt_opaque, void *msg_opaque)
{
    int32_t partition;
    unsigned long key;
    char keystr[sizeof("ffffffff")], *endptr = NULL;
    (void) rkt_opaque;
    (void) msg_opaque;

    assert(partition_cnt > 0);
    assert(keylen <= 8);

    strncpy(keystr, (const char *) keydata, keylen);
    keystr[keylen] = '\0';
    errno = 0;
    key = strtoul(keystr, &endptr, 16);
    if (errno != 0 || *endptr != '\0' || key > 0xffffffffUL) {
        MQ_LOG_Log(LOG_ERR, "Cannot parse partition key: %.*s", (int) keylen,
                   (const char *) keydata);
        return RD_KAFKA_PARTITION_UA;
    }
    if ((partition_cnt & (partition_cnt - 1)) == 0)
        /* partition_cnt is a power of 2 */
        partition = key & (partition_cnt - 1);
    else
        partition = key % partition_cnt;

    if (! rd_kafka_topic_partition_available(rkt, partition)) {
        MQ_LOG_Log(LOG_ERR, "Partition %d not available", partition);
        return RD_KAFKA_PARTITION_UA;
    }
    MQ_LOG_Log(LOG_DEBUG, "Computed partition %d for key %.*s", partition,
               (int) keylen, (const char *) keydata);
    return partition;
}

/* XXX: encapsulate wrk_init and _fini in a separate source */

static const char
*wrk_init(int wrk_num)
{
    char clientid[sizeof("libtrackrdr-kafka-worker-2147483648")];
    rd_kafka_conf_t *wrk_conf;
    rd_kafka_topic_conf_t *wrk_topic_conf;
    rd_kafka_t *rk;
    rd_kafka_topic_t *rkt;
    kafka_wrk_t *wrk;

    assert(wrk_num >= 0 && wrk_num < nwrk);

    wrk_conf = rd_kafka_conf_dup(conf);
    wrk_topic_conf = rd_kafka_topic_conf_dup(topic_conf);
    sprintf(clientid, "libtrackrdr-kafka-worker-%d", wrk_num);
    if (rd_kafka_conf_set(wrk_conf, "client.id", clientid, errmsg,
                          LINE_MAX) != RD_KAFKA_CONF_OK) {
        MQ_LOG_Log(LOG_ERR, "rdkafka config error [client.id = %s]: %s",
                   clientid, errmsg);
        return errmsg;
    }
    rd_kafka_topic_conf_set_partitioner_cb(wrk_topic_conf, partitioner_cb);

    ALLOC_OBJ(wrk, KAFKA_WRK_MAGIC);
    if (wrk == NULL) {
        snprintf(errmsg, LINE_MAX, "Failed to create worker handle: %s",
                 strerror(errno));
        MQ_LOG_Log(LOG_ERR, errmsg);
        return errmsg;
    }
    rd_kafka_conf_set_opaque(wrk_conf, (void *) wrk);
    rd_kafka_topic_conf_set_opaque(wrk_topic_conf, (void *) wrk);

    rk = rd_kafka_new(RD_KAFKA_PRODUCER, wrk_conf, errmsg, LINE_MAX);
    if (rk == NULL) {
        MQ_LOG_Log(LOG_ERR, "Failed to create producer: %s", errmsg);
        return errmsg;
    }
    CHECK_OBJ_NOTNULL((kafka_wrk_t *) rd_kafka_opaque(rk), KAFKA_WRK_MAGIC);
    rd_kafka_set_log_level(rk, loglvl);

    errno = 0;
    rkt = rd_kafka_topic_new(rk, topic, wrk_topic_conf);
    if (rkt == NULL) {
        rd_kafka_resp_err_t rkerr = rd_kafka_errno2err(errno);
        snprintf(errmsg, LINE_MAX, "Failed to initialize topic: %s",
                 rd_kafka_err2str(rkerr));
        MQ_LOG_Log(LOG_ERR, errmsg);
        return errmsg;
    }

    wrk-> n = wrk_num;
    wrk->kafka = rk;
    wrk->topic = rkt;
    wrk->err = 0;
    wrk->errmsg[0] = '\0';
    wrk->reason[0] = '\0';
    wrk->nokey = wrk->badkey = wrk->nodata = 0;
    workers[wrk_num] = wrk;
    MQ_LOG_Log(LOG_INFO, "initialized worker %d: %s", wrk_num,
               rd_kafka_name(wrk->kafka));
    rd_kafka_poll(wrk->kafka, 0);
    return NULL;
}

static void
wrk_fini(kafka_wrk_t *wrk)
{
    int wrk_num;

    CHECK_OBJ_NOTNULL(wrk, KAFKA_WRK_MAGIC);

    wrk_num = wrk->n;
    assert(wrk_num >= 0 && wrk_num < nwrk);

    /* Wait for messages to be delivered */
    /* XXX: timeout? configure poll timeout? */
    while (rd_kafka_outq_len(wrk->kafka) > 0)
        rd_kafka_poll(wrk->kafka, 100);

    rd_kafka_topic_destroy(wrk->topic);
    rd_kafka_destroy(wrk->kafka);
    FREE_OBJ(wrk);
    AN(wrk);
    workers[wrk_num] = NULL;
}

static int
conf_add(const char *lval, const char *rval)
{
    rd_kafka_conf_res_t result;
    char errstr[LINE_MAX];

    errstr[0] = '\0';

    if (strcmp(lval, "mq.log") == 0) {
        strncpy(logpath, rval, PATH_MAX);
        return(0);
    }
    if (strcmp(lval, "zookeeper.connect") == 0) {
        strncpy(zookeeper, rval, LINE_MAX);
        return(0);
    }
    /* XXX: "zookeeper.connection.timeout.ms", to match Kafka config */
    if (strcmp(lval, "zookeeper.timeout") == 0) {
        char *endptr = NULL;
        long val;

        errno = 0;
        val = strtoul(rval, &endptr, 10);
        if (errno != 0)
            return errno;
        if (*endptr != '\0')
            return EINVAL;
        if (val > UINT_MAX)
            return ERANGE;
        zoo_timeout = val;
        return(0);
    }
    if (strcmp(lval, "statistics.interval.ms") == 0) {
        char *endptr = NULL;
        long val;

        errno = 0;
        val = strtoul(rval, &endptr, 10);
        if (errno != 0)
            return errno;
        if (*endptr != '\0')
            return EINVAL;
        if (val > UINT_MAX)
            return ERANGE;
        stats_interval = val;
        result = rd_kafka_conf_set(conf, lval, rval, errstr, LINE_MAX);
        if (result != RD_KAFKA_CONF_OK)
            return EINVAL;
        return(0);
    }
    if (strcmp(lval, "zookeeper.log") == 0) {
        strncpy(zoolog, rval, PATH_MAX);
        return(0);
    }
    if (strcmp(lval, "topic") == 0) {
        strncpy(topic, rval, LINE_MAX);
        return(0);
    }
    if (strcmp(lval, "metadata.broker.list") == 0) {
        strncpy(brokerlist, rval, LINE_MAX);
        result = rd_kafka_conf_set(conf, lval, rval, errstr, LINE_MAX);
        if (result != RD_KAFKA_CONF_OK)
            return EINVAL;
        return(0);
    }
    /* XXX: use the rdkakfka param "log_level" instead */
    if (strcmp(lval, "mq.debug") == 0) {
        if (strcmp(rval, "1") == 0
            || strcasecmp(rval, "true") == 0
            || strcasecmp(rval, "yes") == 0
            || strcasecmp(rval, "on") == 0)
            loglvl = LOG_DEBUG;
        else if (strcmp(rval, "0") != 0
                 && strcasecmp(rval, "false") != 0
                 && strcasecmp(rval, "no") != 0
                 && strcasecmp(rval, "off") != 0)
            return EINVAL;
        return(0);
    }

    result = rd_kafka_topic_conf_set(topic_conf, lval, rval, errstr, LINE_MAX);
    if (result == RD_KAFKA_CONF_UNKNOWN)
        result = rd_kafka_conf_set(conf, lval, rval, errstr, LINE_MAX);
    if (result != RD_KAFKA_CONF_OK)
        return EINVAL;
    else
        return(0);
}

const char *
MQ_GlobalInit(unsigned nworkers, const char *config_fname)
{
    nwrk = nworkers;
    conf = rd_kafka_conf_new();
    topic_conf = rd_kafka_topic_conf_new();

    if (CONF_ReadFile(config_fname, conf_add) != 0)
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
        const char *err = MQ_ZOO_SetLog(zoolog);
        if (err != NULL) {
            snprintf(errmsg, LINE_MAX, "Cannot open zookeeper.log %s: %s",
                     zoolog, err);
            MQ_LOG_Log(LOG_ERR, errmsg);
            return errmsg;
        }
    }

    if (stats_interval != 0) {
        int err = MQ_MON_Init(stats_interval);
        if (err != 0) {
            snprintf(errmsg, LINE_MAX, "Cannot start monitoring thread: %s",
                     strerror(err));
            MQ_LOG_Log(LOG_ERR, errmsg);
            return errmsg;
        }
    }

    rd_kafka_conf_set_dr_cb(conf, dr_cb);
    rd_kafka_conf_set_error_cb(conf, error_cb);
    rd_kafka_conf_set_log_cb(conf, log_cb);
    rd_kafka_conf_set_stats_cb(conf, stats_cb);
    rd_kafka_topic_conf_set_partitioner_cb(topic_conf, partitioner_cb);

    if (loglvl == LOG_DEBUG) {
        size_t cfglen;
        const char **cfg;

        /* Dump config */
        MQ_LOG_Log(LOG_DEBUG, "zookeeper.connect = %s", zookeeper);
        MQ_LOG_Log(LOG_DEBUG, "topic = %s", topic);
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

        if ((err = MQ_ZOO_Init(zookeeper, zoo_timeout, zbrokerlist, LINE_MAX))
            != NULL) {
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
        const char *err = wrk_init(i);
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

    /* XXX: error? */
    if (len == 0)
        return 0;

    if (priv == NULL) {
        MQ_LOG_Log(LOG_ERR, "MQ_Send() called with NULL worker object");
        *error = "MQ_Send() called with NULL worker object";
        return -1;
    }
    CAST_OBJ(wrk, priv, KAFKA_WRK_MAGIC);

    /* Check for an error state */
    rd_kafka_poll(wrk->kafka, 0);
    if (wrk->err) {
        snprintf(wrk->errmsg, LINE_MAX, "%s error state (%d): %s",
                 rd_kafka_name(wrk->kafka), wrk->err, wrk->reason);
        MQ_LOG_Log(LOG_ERR, wrk->errmsg);
        *error = wrk->errmsg;
        return -1;
    }

    /*
     * XXX
     * Toggle log level DEBUG with signals
     */
    if (key == NULL || keylen == 0) {
        snprintf(wrk->errmsg, LINE_MAX, "%s message shard key is missing",
                 rd_kafka_name(wrk->kafka));
        MQ_LOG_Log(LOG_ERR, wrk->errmsg);
        MQ_LOG_Log(LOG_DEBUG, "%s data=[%.*s] key=", rd_kafka_name(wrk->kafka),
                   len, data);
        wrk->nokey++;
        *error = wrk->errmsg;
        return 1;
    }
    if (data == NULL) {
        snprintf(wrk->errmsg, LINE_MAX, "%s message payload is NULL",
                 rd_kafka_name(wrk->kafka));
        MQ_LOG_Log(LOG_DEBUG, "%s data= key=[%.*s]", rd_kafka_name(wrk->kafka),
                   keylen, key);
        MQ_LOG_Log(LOG_ERR, wrk->errmsg);
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
            MQ_LOG_Log(LOG_ERR, wrk->errmsg);
            MQ_LOG_Log(LOG_DEBUG, "%s data=[%.*s] key=[%.*s]",
                       rd_kafka_name(wrk->kafka), len, data, keylen, key);
            *error = wrk->errmsg;
            wrk->badkey++;
            return 1;
        }

    REPLACE(payload, data);
    if (rd_kafka_produce(wrk->topic, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_FREE,
                         payload, len, key, keylen, NULL) == -1) {
        snprintf(wrk->errmsg, LINE_MAX,
                 rd_kafka_err2str(rd_kafka_errno2err(errno)));
        MQ_LOG_Log(LOG_ERR, "%s message send failure (%d): %s",
                   rd_kafka_name(wrk->kafka), errno, wrk->errmsg);
        *error = wrk->errmsg;
        return -1;
    }

    /* Check for an error state again */
    rd_kafka_poll(wrk->kafka, 0);
    if (wrk->err) {
        snprintf(wrk->errmsg, LINE_MAX, "%s error state (%d): %s",
                 rd_kafka_name(wrk->kafka), wrk->err, wrk->reason);
        MQ_LOG_Log(LOG_ERR, wrk->errmsg);
        *error = wrk->errmsg;
        return -1;
    }
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
    wrk_fini(wrk);

    err = wrk_init(wrk_num);
    if (err != NULL)
        return err;
    *priv = workers[wrk_num];
    return NULL;
}

const char *
MQ_Version(void *priv, char *version)
{
    (void) priv;
    strcpy(version, _version);
    return NULL;
}

const char *
MQ_ClientID(void *priv, char *clientID)
{
    kafka_wrk_t *wrk;
    CAST_OBJ_NOTNULL(wrk, priv, KAFKA_WRK_MAGIC);
    strcpy(clientID, rd_kafka_name(wrk->kafka));
    return NULL;
}

const char *
MQ_WorkerShutdown(void **priv)
{
    kafka_wrk_t *wrk;

    CAST_OBJ_NOTNULL(wrk, *priv, KAFKA_WRK_MAGIC);
    wrk_fini(wrk);
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
            wrk_fini(workers[i]);
    free(workers);

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
