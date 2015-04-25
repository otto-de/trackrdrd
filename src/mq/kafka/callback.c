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
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <stdint.h>

#include "mq_kafka.h"
#include "miniobj.h"

/*
 * Partitioner assumes that the key string is an unsigned 32-bit
 * hexadecimal.
 */
static inline int32_t
get_partition(const void *keydata, size_t keylen, int32_t partition_cnt)
{
    int32_t partition;
    unsigned long key;
    char keystr[sizeof("ffffffff")], *endptr = NULL;

    assert(partition_cnt > 0);
    assert(keylen <= 8);

    strncpy(keystr, (const char *) keydata, keylen);
    keystr[keylen] = '\0';
    errno = 0;
    key = strtoul(keystr, &endptr, 16);
    if (errno != 0 || *endptr != '\0' || key < 0 || key > UINT32_MAX)
        return -1;
    if ((partition_cnt & (partition_cnt - 1)) == 0)
        /* partition_cnt is a power of 2 */
        partition = key & (partition_cnt - 1);
    else
        partition = key % partition_cnt;
    return partition;
}

int32_t
TEST_Partition(const void *keydata, size_t keylen, int32_t partition_cnt)
{
    return get_partition(keydata, keylen, partition_cnt);
}

int32_t
CB_Partitioner(const rd_kafka_topic_t *rkt, const void *keydata, size_t keylen,
               int32_t partition_cnt, void *rkt_opaque, void *msg_opaque)
{
    int32_t partition;
    (void) rkt_opaque;
    (void) msg_opaque;

    partition = get_partition(keydata, keylen, partition_cnt);
    if (partition < 0) {
        MQ_LOG_Log(LOG_ERR, "Cannot parse partition key: %.*s", (int) keylen,
                   (const char *) keydata);
        return RD_KAFKA_PARTITION_UA;
    }
    if (! rd_kafka_topic_partition_available(rkt, partition)) {
        MQ_LOG_Log(LOG_ERR, "Partition %d not available", partition);
        return RD_KAFKA_PARTITION_UA;
    }
    MQ_LOG_Log(LOG_DEBUG,
               "Computed partition %d for key %.*s (%d partitions)",
               partition, (int) keylen, (const char *) keydata, partition_cnt);
    return partition;
}

void
CB_Log(const rd_kafka_t *rk, int level, const char *fac, const char *buf)
{
    (void) fac;
    MQ_LOG_Log(level, "rdkafka %s: %s", rd_kafka_name(rk), buf);
}

void
CB_DeliveryReport(rd_kafka_t *rk, void *payload, size_t len,
                  rd_kafka_resp_err_t err, void *opaque, void *msg_opaque)
{
    (void) msg_opaque;
    kafka_wrk_t *wrk = (kafka_wrk_t *) opaque;
    CHECK_OBJ_NOTNULL(wrk, KAFKA_WRK_MAGIC);

    if (err != RD_KAFKA_RESP_ERR_NO_ERROR) {
        if (loglvl == LOG_DEBUG)
            MQ_LOG_Log(LOG_DEBUG,
                       "Delivery error %d (client ID = %s), msg = [%.*s]: %s",
                       err, rd_kafka_name(rk), (int) len, (char *) payload,
                       rd_kafka_err2str(err));
        else
            MQ_LOG_Log(LOG_ERR,
                       "Delivery error %d (client ID = %s): %s",
                       err, rd_kafka_name(rk), rd_kafka_err2str(err));
        wrk->failed++;
    }
    else {
        wrk->delivered++;
        if (loglvl == LOG_DEBUG)
            MQ_LOG_Log(LOG_DEBUG, "Delivered (client ID = %s): msg = [%.*s]",
                       rd_kafka_name(rk), (int) len, (char *) payload);
    }
}

void
CB_Error(rd_kafka_t *rk, int err, const char *reason, void *opaque)
{
    (void) opaque;

    MQ_LOG_Log(LOG_ERR, "Client error (ID = %s) %d: %s", rd_kafka_name(rk), err,
               reason);
}

int
CB_Stats(rd_kafka_t *rk, char *json, size_t json_len, void *opaque)
{
    kafka_wrk_t *wrk = (kafka_wrk_t *) opaque;
    CHECK_OBJ_NOTNULL(wrk, KAFKA_WRK_MAGIC);
    MQ_LOG_Log(LOG_INFO, "rdkafka stats (ID = %s): %.*s", rd_kafka_name(rk),
               (int) json_len, json);
    MQ_LOG_Log(LOG_INFO,
               "mq stats (ID = %s): seen=%u produced=%u delivered=%u failed=%u "
               "nokey=%u badkey=%u nodata=%u",
               rd_kafka_name(rk), wrk->seen, wrk->produced, wrk->delivered,
               wrk->failed, wrk->nokey, wrk->badkey, wrk->nodata);
    return 0;
}
