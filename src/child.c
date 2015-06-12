/*-
 * Copyright (c) 2012-2015 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012-2015 Otto Gmbh & Co KG
 * All rights reserved
 * Use only with permission
 *
 * Authors: Geoffrey Simmons <geoffrey.simmons@uplex.de>
 *	    Nils Goroll <nils.goroll@uplex.de>
 *
 * Portions adopted from varnishlog.c from the Varnish project
 *	Author: Poul-Henning Kamp <phk@phk.freebsd.dk>
 * 	Copyright (c) 2006 Verdens Gang AS
 * 	Copyright (c) 2006-2011 Varnish Software AS
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
 * read tracking data from the Varnish SHM-log and send records to the
 * processor
 */

#include "config.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <pwd.h>
#include <limits.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <float.h>

#include "trackrdrd.h"
#include "config_common.h"
#include "vcs_version.h"
#include "vtim.h"

#include "vapi/vsl.h"
#include "miniobj.h"
#include "vas.h"
#include "vdef.h"

#define I_TAG "VSL"
#define I_FILTER_VCL_LOG "VCL_log:^track "
#define I_FILTER_TS "Timestamp:^Resp"

/* XXX: should these be configurable ? */
#define TRACKLOG_PREFIX "track "
#define TRACKLOG_PREFIX_LEN (sizeof(TRACKLOG_PREFIX)-1)
#define REQEND_T_VAR "req_endt"
#define REQEND_T_LEN (sizeof(REQEND_T_VAR "=1430176881.682097"))

#define DISPATCH_EOL 0
#define DISPATCH_RETURN_OK 0
#define DISPATCH_CONTINUE 1
#define DISPATCH_EOF -1
#define DISPATCH_CLOSED -2
#define DISPATCH_OVERRUN -3
#define DISPATCH_IOERR -4
#define DISPATCH_TERMINATE 10
#define DISPATCH_WRK_RESTART 11
#define DISPATCH_FLUSH 12
#define DISPATCH_WRK_ABANDONED 13

#define MAX_IDLE_PAUSE 0.01

const char *version = PACKAGE_TARNAME "-" PACKAGE_VERSION " revision " \
    VCS_Version " branch " VCS_Branch;

static unsigned len_hi = 0, debug = 0, data_exhausted = 0;

static unsigned long seen = 0, submitted = 0, len_overflows = 0, no_data = 0,
    no_free_data = 0, vcl_log_err = 0, vsl_errs = 0, closed = 0, overrun = 0,
    ioerr = 0, reacquire = 0, truncated = 0, key_hi = 0, key_overflows = 0,
    no_free_chunk = 0, eol = 0, no_timestamp = 0;

static double idle_pause = MAX_IDLE_PAUSE;

static volatile sig_atomic_t flush = 0, term = 0;

static struct sigaction terminate_action, dump_action, flush_action;

/* Local freelists */
static struct rechead_s reader_freerec = 
    VSTAILQ_HEAD_INITIALIZER(reader_freerec);
static chunkhead_t reader_freechunk = 
    VSTAILQ_HEAD_INITIALIZER(reader_freechunk);
static unsigned rdr_rec_free = 0, rdr_chunk_free = 0;

/*--------------------------------------------------------------------*/

void
RDR_Stats(void)
{
    LOG_Log(LOG_INFO, "Reader: seen=%lu submitted=%lu nodata=%lu eol=%lu "
            "idle_pause=%.09f free_rec=%u free_chunk=%u no_free_rec=%lu "
            "no_free_chunk=%lu len_hi=%u key_hi=%lu len_overflows=%lu "
            "truncated=%lu key_overflows=%lu vcl_log_err=%lu no_timestamp=%lu "
            "vsl_err=%lu closed=%lu overrun=%lu ioerr=%lu reacquire=%lu",
            seen, submitted, no_data, eol, idle_pause, rdr_rec_free,
            rdr_chunk_free, no_free_data, no_free_chunk, len_hi, key_hi,
            len_overflows, truncated, key_overflows, vcl_log_err, no_timestamp,
            vsl_errs, closed, overrun, ioerr, reacquire);
}

int
RDR_Exhausted(void)
{
    return data_exhausted;
}

/*--------------------------------------------------------------------*/

static void
dump(int sig)
{
    LOG_Log(LOG_NOTICE, "Child process received signal %d (%s), "
            "dumping config and data table", sig, strsignal(sig));
    CONF_Dump(LOG_INFO);
    DATA_Dump();
}

static void
term_s(int sig)
{
    LOG_Log(LOG_NOTICE, "Child process received signal %d (%s), "
            "will flush logs and terminate", sig, strsignal(sig));
    term = 1;
    flush = 1;
}

static void
sigflush(int sig)
{
    flush = 1;
    LOG_Log(LOG_NOTICE, "Received signal %d (%s), "
            "flushing pending transactions", sig, strsignal(sig));
}

/*--------------------------------------------------------------------*/

/* 
 * the first test is not synced, so we might enter the if body too late or
 * unnecessarily
 *
 * * too late: doesn't matter, will come back next time
 * * unnecessarily: we'll find out now
 */
static inline void
spmcq_signal(void)
{
    if (spmcq_datawaiter) {
        AZ(pthread_mutex_lock(&spmcq_datawaiter_lock));
        if (spmcq_datawaiter)
            AZ(pthread_cond_signal(&spmcq_datawaiter_cond));
        AZ(pthread_mutex_unlock(&spmcq_datawaiter_lock));
    }
}

/* efficiently retrieve a single data entry */

static inline dataentry
*data_get(void)
{
    dataentry *data;

    while (VSTAILQ_EMPTY(&reader_freerec)) {
        spmcq_signal();
        rdr_rec_free = DATA_Take_Freerec(&reader_freerec);
        if (VSTAILQ_EMPTY(&reader_freerec)) {
            data_exhausted = 1;
            return NULL;
        }
        if (debug)
            LOG_Log(LOG_DEBUG, "Reader: took %u free data entries",
                    rdr_rec_free);
    }
    data_exhausted = 0;
    data = VSTAILQ_FIRST(&reader_freerec);
    VSTAILQ_REMOVE_HEAD(&reader_freerec, freelist);
    rdr_rec_free--;
    return (data);
}

static inline chunk_t
*take_chunk(void)
{
    chunk_t *chunk;

    while (VSTAILQ_EMPTY(&reader_freechunk)) {
        spmcq_signal();
        rdr_chunk_free = DATA_Take_Freechunk(&reader_freechunk);
        if (VSTAILQ_EMPTY(&reader_freechunk)) {
            data_exhausted = 1;
            return NULL;
        }
        if (debug)
            LOG_Log(LOG_DEBUG, "Reader: took %u free chunks",
                    rdr_chunk_free);
    }
    data_exhausted = 0;
    chunk = VSTAILQ_FIRST(&reader_freechunk);
    VSTAILQ_REMOVE_HEAD(&reader_freechunk, freelist);
    rdr_chunk_free--;
    return (chunk);
}

/* return to our own local cache */

static inline void
data_free(dataentry *de)
{
    AN(de);
    rdr_chunk_free += DATA_Reset(de, &reader_freechunk);
    VSTAILQ_INSERT_HEAD(&reader_freerec, de, freelist);
}

static inline void
data_submit(dataentry *de)
{
    int wrk_running;

    CHECK_OBJ_NOTNULL(de, DATA_MAGIC);
    assert(OCCUPIED(de));
    if (debug) {
        chunk_t *chunk;
        char *p, *data = (char *) malloc(de->end);
        int n = de->end;
        p = data;
        chunk = VSTAILQ_FIRST(&de->chunks);
        while (n > 0) {
            CHECK_OBJ_NOTNULL(chunk, CHUNK_MAGIC);
            assert(OCCUPIED(chunk));
            int cp = n;
            if (cp > config.chunk_size)
                cp = config.chunk_size;
            memcpy(p, chunk->data, cp);
            chunk = VSTAILQ_NEXT(chunk, chunklist);
            n -= cp;
            p += cp;
        }
        assert(p == data + de->end);
        LOG_Log(LOG_DEBUG, "submit: data=[%.*s]", de->end, data);
        free(data);
    }

    SPMCQ_Enq(de);
    submitted++;

    /* should we wake up another worker?
     * base case: wake up a worker if all are sleeping
     *
     * this is an un-synced access to spmcq_data_waiter, but
     * if we don't wake them up now, we will next time around
     */
    wrk_running = WRK_Running();
    if (wrk_running == spmcq_datawaiter || SPMCQ_NeedWorker(wrk_running))
        spmcq_signal();
}

static inline void
take_free(void)
{
    rdr_rec_free += DATA_Take_Freerec(&reader_freerec);
    rdr_chunk_free += DATA_Take_Freechunk(&reader_freechunk);
}

/*--------------------------------------------------------------------*/

static inline int
need_wrk_restart(void)
{
    return WRK_Exited() - abandoned > 0;
}

static inline int
all_wrk_abandoned(void)
{
    return config.nworkers > 0 && abandoned == config.nworkers;
}

/*--------------------------------------------------------------------*/

static chunk_t *
get_chunk(dataentry *entry)
{
    chunk_t *chunk;

    CHECK_OBJ_NOTNULL(entry, DATA_MAGIC);

    chunk = take_chunk();
    if (chunk == NULL) {
        no_free_chunk++;
        return NULL;
    }
    CHECK_OBJ(chunk, CHUNK_MAGIC);
    assert(!OCCUPIED(chunk));
    entry->curchunk = chunk;
    entry->curchunkidx = 0;
    VSTAILQ_INSERT_TAIL(&entry->chunks, chunk, chunklist);
    chunk->occupied = 1;
    return chunk;
}

static unsigned
append(dataentry *entry, enum VSL_tag_e tag, unsigned xid, char *data,
       int datalen)
{
    chunk_t *chunk;
    char *null, *p;
    unsigned chunks_added = 0;
    int n;

    CHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
    /* Data overflow */
    if (entry->end + datalen + 1 > config.max_reclen) {
        LOG_Log(LOG_ERR, "%s: Data too long, XID=%d, current length=%d, "
            "DISCARDING data=[%.*s]", VSL_tags[tag], xid, entry->end,
            datalen, data);
        len_overflows++;
        return -1;
    }
    /* Null chars in the payload means that the data was truncated in the
       log, due to exceeding shm_reclen. */
    if ((null = memchr(data, '\0', datalen)) != NULL) {
        datalen = null - data;
        LOG_Log(LOG_ERR, "%s: Data truncated in SHM log, XID=%d, data=[%.*s]",
                VSL_tags[tag], xid, datalen, data);
        truncated++;
    }

    assert(entry->curchunkidx <= config.chunk_size);
    if (entry->curchunkidx == config.chunk_size) {
        chunk = get_chunk(entry);
        if (chunk == NULL)
            return -1;
        chunks_added++;
    }
    entry->curchunk->data[entry->curchunkidx] = '&';
    entry->curchunkidx++;
    entry->end++;

    p = data;
    n = datalen;
    while (n > 0) {
        assert(entry->curchunkidx <= config.chunk_size);
        if (entry->curchunkidx == config.chunk_size) {
            chunk = get_chunk(entry);
            if (chunk == NULL)
                return -1;
            chunks_added++;
        }
        int cp = n;
        if (cp + entry->curchunkidx > config.chunk_size)
            cp = config.chunk_size - entry->curchunkidx;
        memcpy(&entry->curchunk->data[entry->curchunkidx], p, cp);
        entry->curchunkidx += cp;
        p += cp;
        n -= cp;
    }
    assert(p == data + datalen);
    entry->end += datalen;
    if (entry->end > len_hi)
        len_hi = entry->end;
    return chunks_added;
}

static inline void
addkey(dataentry *entry, enum VSL_tag_e tag, unsigned xid, char *key,
       int keylen)
{
    CHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
    if (keylen > config.maxkeylen) {
        LOG_Log(LOG_ERR, "%s: Key too long, XID=%d, length=%d, "
                "DISCARDING key=[%.*s]", VSL_tags[tag], xid, keylen,
                keylen, key);
        key_overflows++;
        return;
    }
        
    memcpy(entry->key, key, keylen);
    entry->keylen = keylen;
    if (keylen > key_hi)
        key_hi = keylen;
    return;
}

static int
dispatch(struct VSL_data *vsl, struct VSL_transaction * const pt[], void *priv)
{
    int status = DISPATCH_RETURN_OK, hasdata = 0, chunks = 0;
    dataentry *de = NULL;
    char reqend_str[REQEND_T_LEN];
    int32_t vxid;
    struct timeval latest_t = { 0 };
    unsigned chunks_added = 0;
    (void) priv;

    if (all_wrk_abandoned())
        return DISPATCH_WRK_ABANDONED;

    de = data_get();
    if (de == NULL) {
        no_free_data++;
        return status;
    }
    CHECK_OBJ(de, DATA_MAGIC);
    assert(!OCCUPIED(de));
    seen++;

    for (struct VSL_transaction *t = pt[0]; t != NULL; t = *++pt) {
        if (debug)
            LOG_Log(LOG_DEBUG, "Reader read tx: [%u]", t->vxid);

        if (t->type != VSL_t_req)
            continue;

        while ((status = VSL_Next(t->c)) > 0) {
            int len, err;
            const char *payload;
            unsigned xid;
            enum VSL_tag_e tag;

            /* Quick filter for the tags of interest */
            switch(VSL_TAG(t->c->rec.ptr)) {
            case SLT_VCL_Log:
            case SLT_Timestamp:
            case SLT_VSL:
                break;
            default:
                continue;
            }

            /* Now filter for regexen, etc. */
            if (!VSL_Match(vsl, t->c))
                continue;

            assert(VSL_CLIENT(t->c->rec.ptr));

            if (de->end == 0) {
                chunk_t *chunk;

                chunk = get_chunk(de);
                if (chunk == NULL) {
                    if (debug)
                        LOG_Log(LOG_DEBUG, "Free chunks exhausted, "
                                "DATA DISCARDED: [Tx %d]", t->vxid);
                    data_free(de);
                    return status;
                }
                vxid = t->vxid;
                de->curchunk = chunk;
                /* XXX: minimum chunk size */
                snprintf(de->curchunk->data, config.chunk_size, "XID=%u",
                         t->vxid);
                de->curchunkidx = strlen(de->curchunk->data);
                de->end = de->curchunkidx;
                de->occupied = 1;
                if (de->end > len_hi)
                    len_hi = de->end;
                chunks_added++;
            }

            len = VSL_LEN(t->c->rec.ptr) - 1;
            payload = VSL_CDATA(t->c->rec.ptr);
            xid = VSL_ID(t->c->rec.ptr);
            tag = VSL_TAG(t->c->rec.ptr);
            if (debug)
                LOG_Log(LOG_DEBUG, "Reader read record: [%u %s %.*s]",
                        xid, VSL_tags[tag], len, payload);

            switch (VSL_TAG(t->c->rec.ptr)) {
                int datalen;
                char *data;
                vcl_log_t data_type;
                struct timeval reqend_t;

            case SLT_VCL_Log:
                AZ(strncmp(payload, TRACKLOG_PREFIX, TRACKLOG_PREFIX_LEN));
        
                err = Parse_VCL_Log(payload + TRACKLOG_PREFIX_LEN,
                                    len - TRACKLOG_PREFIX_LEN, &data,
                                    &datalen, &data_type);
                if (err != 0) {
                    LOG_Log(LOG_ERR,
                            "Cannot parse VCL_Log entry, DISCARDING [%.*s]: %s",
                            datalen, data, strerror(err));
                    vcl_log_err++;
                }

                if (debug)
                    LOG_Log(LOG_DEBUG, "%s: XID=%u, %s=[%.*s]", VSL_tags[tag],
                            xid, data_type == VCL_LOG_DATA ? "data" : "key",
                            datalen, data);

                if (data_type == VCL_LOG_DATA) {
                    chunks = append(de, tag, xid, data, datalen);
                    if (chunks < 0) {
                        if (debug)
                            LOG_Log(LOG_DEBUG, "Chunks exhausted, DATA "
                                    "DISCARDED: %.*s", datalen, data);
                        data_free(de);
                        return status;
                    }
                    chunks_added += chunks;
                    hasdata = 1;
                }
                else
                    addkey(de, tag, xid, data, datalen);
                break;

            case SLT_Timestamp:
                AZ(Parse_Timestamp(payload, len, &reqend_t));
                if (debug)
                    LOG_Log(LOG_DEBUG, "%s: XID=%u req_endt=%u.%06lu",
                            VSL_tags[tag], xid, (unsigned) reqend_t.tv_sec,
                            reqend_t.tv_usec);

                if (reqend_t.tv_sec > latest_t.tv_sec
                    || (reqend_t.tv_sec == latest_t.tv_sec
                        && reqend_t.tv_usec > latest_t.tv_usec))
                    memcpy(&latest_t, &reqend_t, sizeof(struct timeval));
                break;

            case SLT_VSL:
                vsl_errs++;
                LOG_Log(LOG_ERR, "VSL diagnostic XID=%u: %.*s", t->vxid,
                        len, payload);
                break;
                    
            default:
                WRONG("Unexpected tag read from the Varnish log");
            }
        }
    }

    if (!hasdata) {
        no_data++;
        data_free(de);
        return status;
    }

    if (latest_t.tv_sec == 0) {
        double t = VTIM_real();
        latest_t.tv_sec = (long) t;
        latest_t.tv_usec = (t - (double)latest_t.tv_sec) * 1e6;
        no_timestamp++;
    }
    snprintf(reqend_str, REQEND_T_LEN, "%s=%u.%06lu", REQEND_T_VAR,
             (unsigned) latest_t.tv_sec, latest_t.tv_usec);
    chunks = append(de, SLT_Timestamp, vxid, reqend_str, REQEND_T_LEN - 1);
    if (chunks < 0) {
        if (debug)
            LOG_Log(LOG_DEBUG, "Chunks exhausted, DATA DISCARDED: Tx %u", vxid);
        data_free(de);
        return status;
    }
    chunks_added += chunks;
    de->occupied = 1;
    MON_StatsUpdate(STATS_OCCUPANCY, chunks_added, 0);
    data_submit(de);
        
    if (term)
        return DISPATCH_TERMINATE;
    if (need_wrk_restart())
        return DISPATCH_WRK_RESTART;
    return status;
}

/*--------------------------------------------------------------------*/

void
CHILD_Main(int readconfig)
{
    int errnum, status = DISPATCH_CONTINUE;
    const char *errmsg;
    pthread_t monitor;
    struct passwd *pw;
    void *mqh;
    struct VSL_data *vsl;
    struct VSLQ *vslq;
    struct VSM_data *vsm = NULL;
    struct VSL_cursor *cursor;
    unsigned long last_seen = 0;
    double last_t;

    MON_StatsInit();
    debug = (LOG_GetLevel() == LOG_DEBUG);
        
    LOG_Log0(LOG_NOTICE, "Worker process starting");

    /* XXX: does not re-configure logging. Feature or bug? */
    if (readconfig) {
        LOG_Log0(LOG_NOTICE, "Re-reading config");
        CONF_Init();
        CONF_ReadDefault();
        if (! EMPTY(cli_config_filename))
            LOG_Log(LOG_NOTICE, "Reading config from %s", cli_config_filename);
            /* XXX: CONF_ReadFile prints err messages to stderr */
        if (CONF_ReadFile(cli_config_filename, CONF_Add) != 0) {
            LOG_Log(LOG_CRIT, "Error reading config from %s",
                    cli_config_filename);
            exit(EXIT_FAILURE);
        }
    }

    PRIV_Sandbox();
    pw = getpwuid(geteuid());
    AN(pw);
    LOG_Log(LOG_NOTICE, "Running as %s", pw->pw_name);

    if (debug)
        CONF_Dump(LOG_DEBUG);

    /* read messaging module */
    if (config.mq_module[0] == '\0') {
        LOG_Log0(LOG_CRIT, "mq.module not found in config (required)");
        exit(EXIT_FAILURE);
    }
    dlerror(); // to clear errors
    mqh = dlopen(config.mq_module, RTLD_NOW);
    if ((errmsg = dlerror()) != NULL) {
        LOG_Log(LOG_CRIT, "error reading mq module %s: %s", config.mq_module,
                errmsg);
        exit(EXIT_FAILURE);
    }

#define METHOD(instm, intfm)                                            \
    mqf.instm = dlsym(mqh, #intfm);                                     \
    if ((errmsg = dlerror()) != NULL) {                                 \
        LOG_Log(LOG_CRIT, "error loading mq method %s: %s", #intfm, errmsg); \
        exit(EXIT_FAILURE);                                             \
    }
#include "methods.h"
#undef METHOD

    /* install signal handlers */
    dump_action.sa_handler = dump;
    AZ(sigemptyset(&dump_action.sa_mask));
    dump_action.sa_flags |= SA_RESTART;

    terminate_action.sa_handler = term_s;
    AZ(sigemptyset(&terminate_action.sa_mask));
    terminate_action.sa_flags &= ~SA_RESTART;

    flush_action.sa_handler = sigflush;
    AZ(sigemptyset(&flush_action.sa_mask));
    flush_action.sa_flags |= SA_RESTART;

#define CHILD(SIG,disp) SIGDISP(SIG,disp)
#define PARENT(SIG,disp) ((void) 0)
#include "signals.h"
#undef PARENT
#undef CHILD

    if (DATA_Init() != 0) {
        LOG_Log(LOG_CRIT, "Cannot init data table: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    vsl = VSL_New();

    if (config.tx_limit > 0) {
        char L[sizeof("4294967296") + 1];
        bprintf(L, "%u", config.tx_limit);
        assert(VSL_Arg(vsl, 'L', L) > 0);
    }
    if (config.tx_timeout >= 0) {
        char T[DBL_MAX_10_EXP - DBL_MIN_10_EXP + 2];
        bprintf(T, "%f", config.tx_timeout);
        assert(VSL_Arg(vsl, 'T', T) > 0);
    }

    if (EMPTY(config.varnish_bindump)) {
        vsm = VSM_New();
        AN(vsm);
        if (!EMPTY(config.varnish_name)
            && VSM_n_Arg(vsm, config.varnish_name) <= 0) {
            LOG_Log(LOG_CRIT, "-n %s: %s\n", config.varnish_name,
                    VSM_Error(vsm));
            exit(EXIT_FAILURE);
        }
        else if (!EMPTY(config.vsmfile)
                 && VSM_N_Arg(vsm, config.vsmfile) <= 0) {
            LOG_Log(LOG_CRIT, "-N %s: %s\n", config.vsmfile, VSM_Error(vsm));
            exit(EXIT_FAILURE);
        }
        if (VSM_Open(vsm) < 0) {
            LOG_Log(LOG_CRIT, "Cannot attach to shared memory for instance %s: "
                    "%s", VSM_Name(vsm), VSM_Error(vsm));
            exit(EXIT_FAILURE);
        }
        cursor = VSL_CursorVSM(vsl, vsm, VSL_COPT_BATCH | VSL_COPT_TAIL);
    }
    else
        cursor = VSL_CursorFile(vsl, config.varnish_bindump, 0);
    if (cursor == NULL) {
        LOG_Log(LOG_CRIT, "Cannot open log: %s\n", VSL_Error(vsl));
        exit(EXIT_FAILURE);
    }
    vslq = VSLQ_New(vsl, &cursor, VSL_g_request, NULL);
    if (vslq == NULL) {
        LOG_Log(LOG_CRIT, "Cannot init log query: %s\n", VSL_Error(vsl));
        exit(EXIT_FAILURE);
    }

    if (!EMPTY(config.varnish_bindump))
        LOG_Log(LOG_INFO, "Reading from file: %s", config.varnish_bindump);
    else {
        if (EMPTY(VSM_Name(vsm)))
            LOG_Log0(LOG_INFO, "Reading default varnish instance");
        else
            LOG_Log(LOG_INFO, "Reading varnish instance %s", VSM_Name(vsm));
    }

    /* Log filters */
    assert(VSL_Arg(vsl, 'c', NULL) > 0);
    assert(VSL_Arg(vsl, 'i', I_TAG) > 0);
    assert(VSL_Arg(vsl, 'I', I_FILTER_VCL_LOG) > 0);
    assert(VSL_Arg(vsl, 'I', I_FILTER_TS) > 0);

    /* Start the monitor thread */
    if (config.monitor_interval > 0.0) {
        if (pthread_create(&monitor, NULL, MON_StatusThread,
                           (void *) &config.monitor_interval) != 0) {
            LOG_Log(LOG_CRIT, "Cannot start monitoring thread: %s\n",
                    strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    else
        LOG_Log0(LOG_INFO, "Monitoring thread not running");

    errmsg = mqf.global_init(config.nworkers, config.mq_config_file);
    if (errmsg != NULL) {
        LOG_Log(LOG_CRIT, "Cannot initialize message broker access: %s",
                errmsg);
        exit(EXIT_FAILURE);
    }

    errmsg = mqf.init_connections();
    if (errmsg != NULL) {
        LOG_Log(LOG_CRIT, "Cannot initialize message broker connections: %s",
                errmsg);
        exit(EXIT_FAILURE);
    }

    errnum = WRK_Init();
    if (errnum != 0) {
        LOG_Log(LOG_CRIT, "Cannot prepare worker threads: %s",
                strerror(errnum));
        exit(EXIT_FAILURE);
    }
    if ((errnum = SPMCQ_Init()) != 0) {
        LOG_Log(LOG_CRIT, "Cannot initialize internal worker queue: %s",
                strerror(errnum));
        exit(EXIT_FAILURE);
    }

    if (config.nworkers > 0) {
        WRK_Start();
        /* XXX: wrk_wait & sleep interval configurable */
        int wrk_wait = 0, wrk_running;
        while ((wrk_running = WRK_Running()) == 0) {
            if (wrk_wait++ > 10) {
                LOG_Log0(LOG_CRIT,
                         "Worker threads not starting, shutting down");
                exit(EXIT_FAILURE);
            }
            VTIM_sleep(1);
        }
        LOG_Log(LOG_INFO, "%d worker threads running", wrk_running);
    }
    else
        LOG_Log0(LOG_INFO, "Worker threads not running");
        
    /* Main loop */
    last_t = VTIM_mono();
    term = 0;
    while (!term) {
        status = VSLQ_Dispatch(vslq, dispatch, NULL);
        switch(status) {
        case DISPATCH_CONTINUE:
        case DISPATCH_WRK_RESTART:
        case DISPATCH_WRK_ABANDONED:
            break;
        case DISPATCH_EOL:
            take_free();
            eol++;
            /* re-adjust idle pause every 1024 seen txn */
            if ((seen & (~0L << 10)) > (last_seen & (~0L << 10))) {
                double t = VTIM_mono();
                idle_pause = (t - last_t) / (double) (seen - last_seen);
                last_seen = seen;
                if (idle_pause > MAX_IDLE_PAUSE)
                    idle_pause = MAX_IDLE_PAUSE;
                if (idle_pause < 1e-6)
                    idle_pause = 1e-6;
                last_t = t;
            }
            VTIM_sleep(idle_pause);
            break;
        case DISPATCH_TERMINATE:
            AN(term);
            AN(flush);
            break;
        case DISPATCH_FLUSH:
            AN(flush);
            break;
        case DISPATCH_EOF:
            term = 1;
            LOG_Log0(LOG_NOTICE, "Reached end of file");
            break;
        case DISPATCH_CLOSED:
            flush = 1;
            closed++;
            LOG_Log0(LOG_ERR, "Log was closed or abandoned");
            break;
        case DISPATCH_OVERRUN:
            flush = 1;
            overrun++;
            LOG_Log0(LOG_ERR, "Log reads were overrun");
            break;
        case DISPATCH_IOERR:
            flush = 1;
            ioerr++;
            LOG_Log(LOG_ERR, "IO error reading the log: %s (errno = %d)",
                    strerror(errno), errno);
            break;
        default:
            WRONG("Unknown return status from dispatcher");
        }

        if (status == DISPATCH_WRK_ABANDONED || all_wrk_abandoned()) {
            LOG_Log0(LOG_ALERT, "All worker threads abandoned, giving up");
            term = 1;
            flush = 0;
            break;
        }
        if (status == DISPATCH_WRK_RESTART || need_wrk_restart()) {
            if ((errnum = WRK_Restart()) != 0) {
                LOG_Log(LOG_ALERT, "Cannot restart worker threads, giving up "
                        "(%s)", strerror(errnum));
                term = 1;
                flush = 0;
                break;
            }
        }
        if (flush && !term) {
            LOG_Log0(LOG_NOTICE, "Flushing transactions");
            take_free();
            VSLQ_Flush(vslq, dispatch, NULL);
            flush = 0;
            if (EMPTY(config.varnish_bindump) && status != DISPATCH_CLOSED
                && status != DISPATCH_OVERRUN && status != DISPATCH_IOERR)
                continue;
            VSLQ_Delete(&vslq);
            AZ(vslq);
            /* cf. VUT_Main() in Varnish vut.c */
            LOG_Log0(LOG_NOTICE, "Attempting to reacquire the log");
            while (vslq == NULL) {
                AN(vsm);
                VTIM_sleep(0.1);
                if (VSM_Open(vsm)) {
                    VSM_ResetError(vsm);
                    continue;
                }
                cursor = VSL_CursorVSM(vsl, vsm,
                                       VSL_COPT_TAIL | VSL_COPT_BATCH);
                if (cursor == NULL) {
                    VSL_ResetError(vsl);
                    VSM_Close(vsm);
                    continue;
                }
                vslq = VSLQ_New(vsl, &cursor, VSL_g_request, NULL);
                AZ(cursor);
            }
            if (vslq != NULL) {
                reacquire++;
                LOG_Log0(LOG_NOTICE, "Log reacquired");
            }
        }
    }

    if (term && status != DISPATCH_EOF && flush && vslq != NULL) {
        LOG_Log0(LOG_NOTICE, "Flushing transactions");
        take_free();
        VSLQ_Flush(vslq, dispatch, NULL);
    }

    WRK_Halt();
    WRK_Shutdown();
    if ((errmsg = mqf.global_shutdown()) != NULL)
        LOG_Log(LOG_ERR, "Message queue shutdown failed: %s", errmsg);
    if (dlclose(mqh) != 0)
        LOG_Log(LOG_ERR, "Error closing mq module %s: %s", config.mq_module,
                dlerror());
    if (config.monitor_interval > 0.0)
        MON_StatusShutdown(monitor);
    LOG_Log0(LOG_NOTICE, "Worker process exiting");
    LOG_Close();
    exit(EXIT_SUCCESS);
}

#ifdef TEST_DRIVER

#include "minunit.h"

int tests_run = 0;

static char
*test_append(void)
{
    dataentry entry;
    chunk_t chunk, *c;
    char data[DEF_MAX_RECLEN - 1], result[DEF_MAX_RECLEN];

    printf("... testing data append\n");

    config.max_reclen = DEF_MAX_RECLEN;
    config.chunk_size = DEF_CHUNK_SIZE;
    config.max_records = DEF_MAX_RECORDS;
    MAZ(DATA_Init());

    entry.magic = DATA_MAGIC;
    VSTAILQ_INIT(&entry.chunks);
    chunk.magic = CHUNK_MAGIC;
    chunk.data = (char *) calloc(1, config.max_reclen);
    VSTAILQ_INSERT_TAIL(&entry.chunks, &chunk, chunklist);
    entry.curchunk = &chunk;
    entry.curchunkidx = 0;
    entry.end = 0;
    entry.occupied = 1;
    chunk.occupied = 1;
    truncated = len_overflows = len_hi = 0;
    strcpy(config.log_file, "-");
    AZ(LOG_Open("test_append"));

    for (int i = 0; i < DEF_MAX_RECLEN - 1; i++)
        data[i] = (i % 10) + '0';

    append(&entry, SLT_VCL_Log, 12345678, data, DEF_MAX_RECLEN - 1);

    MASSERT(entry.end == DEF_MAX_RECLEN);
    MASSERT(len_hi == DEF_MAX_RECLEN);
    MAZ(truncated);
    MAZ(len_overflows);

    int idx = 0;
    int n = entry.end;
    c = VSTAILQ_FIRST(&entry.chunks);
    while (n > 0) {
        CHECK_OBJ_NOTNULL(c, CHUNK_MAGIC);
        int cp = n;
        if (cp > config.chunk_size)
            cp = config.chunk_size;
        memcpy(&result[idx], c->data, cp);
        n -= cp;
        idx += cp;
        c = VSTAILQ_NEXT(c, chunklist);
    }
    MASSERT(result[0] == '&');
    MASSERT(memcmp(&result[1], data, DEF_MAX_RECLEN - 1) == 0);

    return NULL;
}

static char
*test_truncated(void)
{
    dataentry entry;
    chunk_t chunk;
    char data_with_null[8];

    printf("... testing data append with truncated data (expect an ERR)\n");

    config.max_reclen = DEF_MAX_RECLEN;
    config.chunk_size = DEF_CHUNK_SIZE;
    entry.magic = DATA_MAGIC;
    VSTAILQ_INIT(&entry.chunks);
    chunk.magic = CHUNK_MAGIC;
    chunk.data = (char *) calloc(1, config.max_reclen);
    VSTAILQ_INSERT_TAIL(&entry.chunks, &chunk, chunklist);
    entry.curchunk = &chunk;
    entry.curchunkidx = 0;
    entry.end = 0;
    entry.occupied = 1;
    chunk.occupied = 1;
    truncated = len_hi = 0;
    strcpy(config.log_file, "-");
    AZ(LOG_Open("test_append"));

    memcpy(data_with_null, "foo\0bar", 8);
    append(&entry, SLT_VCL_Log, 12345678, data_with_null, 7);

    MASSERT(memcmp(chunk.data, "&foo\0\0", 6) == 0);
    MASSERT(entry.end == 4);
    MASSERT(entry.curchunkidx == 4);
    MASSERT(truncated == 1);
    MASSERT(len_hi == 4);

    return NULL;
}

static const char
*all_tests(void)
{
    mu_run_test(test_append);
    mu_run_test(test_truncated);
    return NULL;
}

TEST_RUNNER

#endif
