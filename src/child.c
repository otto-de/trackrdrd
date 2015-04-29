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

#include "trackrdrd.h"
#include "config_common.h"
#include "vcs_version.h"
#include "vtim.h"

#include "vapi/vsl.h"
#include "miniobj.h"
#include "vas.h"

#define QUERY "VCL_log ~ \"^track \""
#define I_TAG "VSL"
#define I_FILTER_VCL_LOG "VCL_log:^track "
#define I_FILTER_TS "Timestamp:^Resp"

/* XXX: should these be configurable ? */
#define TRACKLOG_PREFIX "track "
#define TRACKLOG_PREFIX_LEN (sizeof(TRACKLOG_PREFIX)-1)
#define REQEND_T_VAR "req_endt"
#define REQEND_T_LEN (sizeof(REQEND_T_VAR "=1430176881.682097")-1)

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

const char *version = PACKAGE_TARNAME "-" PACKAGE_VERSION " revision " \
    VCS_Version " branch " VCS_Branch;

static unsigned len_hi = 0, debug = 0, data_exhausted = 0;
    // chunk_exhausted = 0;

static unsigned long seen = 0, submitted = 0, len_overflows = 0, no_data = 0,
    no_free_data = 0, vcl_log_err = 0, vsl_errs = 0, closed = 0, overrun = 0,
    ioerr = 0, reacquire = 0, truncated = 0, key_hi = 0, key_overflows = 0;
// no_free_chunk = 0;

static volatile sig_atomic_t flush = 0;

static struct sigaction dump_action;

/* Local freelist */
static struct freehead_s reader_freelist = 
    VSTAILQ_HEAD_INITIALIZER(reader_freelist);
static unsigned rdr_data_free = 0;

/*--------------------------------------------------------------------*/

void
RDR_Stats(void)
{
    LOG_Log(LOG_INFO, "Reader: seen=%lu submitted=%lu nodata=%lu free=%u "
            "no_free_rec=%lu len_hi=%u key_hi=%lu len_overflows=%lu "
            "truncated=%lu key_overflows=%lu vcl_log_err=%lu vsl_err=%lu "
            "closed=%lu overrun=%lu ioerr=%lu reacquire=%lu",
            seen, submitted, no_data, rdr_data_free, no_free_data, len_hi,
            key_hi, len_overflows, truncated, key_overflows, vcl_log_err,
            vsl_errs, closed, overrun, ioerr, reacquire);
}

/*--------------------------------------------------------------------*/

static void
dump(int sig)
{
    LOG_Log(LOG_NOTICE, "Received signal %d (%s), "
            "dumping config and data table", sig, strsignal(sig));
    CONF_Dump();
    DATA_Dump();
}

/*--------------------------------------------------------------------*/

/* efficiently retrieve a single data entry */

static inline dataentry
*data_get(void)
{
    dataentry *data;

    while (VSTAILQ_EMPTY(&reader_freelist)) {
        spmcq_signal(data);
        rdr_data_free = DATA_Take_Freelist(&reader_freelist);
        if (VSTAILQ_EMPTY(&reader_freelist)) {
            data_exhausted = 1;
            return NULL;
        }
        if (debug)
            LOG_Log(LOG_DEBUG, "Reader: took %u free data entries",
                    rdr_data_free);
    }
    data_exhausted = 0;
    data = VSTAILQ_FIRST(&reader_freelist);
    VSTAILQ_REMOVE_HEAD(&reader_freelist, freelist);
    rdr_data_free--;
    return (data);
}

/* return to our own local cache */

static inline void
data_free(dataentry *de)
{
    AN(de);
    assert(!OCCUPIED(de));
    VSTAILQ_INSERT_HEAD(&reader_freelist, de, freelist);
}

static inline void
data_submit(dataentry *de)
{
    int wrk_running;

    CHECK_OBJ_NOTNULL(de, DATA_MAGIC);
    assert(OCCUPIED(de));
    assert(de->hasdata);
    AZ(memchr(de->data, '\0', de->end));
    if (debug)
        LOG_Log(LOG_DEBUG, "submit: data=[%.*s]", de->end, de->data);

    SPMCQ_Enq(de);
    submitted++;

    /* should we wake up another worker? */
    wrk_running = WRK_Running();
    if (SPMCQ_NeedWorker(wrk_running))
        spmcq_signal(data);

    /*
     * base case: wake up a worker if all are sleeping
     *
     * this is an un-synced access to spmcq_data_waiter, but
     * if we don't wake them up now, we will next time around
     */
    if (wrk_running == spmcq_datawaiter)
        spmcq_signal(data);
}

static inline void
take_free(void)
{
    rdr_data_free += DATA_Take_Freelist(&reader_freelist);
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

static void
append(dataentry *entry, enum VSL_tag_e tag, unsigned xid, char *data,
       int datalen)
{
    char *null;

    CHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
    /* Data overflow */
    if (entry->end + datalen + 1 > config.maxdata) {
        LOG_Log(LOG_ERR, "%s: Data too long, XID=%d, current length=%d, "
            "DISCARDING data=[%.*s]", VSL_tags[tag], xid, entry->end,
            datalen, data);
        len_overflows++;
        return;
    }
    /* Null chars in the payload means that the data was truncated in the
       log, due to exceeding shm_reclen. */
    if ((null = memchr(data, '\0', datalen)) != NULL) {
        datalen = null - data;
        LOG_Log(LOG_ERR, "%s: Data truncated in SHM log, XID=%d, data=[%.*s]",
                VSL_tags[tag], xid, datalen, data);
        truncated++;
    }
        
    entry->data[entry->end] = '&';
    entry->end++;
    memcpy(&entry->data[entry->end], data, datalen);
    entry->end += datalen;
    if (entry->end > len_hi)
        len_hi = entry->end;
    return;
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
    int status = DISPATCH_RETURN_OK;
    dataentry *de = NULL;
    char reqend_str[REQEND_T_LEN];
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
    AZ(de->reqend_t.tv_sec);
    AZ(de->reqend_t.tv_usec);
    de->hasdata = 0;
    seen++;

    for (struct VSL_transaction *t = pt[0]; t != NULL; t = *++pt) {
        if (debug)
            LOG_Log(LOG_DEBUG, "Reader read tx: [%u]", t->vxid);

        while ((status = VSL_Next(t->c)) > 0) {
            int len, err;
            const char *payload;
            unsigned xid;
            enum VSL_tag_e tag;

            if (!VSL_Match(vsl, t->c))
                continue;

            assert(t->type == VSL_t_req);
            assert(VSL_CLIENT(t->c->rec.ptr));

            if (t == pt[0]) {
                de->xid = t->vxid;
                snprintf(de->data, config.maxdata, "XID=%u", t->vxid);
                de->end = strlen(de->data);
                if (de->end > len_hi)
                    len_hi = de->end;
            }

            len = VSL_LEN(t->c->rec.ptr);
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
                    append(de, tag, xid, data, datalen);
                    de->hasdata = 1;
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

                if (reqend_t.tv_sec > de->reqend_t.tv_sec
                    || reqend_t.tv_usec > de->reqend_t.tv_usec)
                    memcpy(&de->reqend_t, &reqend_t, sizeof(struct timeval));
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

    if (!de->hasdata) {
        no_data++;
        data_free(de);
        return status;
    }

    snprintf(reqend_str, REQEND_T_LEN, "%s=%u.%06lu", REQEND_T_VAR,
             (unsigned) de->reqend_t.tv_sec, de->reqend_t.tv_usec);
    append(de, SLT_Timestamp, de->xid, reqend_str, REQEND_T_LEN);
    de->occupied = 1;
    MON_StatsUpdate(STATS_OCCUPANCY);
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
    int errnum, status;
    const char *errmsg;
    pthread_t monitor;
    struct passwd *pw;
    void *mqh;
    struct VSL_data *vsl;
    struct VSLQ *vslq;
    struct VSM_data *vsm = NULL;
    struct VSL_cursor *cursor;

    MON_StatsInit();
        
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
    vslq = VSLQ_New(vsl, &cursor, VSL_g_request, QUERY);
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
            VTIM_sleep(config.idle_pause);
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
        if (flush) {
            LOG_Log0(LOG_NOTICE, "Flushing transactions");
            take_free();
            VSLQ_Flush(vslq, dispatch, NULL);
            flush = 0;
            if (!term && EMPTY(config.varnish_bindump) &&
                status != DISPATCH_CLOSED && status != DISPATCH_OVERRUN
                && status != DISPATCH_IOERR)
                continue;
            VSLQ_Delete(&vslq);
            AZ(vslq);
            /* cf. VUT_Main() in Varnish vut.c */
            LOG_Log0(LOG_NOTICE, "Attempting to reacquire the log");
            while (!term && vslq == NULL) {
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
                vslq = VSLQ_New(vsl, &cursor, VSL_g_request, QUERY);
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
    dataentry *entry;
    char data_with_null[8];

    printf("... testing data append (expect an ERR)\n");

    config.maxdata = DEF_MAXDATA;
    entry = calloc(1, sizeof(dataentry));
    AN(entry);
    entry->data = calloc(1, config.maxdata);
    AN(entry->data);
    entry->magic = DATA_MAGIC;
    truncated = len_hi = 0;
    strcpy(config.log_file, "-");
    AZ(LOG_Open("test_append"));

    memcpy(data_with_null, "foo\0bar", 8);
    append(entry, SLT_VCL_Log, 12345678, data_with_null, 7);

    MASSERT(memcmp(entry->data, "&foo\0\0", 6) == 0);
    MASSERT(entry->end == 4);
    MASSERT(truncated == 1);
    MASSERT(len_hi == 4);

    return NULL;
}

static const char
*all_tests(void)
{
    mu_run_test(test_append);
    return NULL;
}

TEST_RUNNER

#endif
