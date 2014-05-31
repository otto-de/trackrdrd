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

#define AZ(foo)         do { assert((foo) == 0); } while (0)
#define AN(foo)         do { assert((foo) != 0); } while (0)

typedef struct kafka_wrk {
    unsigned		magic;
#define KAFKA_WRK_MAGIC 0xd14d4425
    int			n;
    rd_kafka_t		*kafka;
    rd_kafka_topic_t	*topic;
    int			err;
    char		reason[LINE_MAX]; /* errs from rdkafka callbacks    */
    char		errmsg[LINE_MAX]; /* thread-safe return from MQ_*() */
    unsigned		nokey;
    unsigned		badkey;
    unsigned		nodata;
} kafka_wrk_t;

kafka_wrk_t **workers;
unsigned nwrk;

/* log.c */
int MQ_LOG_Open(const char *path);
void MQ_LOG_Log(int level, const char *msg, ...);
void MQ_LOG_SetLevel(int level);
void MQ_LOG_Close(void);

/* monitor.c */
int MQ_MON_Init(unsigned interval);
void MQ_MON_Fini(void);
