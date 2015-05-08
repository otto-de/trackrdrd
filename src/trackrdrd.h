/*-
 * Copyright (c) 2012-2015 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012-2015 Otto Gmbh & Co KG
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
#include <pthread.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <stdint.h>
#include <limits.h>
#include <stddef.h>

#include "vapi/vsl.h"
#include "vqueue.h"

/* message queue methods, typedefs match the interface in mq.h */
typedef const char *global_init_f(unsigned nworkers, const char *config_fname);
typedef const char *init_connections_f(void);
typedef const char *worker_init_f(void **priv, int wrk_num);
typedef int send_f(void *priv, const char *data, unsigned len,
                    const char *key, unsigned keylen, const char **error);
typedef const char *version_f(void *priv, char *version, size_t len);
typedef const char *client_id_f(void *priv, char *clientID, size_t len);
typedef const char *reconnect_f(void **priv);
typedef const char *worker_shutdown_f(void **priv, int wrk_num);
typedef const char *global_shutdown_f(void);

struct mqf {
    global_init_f	*global_init;
    init_connections_f	*init_connections;
    worker_init_f	*worker_init;
    send_f		*send;
    version_f		*version;
    client_id_f		*client_id;
    reconnect_f		*reconnect;
    worker_shutdown_f	*worker_shutdown;
    global_shutdown_f	*global_shutdown;
} mqf;

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

struct sigaction ignore_action, stacktrace_action, default_action;

void HNDL_Init(const char *a0);
void HNDL_Abort(int sig);

/* sandbox.c */

void PRIV_Sandbox(void);

/* worker.c */

/* stats */
unsigned abandoned;

/**
 * Initializes resources for worker threads -- allocates memory,
 * initializes mutexes and condition variables.
 *
 * @returns 0 on success, an errno value on failure
 */
int WRK_Init(void);
void WRK_Start(void);
int WRK_Restart(void);
void WRK_Stats(void);
int WRK_Running(void);
int WRK_Exited(void);
void WRK_Halt(void);
void WRK_Shutdown(void);

/* data.c */

#define OCCUPIED(e) ((e)->occupied == 1)

unsigned global_nfree_rec, global_nfree_chunk;

typedef struct chunk_t {
    unsigned magic;
#define CHUNK_MAGIC 0x224a86ed
    char *data;
    VSTAILQ_ENTRY(chunk_t) freelist;
    VSTAILQ_ENTRY(chunk_t) chunklist;
    unsigned char occupied;
} chunk_t;

typedef VSTAILQ_HEAD(chunkhead_s, chunk_t) chunkhead_t;

struct dataentry_s {
    unsigned 			magic;
#define DATA_MAGIC 0xb41cb1e1
    chunkhead_t			chunks;
    VSTAILQ_ENTRY(dataentry_s)	freelist;
    VSTAILQ_ENTRY(dataentry_s)	spmcq;
    char			*key;
    chunk_t			*curchunk;
    unsigned			curchunkidx;
    unsigned			keylen;
    unsigned			end;	/* End of string index in data */
    unsigned char		occupied;
};
typedef struct dataentry_s dataentry;

VSTAILQ_HEAD(rechead_s, dataentry_s);

int DATA_Init(void);
unsigned DATA_Reset(dataentry *entry, chunkhead_t * const freechunk);
unsigned DATA_Take_Freerec(struct rechead_s *dst);
void DATA_Return_Freerec(struct rechead_s *returned, unsigned nreturned);
unsigned DATA_Take_Freechunk(struct chunkhead_s *dst);
void DATA_Return_Freechunk(struct chunkhead_s *returned, unsigned nreturned);
void DATA_Dump(void);

/* spmcq.c */

int SPMCQ_Init(void);
void SPMCQ_Enq(dataentry *ptr);
dataentry *SPMCQ_Deq(void);
void SPMCQ_Drain(void);
unsigned SPMCQ_NeedWorker(int running);

/* Consumers wait for this condition when the spmc queue is empty.
   Producer signals this condition after enqueue. */
pthread_cond_t  spmcq_datawaiter_cond;
pthread_mutex_t spmcq_datawaiter_lock;
int		spmcq_datawaiter;

/* child.c */
void RDR_Stats(void);
void CHILD_Main(int readconfig);
int RDR_Exhausted(void);

/* config.c */
#define EMPTY(s) (s[0] == '\0')

#define DEFAULT_CONFIG "/etc/trackrdrd.conf"
char cli_config_filename[PATH_MAX + 1];

struct config {
    char	pid_file[PATH_MAX];
    char	varnish_name[PATH_MAX];
    char	vsmfile[PATH_MAX];
    char	log_file[PATH_MAX];
    char	varnish_bindump[PATH_MAX];
    char	mq_module[PATH_MAX];
    char	mq_config_file[PATH_MAX];
    char	user_name[LOGIN_NAME_MAX + 1];
    char	syslog_facility_name[sizeof("LOCAL0")];

#define DEF_IDLE_PAUSE 0.01
    double	idle_pause;
    double	tx_timeout;

    uid_t	uid;
    gid_t	gid;
    int		syslog_facility;
    unsigned	monitor_interval;
    unsigned	monitor_workers;

    unsigned	max_records;	/* max number of buffered records */
#define DEF_MAX_RECORDS 1024
    
    unsigned	max_reclen;  	/* size of char data buffer */
#define DEF_MAX_RECLEN 1024

    unsigned	maxkeylen;	/* size of shard key buffer */
#define DEF_MAXKEYLEN 128

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
#define DEF_QLEN_GOAL 512

    unsigned	nworkers;
    unsigned	restarts;
    unsigned	restart_pause;
    unsigned	thread_restarts;
    unsigned	chunk_size;
#define DEF_CHUNK_SIZE 256
#define MIN_CHUNK_SIZE 64

    unsigned	tx_limit;
} config;

void CONF_Init(void);
int CONF_Add(const char *lval, const char *rval);
/**
 * Reads the default config file `/etc/trackrdrd.conf`, if present
 *
 * @returns 0 if the file does not exist or was successfully read,
 *          >0 (errno) if the file exists but cannot be read,
 *          <0 if the file was read but the contents could not be processed
 */
int CONF_ReadDefault(void);
void CONF_Dump(int level);

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
int LOG_GetLevel(void);
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
    /* Reconnected to MQ */
    STATS_RECONNECT,
    /* Update occupancy and high water marks */
    STATS_OCCUPANCY,
    /* Worker thread restarted */
    STATS_RESTART,
} stats_update_t;

void *MON_StatusThread(void *arg);
void MON_Output(void);
void MON_StatusShutdown(pthread_t monitor);
void MON_StatsInit(void);
void MON_StatsUpdate(stats_update_t update, unsigned nchunks, unsigned nbytes);

/* parse.c */

/* Whether a VCL_Log entry contains a data payload or a shard key */
typedef enum { VCL_LOG_DATA, VCL_LOG_KEY } vcl_log_t;

int Parse_VCL_Log(const char *ptr, int len, char **data, int *datalen,
                  vcl_log_t *type);
int Parse_Timestamp(const char *ptr, int len, struct timeval *t);
