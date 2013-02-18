/*-
 * Copyright (c) 2012 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012 Otto Gmbh & Co KG
 * All rights reserved
 * Use only with permission
 *
 * Authors: Geoffrey Simmons <geoffrey.simmons@uplex.de>
 *	    Nils Goroll <nils.goroll@uplex.de>
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
#include <sys/types.h>
#include <time.h>
#include <signal.h>

#include "vqueue.h"
#include "varnishapi.h"

/* assert.c */

void ASRT_Fail(const char *func, const char *file, int line, const char *cond,
    int err, int xxx);
               
/* handler.c */

/* Hack, because we cannot have #ifdef in the macro definition SIGDISP */
#define _UNDEFINED(SIG) ((#SIG)[0] == 0)
#define UNDEFINED(SIG) _UNDEFINED(SIG)

#define SIGDISP(SIG, action)						\
    do { if (UNDEFINED(SIG)) break;					\
	if (sigaction((SIG), (&action), NULL) != 0)			\
             LOG_Log(LOG_ALERT,						\
                 "Cannot install handler for " #SIG ": %s",		\
                 strerror(errno));					\
    } while(0)

volatile sig_atomic_t term;

struct sigaction terminate_action, ignore_action, stacktrace_action,
    default_action;
    
void HNDL_Abort(int sig);
void HNDL_Terminate(int sig);

/* sandbox.c */

void PRIV_Sandbox(void);

/* worker.c */

/**
 * Initializes resources for worker threads -- allocates memory,
 * initializes mutexes and condition variables.
 *
 * @returns 0 on success, an errno value on failure
 */
int WRK_Init(void);
void WRK_Start(void);
void WRK_Stats(void);
int WRK_Running(void);
void WRK_Halt(void);
void WRK_Shutdown(void);

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
bool SPMCQ_NeedWorker(int running);
bool SPMCQ_StopWorker(int running);

#define spmcq_wait(what)						\
    do {								\
	AZ(pthread_mutex_lock(&spmcq_##what##waiter_lock));		\
        spmcq_##what##waiter++;                                         \
        AZ(pthread_cond_wait(&spmcq_##what##waiter_cond,		\
                &spmcq_##what##waiter_lock));                           \
        spmcq_##what##waiter--;                                         \
        AZ(pthread_mutex_unlock(&spmcq_##what##waiter_lock));           \
    } while (0)

/* 
 * the first test is not synced, so we might enter the if body too late or
 * unnecessarily
 *
 * * too late: doesn't matter, will come back next time
 * * unnecessarily: we'll find out now
 */

#define spmcq_signal(what)						\
    do {								\
        if (spmcq_##what##waiter) {					\
            AZ(pthread_mutex_lock(&spmcq_##what##waiter_lock));         \
            if (spmcq_##what##waiter)                                   \
                AZ(pthread_cond_signal(&spmcq_##what##waiter_cond));    \
            AZ(pthread_mutex_unlock(&spmcq_##what##waiter_lock));	\
        }								\
    } while (0)

/* Producer waits for this condition when the spmc queue is full.
   Consumers signal this condition after dequeue. */
pthread_cond_t  spmcq_roomwaiter_cond;
pthread_mutex_t spmcq_roomwaiter_lock;
int		spmcq_roomwaiter;

/* Consumers wait for this condition when the spmc queue is empty.
   Producer signals this condition after enqueue. */
pthread_cond_t  spmcq_datawaiter_cond;
pthread_mutex_t spmcq_datawaiter_lock;
int		spmcq_datawaiter;

/* mq.c */
const char *MQ_GlobalInit(void);
const char *MQ_InitConnections(void);
const char *MQ_WorkerInit(void **priv);
const char *MQ_Send(void *priv, const char *data, unsigned len);
const char *MQ_Version(void *priv, char *version);
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

struct dataentry_s {
    unsigned 			magic;
#define DATA_MAGIC 0xb41cb1e1
    VSTAILQ_ENTRY(dataentry_s)	freelist;

    data_state_e		state;
    unsigned 			xid;
    unsigned 			tid;	/* 'Thread ID', fd in the callback */
    unsigned			end;	/* End of string index in data */
    bool			hasdata;
    
    bool		        incomplete; /* expired or evacuated */
    char			*data;
};
typedef struct dataentry_s dataentry;

VSTAILQ_HEAD(freehead_s, dataentry_s);

/* stats owned by VSL thread */
struct data_writer_stats_s {
    unsigned		nodata;		/* Not submitted, no data */
    unsigned		submitted;	/* Submitted to worker threads */
    unsigned		wait_qfull;	/* Waits for SPMCQ - should not happen */
    unsigned		wait_room;	/* waits for space in dtbl */
    unsigned		data_hi;	/* max string length of entry->data */

#ifdef REMOVE
    unsigned		len_overflows;
#endif
    unsigned		data_overflows;
};

/* stats protected by mutex */
struct data_reader_stats_s {
    pthread_mutex_t	mutex;
    unsigned		done;
    unsigned       	open;	
    unsigned		sent;		/* Sent successfully to MQ */
    unsigned		failed;		/* MQ send fails */
    unsigned		occ_hi;		/* Occupancy high water mark */ 
    unsigned		occ_hi_this;	/* Occupancy high water mark this reporting interval*/
};

struct datatable_s {
    unsigned			magic;
#define DATATABLE_MAGIC 	0xd3ef3bd4
    unsigned			len;

    /* protected by freelist_lock */
    struct freehead_s		freehead;
    pthread_mutex_t		freelist_lock;
    unsigned			nfree;

    dataentry			*entry;
    char			*buf;

    struct data_writer_stats_s	w_stats;
    struct data_reader_stats_s	r_stats;
};

typedef struct datatable_s datatable;

datatable dtbl;

int DATA_Init(void);
void DATA_Take_Freelist(struct freehead_s *dst);
void DATA_Return_Freelist(struct freehead_s *returned, unsigned nreturned);
void DATA_Dump1(dataentry *entry, int i);
void DATA_Dump(void);

/* trackrdrd.c */
void HASH_Stats(void);

/* child.c */
void CHILD_Main(struct VSM_data *vd, int endless, int readconfig);

/* config.c */
#define EMPTY(s) (s[0] == '\0')

#define DEFAULT_CONFIG "/etc/trackrdrd.conf"
char cli_config_filename[BUFSIZ];

struct config {
    char	pid_file[BUFSIZ];
    char	varnish_name[BUFSIZ];
    char	log_file[BUFSIZ];
    char	varnish_bindump[BUFSIZ];
    int		syslog_facility;
    char	syslog_facility_name[BUFSIZ];
    unsigned	monitor_interval;
    bool	monitor_workers;

    /* scale: unit is log(2,n), iow scale is taken to the power of 2 */
    unsigned	maxopen_scale;	/* max number of records in *_OPEN state */
#define DEF_MAXOPEN_SCALE 10
    
    unsigned	maxdone;	/* max number of records in *_DONE state */
#define DEF_MAXDONE 1024
    
    unsigned	maxdata;  	/* size of char data buffer */
#define DEF_MAXDATA 1024

    /*
     * queue-length goal:
     *
     * we scale the number of running workers dynamically propotionally to
     * the queue length.
     *
     * this specifies the queue length at which all workers should be
     * running
     */
    unsigned	qlen_goal;
#define DEF_QLEN_GOAL 1024

    /* max number of probes for insert/lookup */
    unsigned	hash_max_probes;
#define DEF_HASH_MAX_PROBES 10

    /* 
     * hash_ttl: max ttl for entries in HASH_OPEN
     * 
     * entries which are older than this ttl _may_ get expired from the
     * trackrdrd state.
     *
     * set to a value significantly longer than your maximum session
     * lifetime in varnish.
     */
    unsigned	hash_ttl;
#define DEF_HASH_TTL 120

    /*
     * hash_mlt: min lifetime for entries in HASH_OPEN before they could
     * get evacuated
     *
     * entries are guaranteed to remain in trackrdrd for this duration.
     * once the mlt is reached, they _may_ get expired if trackrdrd needs
     * space in the hash
     */
    unsigned	hash_mlt;
#define DEF_HASH_MLT 5

    unsigned	n_mq_uris;
    char	**mq_uri;
    char	mq_qname[BUFSIZ];
    unsigned	mq_pool_size;
    unsigned	nworkers;
    unsigned	restarts;
    char	user_name[BUFSIZ];
    uid_t	uid;
    gid_t	gid;
} config;

void CONF_Init(void);
int CONF_Add(const char *lval, const char *rval);
int CONF_ReadFile(const char *file);
int CONF_ReadDefault(void);
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
typedef enum {
    /* Record sent successfully to MQ */
    STATS_SENT,
    /* Failed to send record to MQ */
    STATS_FAILED,
    /* ReqStart seen, finished reading record from SHM log */
    STATS_DONE,
    /* Update occupancy high water mark */
    STATS_OCCUPANCY,
    /* ReqEnd seen, no data in the record */
    STATS_NODATA,
} stats_update_t;

void *MON_StatusThread(void *arg);
void MON_StatusShutdown(pthread_t monitor);
void MON_StatsInit(void);
void MON_StatsUpdate(stats_update_t update);

/* parse.c */
int Parse_XID(const char *str, int len, unsigned *xid);
int Parse_ReqStart(const char *ptr, int len, unsigned *xid);
int Parse_ReqEnd(const char *ptr, unsigned len, unsigned *xid,
    struct timespec *reqend_t);
int Parse_VCL_Log(const char *ptr, int len, unsigned *xid,
    char **data, int *datalen);

/* generic init attributes */
pthread_mutexattr_t attr_lock;
pthread_condattr_t  attr_cond;
