/*-
 * Copyright (c) 2012 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012 Otto Gmbh & Co KG
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
#include <stdbool.h>
#include <pthread.h>

#define MIN_TABLE_SCALE 10

/* spmcq.c */

/* Single producer multiple consumer bounded FIFO queue */
typedef struct {
        unsigned magic;
#define SPMCQ_MAGIC 0xe9a5d0a8
        const unsigned mask;
        void **data;
        volatile unsigned head;
        volatile unsigned tail;
} spmcq_t;

spmcq_t spmcq;

int SPMCQ_Init(void);
bool SPMCQ_Enq(void *ptr);
void *SPMCQ_Deq(void);

/* Producer waits for this condition when the spmc queue is full.
   Consumers signal this condition after dequeue. */
pthread_cond_t spmcq_nonfull_cond;
pthread_mutex_t spmcq_nonfull_lock;

/* Consumers wait for this condition when the spmc queue is empty.
   Producer signals this condition after enqueue. */
pthread_cond_t spmcq_nonempty_cond;
pthread_mutex_t spmcq_nonempty_lock;

/* mq.c */
const char *MQ_GlobalInit(void);
const char *MQ_WorkerInit(void **priv);
const char *MQ_Send(void *priv, const char *data, unsigned len);
const char *MQ_WorkerShutdown(void **priv);
const char *MQ_GlobalShutdown(void);

/* data.c */
typedef enum {
    DATA_EMPTY = 0,
    /* OPEN when the main thread is filling data, ReqEnd not yet seen. */
    DATA_OPEN,
    /* DONE when ReqEnd has been seen, data have not yet been submitted. */
    DATA_DONE
} data_state_e;

typedef struct {
    unsigned 		magic;
#define DATA_MAGIC 0xb41cb1e1
    data_state_e	state;
    unsigned 		xid;
    unsigned 		tid;	/* 'Thread ID', fd in the callback */
    unsigned		end;	/* End of string index in data */
    char		*data;
} dataentry;

typedef struct {
    unsigned		magic;
#define DATATABLE_MAGIC 0xd3ef3bd4
    const unsigned	len;
    unsigned		collisions;
    unsigned		insert_probes;
    unsigned		find_probes;
    unsigned		seen;		/* Records (ReqStarts) seen */
    unsigned		open;
    unsigned		done;
    unsigned		submitted;	/* Records submitted */
    unsigned		occ_hi;		/* Occupancy high water mark */ 
    unsigned		data_hi;	/* Data high water mark */
    dataentry		*entry;
    char		*buf;
} datatable;

datatable tbl;

/* XXX: inline DATA_Insert and DATA_Find */
int DATA_Init(void);
dataentry *DATA_Insert(unsigned xid);
dataentry *DATA_Find(unsigned xid);
void DATA_Dump(void);

/* config.c */
#define EMPTY(s) (s[0] == '\0')

struct config {
    char	pid_file[BUFSIZ];
    char	varnish_name[BUFSIZ];
    char	log_file[BUFSIZ];
    char	varnish_bindump[BUFSIZ];
    int		syslog_facility;
    char	syslog_facility_name[BUFSIZ];
    double	monitor_interval;
    char	processor_log[BUFSIZ];
    unsigned	maxopen_scale;
    unsigned	maxdata_scale;
    char	mq_uri[BUFSIZ];
    char	mq_qname[BUFSIZ];
} config;

void CONF_Init(void);
int CONF_Add(const char *lval, const char *rval);
int CONF_ReadFile(const char *file);
void CONF_Dump(void);

/* log.c */
typedef void log_log_t(int level, const char *msg, ...);
typedef void log_setlevel_t(int level);
typedef void log_close_t(void);

struct logconf {
    log_log_t		*log;
    log_setlevel_t	*setlevel;
    log_close_t		*close;
    FILE		*out;
    int			level;
} logconf;

int LOG_Open(const char *progname);
/* XXX: __VA_ARGS__ can't be empty ... */
#define LOG_Log0(level, msg) logconf.log(level, msg)
#define LOG_Log(level, msg, ...) logconf.log(level, msg, __VA_ARGS__)
#define LOG_SetLevel(level) logconf.setlevel(level)
#define LOG_Close() logconf.close()

/* monitor.c */
void *MON_StatusThread(void *arg);

/* parse.c */
int Parse_XID(const char *str, int len, unsigned *xid);
int Parse_ReqStart(const char *ptr, int len, unsigned *xid);
int Parse_ReqEnd(const char *ptr, unsigned len, unsigned *xid);
int Parse_VCL_Log(const char *ptr, int len, unsigned *xid,
    char **data, int *datalen);

