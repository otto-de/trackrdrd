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

#include "mq_kafka.h"

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
}

int
CONF_Add(const char *lval, const char *rval)
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

void
CONF_Dump(void)
{
    MQ_LOG_Log(LOG_DEBUG, "mq.log = %s", logpath);
    MQ_LOG_Log(LOG_DEBUG, "zookeeper.connect = %s", zookeeper);
    MQ_LOG_Log(LOG_DEBUG, "zookeeper.timeout = %u", zoo_timeout);
    MQ_LOG_Log(LOG_DEBUG, "zookeeper.log = %s", zoolog);
    MQ_LOG_Log(LOG_DEBUG, "topic = %s", topic);
    // leaving out mq.debug for now
}
