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

/* Callbacks used by rdkafka */

#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdbool.h>

#include "mq_kafka.h"

static int
conf_getUnsignedInt(const char *rval, unsigned *i)
{
    unsigned long n;
    char *p;

    errno = 0;
    n = strtoul(rval, &p, 10);
    if (errno)
        return(errno);
    if (strlen(p) != 0)
        return(EINVAL);
    if (n > UINT_MAX)
        return(ERANGE);
    *i = (unsigned int) n;
    return(0);
}

void
CONF_Init(void)
{
    conf = rd_kafka_conf_new();
    topic_conf = rd_kafka_topic_conf_new();
    loglvl = LOG_INFO;
    topic[0] = '\0';
    logpath[0] = '\0';
    zookeeper[0] = '\0';
    zoo_timeout = 0;
    stats_interval = 0;
    zoolog[0] = '\0';
    brokerlist[0] = '\0';
    wrk_shutdown_timeout = 1000;
    log_error_data = false;
}

int
CONF_Add(const char *lval, const char *rval)
{
    rd_kafka_conf_res_t result;
    char errstr[LINE_MAX];
    int err;

    errstr[0] = '\0';

    if (strcmp(lval, "mq.log") == 0) {
        strncpy(logpath, rval, PATH_MAX);
        return(0);
    }
    if (strcmp(lval, "zookeeper.connect") == 0) {
        strncpy(zookeeper, rval, LINE_MAX);
        return(0);
    }
    if (strcmp(lval, "zookeeper.connection.timeout.ms") == 0) {
        if ((err = conf_getUnsignedInt(rval, &zoo_timeout)) != 0)
            return(err);
        return(0);
    }
    if (strcmp(lval, "worker.shutdown.timeout.ms") == 0) {
        if ((err = conf_getUnsignedInt(rval, &wrk_shutdown_timeout)) != 0)
            return(err);
        return(0);
    }
    if (strcmp(lval, "statistics.interval.ms") == 0) {
        if ((err = conf_getUnsignedInt(rval, &stats_interval)) != 0)
            return(err);
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
    if (strcmp(lval, "log_level") == 0) {
        unsigned l;
        if ((err = conf_getUnsignedInt(rval, &l)) != 0)
            return(err);
        if (loglvl > LOG_DEBUG)
            return EINVAL;
        loglvl = l;
        result = rd_kafka_conf_set(conf, lval, rval, errstr, LINE_MAX);
        if (result != RD_KAFKA_CONF_OK)
            return EINVAL;
        return(0);
    }
    if (strcmp(lval, "log_error_data") == 0) {
        if (strcasecmp(rval, "true") == 0
            || strcasecmp(rval, "on") == 0
            || strcasecmp(rval, "yes") == 0
            || strcmp(rval, "1") == 0) {
            log_error_data = true;
            return(0);
        }
        if (strcasecmp(rval, "false") == 0
            || strcasecmp(rval, "off") == 0
            || strcasecmp(rval, "no") == 0
            || strcmp(rval, "0") == 0) {
            log_error_data = false;
            return(0);
        }
        return(EINVAL);
    }

    result = rd_kafka_topic_conf_set(topic_conf, lval, rval, errstr, LINE_MAX);
    if (result == RD_KAFKA_CONF_UNKNOWN)
        result = rd_kafka_conf_set(conf, lval, rval, errstr, LINE_MAX);
    if (result != RD_KAFKA_CONF_OK)
        return EINVAL;
    else
        return(0);
}

void
CONF_Dump(void)
{
    MQ_LOG_Log(LOG_DEBUG, "mq.log = %s", logpath);
    MQ_LOG_Log(LOG_DEBUG, "zookeeper.connect = %s", zookeeper);
    MQ_LOG_Log(LOG_DEBUG, "zookeeper.timeout = %u", zoo_timeout);
    MQ_LOG_Log(LOG_DEBUG, "zookeeper.log = %s", zoolog);
    MQ_LOG_Log(LOG_DEBUG, "topic = %s", topic);
    MQ_LOG_Log(LOG_DEBUG, "worker.shutdown.timeout.ms = %u", wrk_shutdown_timeout);
    MQ_LOG_Log(LOG_DEBUG, "log_error_data = %s",
               log_error_data ? "true" : "false");
}
