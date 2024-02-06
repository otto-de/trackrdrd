/*-
 * Copyright (c) 2014 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2014 Otto Gmbh & Co KG
 * All rights reserved
 * Use only with permission
 *
 * Authors: Geoffrey Simmons <geoffrey.simmons@uplex.de>
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

#include <assert.h>
#include <limits.h>

#include <librdkafka/rdkafka.h>

#include <syslog.h>

#define AZ(foo)         do { assert((foo) == 0); } while (0)
#define AN(foo)         do { assert((foo) != 0); } while (0)

typedef struct kafka_wrk {
    unsigned		magic;
#define KAFKA_WRK_MAGIC 0xd14d4425
    int			n;
    rd_kafka_t		*kafka;
    rd_kafka_topic_t	*topic;
    char		errmsg[LINE_MAX]; /* thread-safe return from MQ_*() */
    unsigned long	seen;
    unsigned long	produced;
    unsigned long	delivered;
    unsigned long	failed;
    unsigned long	nokey;
    unsigned long	badkey;
    unsigned long	nodata;
} kafka_wrk_t;

extern kafka_wrk_t **workers;
extern unsigned nwrk;

/* configuration */
extern char topic[LINE_MAX];
extern int loglvl;
extern char logpath[PATH_MAX];
extern char zookeeper[LINE_MAX];
extern char brokerlist[LINE_MAX];
extern char zoolog[PATH_MAX];
extern unsigned zoo_timeout;
extern unsigned stats_interval;
extern unsigned wrk_shutdown_timeout;
extern unsigned log_error_data;

extern rd_kafka_topic_conf_t *topic_conf;
extern rd_kafka_conf_t *conf;

/* log.c */
int MQ_LOG_Open(const char *path);
void MQ_LOG_Log(int level, const char *msg, ...);
void MQ_LOG_SetLevel(int level);
void MQ_LOG_Close(void);

/* monitor.c */
int MQ_MON_Init(void);
void MQ_MON_Fini(void);

/* zookeeper.c */
const char *MQ_ZOO_Init(char *brokers, int max);
const char *MQ_ZOO_OpenLog(void);
void MQ_ZOO_SetLogLevel(int level);
const char *MQ_ZOO_Fini(void);

/* worker.c */
const char *WRK_Init(int wrk_num);
void WRK_AddBrokers(const char *brokers);
void WRK_Fini(kafka_wrk_t *wrk);

/* callback.c */
int32_t CB_Partitioner(const rd_kafka_topic_t *rkt, const void *keydata,
                       size_t keylen, int32_t partition_cnt, void *rkt_opaque,
                       void *msg_opaque);
int32_t TEST_Partition(const void *keydata, size_t keylen,
                       int32_t partition_cnt);
void CB_Log(const rd_kafka_t *rk, int level, const char *fac, const char *buf);
void CB_DeliveryReport(rd_kafka_t *rk, void *payload, size_t len,
                       rd_kafka_resp_err_t err, void *opaque, void *msg_opaque);
void CB_Error(rd_kafka_t *rk, int err, const char *reason, void *opaque);
int CB_Stats(rd_kafka_t *rk, char *json, size_t json_len, void *opaque);

/* config.c */
void CONF_Init(void);
int CONF_Add(const char *lval, const char *rval);
void CONF_Dump(void);
