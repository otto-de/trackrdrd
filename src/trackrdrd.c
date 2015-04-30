/*-
 * Copyright (c) 2012-2014 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012-2014 Otto Gmbh & Co KG
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
#include <stdarg.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pwd.h>

#include "vas.h"

#include "trackrdrd.h"
#include "config_common.h"
#include "vcs_version.h"
#include "usage.h"
#include "vpf.h"
#include "vtim.h"

static volatile sig_atomic_t term = 0, reload = 0;

static struct sigaction terminate_action, restart_action;

static const char *version = PACKAGE_STRING " revision " VCS_Version
    " branch " VCS_Branch;

/*--------------------------------------------------------------------*/

static void
restart(int sig)
{
    LOG_Log(LOG_NOTICE, "Management process received signal %d (%s), "
            "will restart child process with new config", sig, strsignal(sig));
    reload = 1;
}

static void
term_s(int sig)
{
    LOG_Log(LOG_NOTICE, "Management process received signal %d (%s), "
            "will terminate management and worker", sig, strsignal(sig));
    term = 1;
}

/*--------------------------------------------------------------------*/

/* Handle for the PID file */
struct vpf_fh *pfh = NULL;

static void
parent_shutdown(int status, pid_t child_pid)
{
    if (child_pid && kill(child_pid, SIGTERM) != 0) {
        LOG_Log(LOG_ERR, "Cannot kill child process %d: %s", child_pid,
            strerror(errno));
        status = EXIT_FAILURE;
    }

    /* Remove PID file if necessary */
    if (pfh != NULL)
        VPF_Remove(pfh);

    LOG_Log0(LOG_INFO, "Management process exiting");
    LOG_Close();
    exit(status);
}

static pid_t
child_restart(pid_t child_pid, int readconfig)
{
    int errnum;
    
    if (readconfig) {
        LOG_Log(LOG_NOTICE, "Sending TERM signal to worker process %d",
                child_pid);
        if ((errnum = kill(child_pid, SIGTERM)) != 0) {
            LOG_Log(LOG_ALERT, "Signal TERM delivery to process %d failed: %s",
                    strerror(errnum));
            parent_shutdown(EXIT_FAILURE, 0);
        }
    }
    LOG_Log0(LOG_NOTICE, "Restarting child process");
    child_pid = fork();
    if (child_pid == -1) {
        LOG_Log(LOG_ALERT, "Cannot fork: %s", strerror(errno));
        parent_shutdown(EXIT_FAILURE, child_pid);
    }
    else if (child_pid == 0)
        CHILD_Main(readconfig);

    return child_pid;
}   

static void
parent_main(pid_t child_pid)
{
    int restarts = 0, status;
    pid_t wpid;

    LOG_Log0(LOG_NOTICE, "Management process starting");

    restart_action.sa_handler = restart;
    AZ(sigemptyset(&restart_action.sa_mask));
    restart_action.sa_flags &= ~SA_RESTART;

    terminate_action.sa_handler = term_s;
    AZ(sigemptyset(&terminate_action.sa_mask));
    terminate_action.sa_flags &= ~SA_RESTART;

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
                    child_pid = child_restart(child_pid, reload);
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
            LOG_Log(LOG_ALERT, "Cannot wait for worker processes: %s",
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
            LOG_Log(LOG_ALERT, "Too many restarts: %d", restarts);
            parent_shutdown(EXIT_FAILURE, 0);
        }
        
        if (config.restart_pause > 0) {
            LOG_Log(LOG_NOTICE, "Pausing %u seconds before restarting child",
                    config.restart_pause);
            VTIM_sleep(config.restart_pause);
        }
        child_pid = child_restart(child_pid, 0);
        restarts++;
    }
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
    int c, d_flag = 0, D_flag = 0, err;
    const char *P_arg = NULL, *l_arg = NULL, *n_arg = NULL, *f_arg = NULL,
        *y_arg = NULL, *c_arg = NULL, *u_arg = NULL;
    pid_t child_pid;

    CONF_Init();
    if ((err = CONF_ReadDefault()) != 0) {
        if (err != -1)
            LOG_Log(LOG_ALERT, "Cannot read %s: %s", DEFAULT_CONFIG,
                strerror(err));
        exit(EXIT_FAILURE);
    }
    cli_config_filename[0] = '\0';

    while ((c = getopt(argc, argv, "u:P:Vn:hl:df:y:c:D")) != -1) {
        switch (c) {
        case 'P':
            P_arg = optarg;
            break;
        case 'V':
            fprintf(stderr, "%s\n", version);
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
        if (CONF_ReadFile(c_arg, CONF_Add) != 0)
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
    }
        
    if (LOG_Open(PACKAGE_NAME) != 0) {
        exit(EXIT_FAILURE);
    }

    if (d_flag)
        LOG_SetLevel(LOG_DEBUG);
    LOG_Log(LOG_INFO, "initializing (%s)", version);

    CONF_Dump(LOG_DEBUG);
        
    if (!EMPTY(config.pid_file)
        && (pfh = VPF_Open(config.pid_file, 0644, NULL)) == NULL) {
        LOG_Log(LOG_CRIT, "Cannot write pid file %s: %s\n",
                config.pid_file, strerror(errno));
        exit(EXIT_FAILURE);
    }

#ifdef HAVE_DAEMON
    if (!D_flag && daemon(0, 0) == -1) {
        perror("daemon()");
        if (pfh != NULL)
            VPF_Remove(pfh);
        exit(EXIT_FAILURE);
    }
#endif

    if (pfh != NULL)
        VPF_Write(pfh);

    stacktrace_action.sa_handler = HNDL_Abort;

    ignore_action.sa_handler = SIG_IGN;
    default_action.sa_handler = SIG_DFL;

    HNDL_Init(argv[0]);

    if (!D_flag) {
        child_pid = fork();
        switch(child_pid) {
        case -1:
            LOG_Log(LOG_ERR,
                "Cannot fork (%s), running as single process",
                strerror(errno));
            CHILD_Main(0);
            break;
        case 0:
            CHILD_Main(0);
            break;
        default:
            parent_main(child_pid);
            break;
        }
    }
    else {
        LOG_Log0(LOG_NOTICE, "Running as single process");
        CHILD_Main(0);
    }
}
