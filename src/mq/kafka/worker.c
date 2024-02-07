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

#include <syslog.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

#include "mq_kafka.h"
#include "miniobj.h"

static char errmsg[LINE_MAX];

static unsigned
get_clock_ms(void)
{
    struct timespec t;
    AZ(clock_gettime(CLOCK_REALTIME, &t));
    return (t.tv_sec * 1e3) + (t.tv_nsec / 1e6);
}

const char
*WRK_Init(int wrk_num)
{
    char clientid[HOST_NAME_MAX + 1 + sizeof("-kafka-worker-2147483648")];
    char host[HOST_NAME_MAX + 1];
    rd_kafka_conf_t *wrk_conf;
    rd_kafka_topic_conf_t *wrk_topic_conf;
    rd_kafka_t *rk;
    rd_kafka_topic_t *rkt;
    kafka_wrk_t *wrk;

    assert(wrk_num >= 0 && wrk_num < nwrk);

    wrk_conf = rd_kafka_conf_dup(conf);
    wrk_topic_conf = rd_kafka_topic_conf_dup(topic_conf);
    AZ(gethostname(host, HOST_NAME_MAX + 1));
    sprintf(clientid, "%s-kafka-worker-%d", host, wrk_num);
    if (rd_kafka_conf_set(wrk_conf, "client.id", clientid, errmsg,
                          LINE_MAX) != RD_KAFKA_CONF_OK) {
        MQ_LOG_Log(LOG_ERR, "rdkafka config error [client.id = %s]: %s",
                   clientid, errmsg);
        return errmsg;
    }
    rd_kafka_topic_conf_set_partitioner_cb(wrk_topic_conf, CB_Partitioner);

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

    rkt = rd_kafka_topic_new(rk, topic, wrk_topic_conf);
    if (rkt == NULL) {
        rd_kafka_resp_err_t rkerr = rd_kafka_last_error();
        snprintf(errmsg, LINE_MAX, "Failed to initialize topic: %s",
                 rd_kafka_err2str(rkerr));
        MQ_LOG_Log(LOG_ERR, errmsg);
        return errmsg;
    }

    wrk->n = wrk_num;
    wrk->kafka = rk;
    wrk->topic = rkt;
    wrk->errmsg[0] = '\0';
    wrk->seen = wrk->produced = wrk->delivered = wrk->failed = wrk->nokey
        = wrk->badkey = wrk->nodata = 0;
    workers[wrk_num] = wrk;
    MQ_LOG_Log(LOG_INFO, "initialized worker %d: %s", wrk_num,
               rd_kafka_name(wrk->kafka));
    rd_kafka_poll(wrk->kafka, 0);
    return NULL;
}

void
WRK_AddBrokers(const char *brokers)
{
    for (int i = 0; i < nwrk; i++)
        if (workers[i] != NULL) {
            int nbrokers;

            CHECK_OBJ(workers[i], KAFKA_WRK_MAGIC);
            nbrokers = rd_kafka_brokers_add(workers[i]->kafka, brokers);
            /* XXX: poll timeout configurable? */
            rd_kafka_poll(workers[i]->kafka, 10);
            MQ_LOG_Log(LOG_INFO, "%s: added %d brokers [%s]",
                       rd_kafka_name(workers[i]->kafka), nbrokers, brokers);
        }
}

void
WRK_Fini(kafka_wrk_t *wrk)
{
    int wrk_num;
    unsigned t = 0;

    CHECK_OBJ_NOTNULL(wrk, KAFKA_WRK_MAGIC);

    wrk_num = wrk->n;
    assert(wrk_num >= 0 && wrk_num < nwrk);

    /* Wait for messages to be delivered */
    if (wrk_shutdown_timeout)
        t = get_clock_ms();
    while (rd_kafka_outq_len(wrk->kafka) > 0) {
        rd_kafka_poll(wrk->kafka, 100);
        if (t && (get_clock_ms() - t > wrk_shutdown_timeout)) {
            MQ_LOG_Log(LOG_WARNING,
                       "%s: timeout (%u ms) waiting for message delivery",
                       rd_kafka_name(wrk->kafka), wrk_shutdown_timeout);
            break;
        }
    }

    rd_kafka_topic_destroy(wrk->topic);
    rd_kafka_destroy(wrk->kafka);
    FREE_OBJ(wrk);
    workers[wrk_num] = NULL;
}
