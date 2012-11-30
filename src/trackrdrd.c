/*-
 * Copyright (c) 2012 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012 Otto Gmbh & Co KG
 * All rights reserved
 * Use only with permission
 *
 * Author: Geoffrey Simmons <geoffrey.simmons@uplex.de>
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
#include <stdarg.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pwd.h>

#ifndef HAVE_EXECINFO_H
#include "compat/execinfo.h"
#else
#include <execinfo.h>
#endif
#include "compat/daemon.h"

#include "vsb.h"
#include "vpf.h"

#include "libvarnish.h"
#include "vsl.h"
#include "varnishapi.h"
#include "miniobj.h"

#include "trackrdrd.h"
#include "revision.h"
#include "usage.h"

#define TRACK_TAGS "ReqStart,VCL_log,ReqEnd"
#define TRACKLOG_PREFIX "track "
#define TRACKLOG_PREFIX_LEN (sizeof(TRACKLOG_PREFIX)-1)

#define DEFAULT_CONFIG "/etc/trackrdrd.conf"

/* XXX: should these be configurable ? */
#define MAX_STACK_DEPTH 100
#define REQEND_T_VAR "req_endt"

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

static void child_main(struct VSM_data *vd, int endless, int readconfig);

static volatile sig_atomic_t term, reload;

static struct sigaction terminate_action, dump_action, ignore_action,
    stacktrace_action, default_action, restart_action;

static char cli_config_filename[BUFSIZ] = "";

/*--------------------------------------------------------------------*/

static void
submit(unsigned xid)
{
    dataentry *entry;
    
    entry = DATA_Find(xid);
    CHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
    assert(entry->state == DATA_DONE);
    LOG_Log(LOG_DEBUG, "submit: data=[%.*s]", entry->end, entry->data);
    
    if (! entry->hasdata) {
        entry->state = DATA_EMPTY;
        MON_StatsUpdate(STATS_NODATA);
        return;
    }
    while (!SPMCQ_Enq((void *) entry)) {
        tbl.wait_qfull++;
        LOG_Log(LOG_ALERT, "%s", "Internal queue full, waiting for dequeue");
        AZ(pthread_mutex_lock(&spmcq_nonfull_lock));
        AZ(pthread_cond_wait(&spmcq_nonfull_cond, &spmcq_nonempty_lock));
    }
    AZ(pthread_cond_signal(&spmcq_nonempty_cond));
    tbl.submitted++;
}

static inline dataentry
*insert(unsigned xid, enum VSL_tag_e tag, unsigned fd)
{
    dataentry *entry;
    
    entry = DATA_Insert(xid);
    if (entry == NULL) {
        LOG_Log(LOG_ALERT,
            "%s: Cannot insert data, XID=%d tid=%d DISCARDED",
            VSL_tags[tag], xid, fd);
        return NULL;
    }
    CHECK_OBJ(entry, DATA_MAGIC);
        
    entry->state = DATA_OPEN;
    entry->xid = xid;
    entry->tid = fd;
    entry->hasdata = false;
    sprintf(entry->data, "XID=%d", xid);
    entry->end = strlen(entry->data);
    if (entry->end > tbl.data_hi)
        tbl.data_hi = entry->end;
    tbl.seen++;
    MON_StatsUpdate(STATS_OCCUPANCY);
    
    return entry;
}

static inline dataentry
*find_or_insert(unsigned xid, enum VSL_tag_e tag, unsigned fd)
{
    dataentry *entry;

    entry = DATA_Find(xid);
    if (entry == NULL) {
        if (term)
            return NULL;
        LOG_Log(LOG_WARNING, "%s: XID %d not found, attempting insert",
            VSL_tags[tag], xid);
        entry = insert(xid, tag, fd);
        if (entry == NULL)
            return NULL;
    }
    else {
        CHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
        assert(entry->xid == xid);
        assert(entry->tid == fd);
        assert(entry->state == DATA_OPEN);
    }

    return entry;
}

static inline void
append(dataentry *entry, enum VSL_tag_e tag, unsigned xid, char *data,
    int datalen)
{
    /* Data overflow */
    /* XXX: Encapsulate (1 << (config.maxdata_scale+10)) */
    if (entry->end + datalen + 1 > (1 << (config.maxdata_scale+10))) {
        LOG_Log(LOG_ALERT,
            "%s: Data too long, XID=%d, current length=%d, "
            "DISCARDING data=[%.*s]", VSL_tags[tag], xid, entry->end,
            datalen, data);
        tbl.data_overflows++;
        return;
    }
        
    entry->data[entry->end] = '&';
    entry->end++;
    memcpy(&entry->data[entry->end], data, datalen);
    entry->end += datalen;
    if (entry->end > tbl.data_hi)
        tbl.data_hi = entry->end;
    return;
}

static int
OSL_Track(void *priv, enum VSL_tag_e tag, unsigned fd, unsigned len,
          unsigned spec, const char *ptr, uint64_t bitmap)
{
    unsigned xid;
    dataentry *entry;
    int err, datalen;
    char *data, reqend_str[strlen(REQEND_T_VAR)+22];
    struct timespec reqend_t;

    (void) priv;
    (void) bitmap;

    if (term && tbl.open == 0)
        return 1;
    
    /* spec != 'c' */
    if ((spec & VSL_S_CLIENT) == 0)
        LOG_Log(LOG_WARNING, "%s: Client bit ('c') not set", VSL_tags[tag]);
    
    switch (tag) {
    case SLT_ReqStart:

        if (term) return(0);

        err = Parse_ReqStart(ptr, len, &xid);
        AZ(err);
        LOG_Log(LOG_DEBUG, "%s: XID=%d", VSL_tags[tag], xid);

        (void) insert(xid, tag, fd);
        break;

    case SLT_VCL_Log:
        /* Skip VCL_Log entries without the "track " prefix. */
        if (strncmp(ptr, TRACKLOG_PREFIX, TRACKLOG_PREFIX_LEN) != 0)
            break;
        
        err = Parse_VCL_Log(&ptr[TRACKLOG_PREFIX_LEN], len-TRACKLOG_PREFIX_LEN,
                            &xid, &data, &datalen);
        AZ(err);
        LOG_Log(LOG_DEBUG, "%s: XID=%d, data=[%.*s]", VSL_tags[tag],
            xid, datalen, data);
        
        entry = find_or_insert(xid, tag, fd);
        if (entry == NULL)
            break;

        append(entry, tag, xid, data, datalen);
        entry->hasdata = true;
        break;

    case SLT_ReqEnd:

        err = Parse_ReqEnd(ptr, len, &xid, &reqend_t);
        AZ(err);
        LOG_Log(LOG_DEBUG, "%s: XID=%d req_endt=%u.%09lu", VSL_tags[tag], xid,
            (unsigned) reqend_t.tv_sec, reqend_t.tv_nsec);

        entry = find_or_insert(xid, tag, fd);
        if (entry == NULL)
            break;
        sprintf(reqend_str, "%s=%u.%09lu", REQEND_T_VAR,
            (unsigned) reqend_t.tv_sec, reqend_t.tv_nsec);
        append(entry, tag, xid, reqend_str, strlen(reqend_str));
        entry->state = DATA_DONE;
        MON_StatsUpdate(STATS_DONE);
        submit(xid);
        break;

    default:
        /* Unreachable */
        AN(NULL);
        return(1);
    }
    return(0);
}

/*--------------------------------------------------------------------*/

/* Handle for the PID file */
struct vpf_fh *pfh = NULL;

static void
read_default_config(void) {
    if (access(DEFAULT_CONFIG, F_OK) == 0) {
        if (access(DEFAULT_CONFIG, R_OK) != 0) {
            perror(DEFAULT_CONFIG);
            exit(EXIT_FAILURE);
        }
        printf("Reading config from %s\n", DEFAULT_CONFIG);
        if (CONF_ReadFile(DEFAULT_CONFIG) != 0)
            exit(EXIT_FAILURE);
    }
}

static void
assert_failure(const char *func, const char *file, int line, const char *cond,
    int err, int xxx)
{
    (void) xxx;
    
    LOG_Log(LOG_ALERT, "Condition (%s) failed in %s(), %s line %d",
        cond, func, file, line);
    if (err)
        LOG_Log(LOG_ALERT, "errno = %d (%s)", err, strerror(err));
    abort();
}

static void
stacktrace(void)
{
    void *buf[MAX_STACK_DEPTH];
    int depth, i;
    char **strings;

    depth = backtrace (buf, MAX_STACK_DEPTH);
    if (depth == 0) {
	LOG_Log0(LOG_ERR, "Stacktrace empty");
	return;
    }
    strings = backtrace_symbols(buf, depth);
    if (strings == NULL) {
	LOG_Log0(LOG_ERR, "Cannot retrieve symbols for stacktrace");
	return;
    }
    /* XXX: get symbol names from nm? cf. cache_panic.c/pan_backtrace */
    for (i = 0; i < depth; i++)
	LOG_Log(LOG_ERR, "%s", strings[i]);
    
    free(strings);
}

static void
dump(int sig)
{
    (void) sig;
    DATA_Dump();
}

static void
terminate(int sig)
{
    (void) sig;
    term = 1;
}

static void
restart(int sig)
{
    (void) sig;
    reload = 1;
}

static void
stacktrace_abort(int sig)
{
    LOG_Log(LOG_ALERT, "Received signal %d (%s), stacktrace follows", sig,
	strsignal(sig));
    stacktrace();
    AZ(sigaction(SIGABRT, &default_action, NULL));
    LOG_Log0(LOG_ALERT, "Aborting");
    abort();
}

static void
parent_shutdown(int status, pid_t child_pid)
{
    if (child_pid && kill(child_pid, SIGTERM) != 0) {
        LOG_Log(LOG_ERR, "Cannot kill child process %d: %s", child_pid,
            strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Remove PID file if necessary */
    if (pfh != NULL)
        VPF_Remove(pfh);

    LOG_Log0(LOG_INFO, "Management process exiting");
    LOG_Close();
    exit(status);
}

static pid_t
child_restart(pid_t child_pid, struct VSM_data *vd, int endless, int readconfig)
{
    int errnum;
    
    if (readconfig) {
        LOG_Log(LOG_INFO, "Sending TERM signal to worker process %d",
            child_pid);
        if ((errnum = kill(child_pid, SIGTERM)) != 0) {
            LOG_Log(LOG_ALERT, "Signal TERM delivery to process %d failed: %s",
                strerror(errnum));
            parent_shutdown(EXIT_FAILURE, 0);
        }
    }
    LOG_Log0(LOG_INFO, "Restarting child process");
    child_pid = fork();
    if (child_pid == -1) {
        LOG_Log(LOG_ALERT, "Cannot fork: %s", strerror(errno));
        parent_shutdown(EXIT_FAILURE, child_pid);
    }
    else if (child_pid == 0)
        child_main(vd, endless, readconfig);

    return child_pid;
}   

static void
parent_main(pid_t child_pid, struct VSM_data *vd, int endless)
{
    int restarts = 0, status;
    pid_t wpid;

    LOG_Log0(LOG_INFO, "Management process starting");
    
    term = 0;
    reload = 0;
    /* install signal handlers */
#define PARENT(SIG,disp) SIGDISP(SIG,disp)
#define CHILD(SIG,disp) ((void) 0)
#include "signals.h"
#undef PARENT
#undef CHILD
    
    while (!term) {
        wpid = wait(&status);
        if (wpid == -1) {
            if (errno == EINTR) {
                if (term)
                    parent_shutdown(EXIT_SUCCESS, child_pid);
                else if (reload) {
                    child_pid = child_restart(child_pid, vd, endless, reload);
                    reload = 0;
                    continue;
                }
                else {
                    LOG_Log0(LOG_WARNING,
                        "Interrupted while waiting for worker process, "
                        "continuing");
                    continue;
                }
            }
            LOG_Log(LOG_ERR, "Cannot wait for worker processes: %s",
                strerror(errno));
            parent_shutdown(EXIT_FAILURE, child_pid);
        }
        AZ(WIFSTOPPED(status));
        AZ(WIFCONTINUED(status));
        if (WIFEXITED(status))
            LOG_Log(LOG_WARNING, "Worker process %d exited with status %d",
                wpid, WEXITSTATUS(status));
        if (WIFSIGNALED(status))
            LOG_Log(LOG_WARNING,
		"Worker process %d exited due to signal %d (%s)",
                wpid, WTERMSIG(status), strsignal(WTERMSIG(status)));

        if (wpid != child_pid)
            continue;
        
        if (config.restarts && restarts > config.restarts) {
            LOG_Log(LOG_ERR, "Too many restarts: %d", restarts);
            parent_shutdown(EXIT_FAILURE, 0);
        }
        
        child_pid = child_restart(child_pid, vd, endless, 0);
        restarts++;
    }
}

/* Matches typedef VSM_diag_f in include/varnishapi.h
   Log error messages from VSL_Open and VSL_Arg */
static void
vsl_diag(void *priv, const char *fmt, ...)
{
    (void) priv;
    
    va_list ap;
    va_start(ap, fmt);
    logconf.log(LOG_ERR, fmt, ap);
    va_end(ap);
}

static void
child_main(struct VSM_data *vd, int endless, int readconfig)
{
    int errnum;
    const char *errmsg;
    pthread_t monitor;
    struct passwd *pw;

    LOG_Log0(LOG_INFO, "Worker process starting");

    /* XXX: does not re-configure logging. Feature or bug? */
    if (readconfig) {
        LOG_Log0(LOG_INFO, "Re-reading config");
        CONF_Init();
        read_default_config();
        if (! EMPTY(cli_config_filename))
            LOG_Log(LOG_INFO, "Reading config from %s", cli_config_filename);
            /* XXX: CONF_ReadFile prints err messages to stderr */
            if (CONF_ReadFile(cli_config_filename) != 0) {
                LOG_Log(LOG_ERR, "Error reading config from %s",
                    cli_config_filename);
                exit(EXIT_FAILURE);
            }
    }
    
    PRIV_Sandbox();
    pw = getpwuid(geteuid());
    AN(pw);
    LOG_Log(LOG_INFO, "Running as %s", pw->pw_name);

    /* install signal handlers */
#define CHILD(SIG,disp) SIGDISP(SIG,disp)
#define PARENT(SIG,disp) ((void) 0)
#include "signals.h"
#undef PARENT
#undef CHILD

    if (DATA_Init() != 0) {
        LOG_Log(LOG_ERR, "Cannot init data table: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    VSM_Diag(vd, vsl_diag, NULL);
    if (VSL_Open(vd, 1))
        exit(EXIT_FAILURE);

    /* Only read the VSL tags relevant to tracking */
    assert(VSL_Arg(vd, 'i', TRACK_TAGS) > 0);

    /* Start the monitor thread */
    if (config.monitor_interval > 0.0) {
        if (pthread_create(&monitor, NULL, MON_StatusThread,
                (void *) &config.monitor_interval) != 0) {
            LOG_Log(LOG_ERR, "Cannot start monitoring thread: %s\n",
                strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    else
        LOG_Log0(LOG_INFO, "Monitoring thread not running");

    errmsg = MQ_GlobalInit();
    if (errmsg != NULL) {
        LOG_Log(LOG_ERR, "Cannot initialize message broker access: %s", errmsg);
        exit(EXIT_FAILURE);
    }

    errnum = WRK_Init();
    if (errnum != 0) {
        LOG_Log(LOG_ERR, "Cannot prepare worker threads: %s",
            strerror(errnum));
        exit(EXIT_FAILURE);
    }

    if ((errnum = SPMCQ_Init()) != 0) {
        LOG_Log(LOG_ERR, "Cannot initialize internal worker queue: %s",
            strerror(errnum));
        exit(EXIT_FAILURE);
    }

    MON_StatsInit();
        
    /* Start worker threads */
    WRK_Start();
        
    /* Main loop */
    term = 0;
    /* XXX: Varnish restart? */
    /* XXX: TERM not noticed until request received */
    while (VSL_Dispatch(vd, OSL_Track, NULL) > 0)
        if (term || !endless)
            break;
        else {
            LOG_Log0(LOG_WARNING, "Log read interrupted, continuing");
            continue;
        }

    WRK_Halt();
    WRK_Shutdown();
    AZ(MQ_GlobalShutdown());
    if (config.monitor_interval > 0.0)
	MON_StatusShutdown(monitor);
    LOG_Log0(LOG_INFO, "Worker process exiting");
    LOG_Close();
    exit(EXIT_SUCCESS);
}

static void
usage(int status)
{
    fprintf(stderr, "Usage:\n%s\n%s\n", synopsis, options);
    exit(status);
}

int
main(int argc, char * const *argv)
{
    	int c, d_flag = 0, D_flag = 0, endless = 1, err;
	const char *P_arg = NULL, *l_arg = NULL, *n_arg = NULL, *f_arg = NULL,
            *y_arg = NULL, *c_arg = NULL, *u_arg = NULL;
	struct VSM_data *vd;
        pid_t child_pid;

	vd = VSM_New();
	VSL_Setup(vd);

        CONF_Init();
        read_default_config();

	while ((c = getopt(argc, argv, "u:P:Vn:hl:df:y:c:D")) != -1) {
		switch (c) {
		case 'P':
                    P_arg = optarg;
                    break;
		case 'V':
                    printf(PACKAGE_STRING " revision " REVISION "\n");
                    exit(EXIT_SUCCESS);
                case 'n':
                    n_arg = optarg;
                    break;
                case 'l':
                    l_arg = optarg;
                    break;
                case 'd':
                    d_flag = 1;
                    break;
                case 'f':
                    f_arg = optarg;
                    break;
                case 'y':
                    y_arg = optarg;
                    break;
                case 'c':
                    c_arg = optarg;
                    break;
                case 'D':
                    D_flag = 1;
                    break;
                case 'u':
                    u_arg = optarg;
                    break;
                case 'h':
                    usage(EXIT_SUCCESS);
		default:
                    usage(EXIT_FAILURE);
		}
	}

	if ((argc - optind) > 0)
            usage(EXIT_FAILURE);

        if (c_arg) {
            strcpy(cli_config_filename, c_arg);
            printf("Reading config from %s\n", c_arg);
            if (CONF_ReadFile(c_arg) != 0)
                exit(EXIT_FAILURE);
        }
        
        if (f_arg && n_arg)
            usage(EXIT_FAILURE);
        if (l_arg && y_arg)
            usage(EXIT_FAILURE);
        
        if (u_arg) {
            err = CONF_Add("user", u_arg);
            if (err) {
                fprintf(stderr, "Unknown user: %s\n", u_arg);
                exit(EXIT_FAILURE);
            }
        }

        if (y_arg) {
            err = CONF_Add("syslog.facility", y_arg);
            if (err) {
                fprintf(stderr, "Unknown syslog facility: %s\n", y_arg);
                exit(EXIT_FAILURE);
            }
        }
        
        if (P_arg)
            strcpy(config.pid_file, P_arg);
        if (n_arg)
            strcpy(config.varnish_name, n_arg);
        if (l_arg)
            strcpy(config.log_file, l_arg);
        if (f_arg) {
            strcpy(config.varnish_bindump, f_arg);
            endless = 0;
        }
        
        if (f_arg && VSL_Arg(vd, 'r', f_arg) <= 0)
            exit(EXIT_FAILURE);
        else if (!EMPTY(config.varnish_name)
                 && VSL_Arg(vd, 'n', config.varnish_name) <= 0)
            exit(EXIT_FAILURE);
        
        if (LOG_Open(PACKAGE_NAME) != 0) {
            exit(EXIT_FAILURE);
        }

        VAS_Fail = assert_failure;
        
        if (d_flag)
            LOG_SetLevel(LOG_DEBUG);
        LOG_Log0(LOG_INFO,
            "initializing (v" PACKAGE_VERSION " revision " REVISION ")");

        CONF_Dump();
        
	if (!EMPTY(config.pid_file)
            && (pfh = VPF_Open(config.pid_file, 0644, NULL)) == NULL) {
		LOG_Log(LOG_ERR, "Cannot write pid file %s: %s\n",
                        config.pid_file, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (!D_flag && varnish_daemon(0, 0) == -1) {
		perror("daemon()");
		if (pfh != NULL)
			VPF_Remove(pfh);
		exit(EXIT_FAILURE);
	}

	if (pfh != NULL)
		VPF_Write(pfh);

	terminate_action.sa_handler = terminate;
	AZ(sigemptyset(&terminate_action.sa_mask));
	terminate_action.sa_flags &= ~SA_RESTART;

	dump_action.sa_handler = dump;
	AZ(sigemptyset(&dump_action.sa_mask));
	dump_action.sa_flags |= SA_RESTART;

	restart_action.sa_handler = restart;
	AZ(sigemptyset(&restart_action.sa_mask));
	restart_action.sa_flags &= ~SA_RESTART;

	stacktrace_action.sa_handler = stacktrace_abort;

	ignore_action.sa_handler = SIG_IGN;
	default_action.sa_handler = SIG_DFL;

        if (!D_flag) {
            child_pid = fork();
            switch(child_pid) {
            case -1:
                LOG_Log(LOG_ALERT,
                    "Cannot fork (%s), running as single process",
                    strerror(errno));
                child_main(vd, endless, 0);
                break;
            case 0:
                child_main(vd, endless, 0);
                break;
            default:
                parent_main(child_pid, vd, endless);
                break;
            }
        }
        else {
            LOG_Log0(LOG_INFO, "Running as non-demon single process");
            child_main(vd, endless, 0);
        }
}
