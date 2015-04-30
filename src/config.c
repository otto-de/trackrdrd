/*-
 * Copyright (c) 2012-2014 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012-2014 Otto Gmbh & Co KG
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
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <syslog.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <unistd.h>
#include <pwd.h>
#include <stdbool.h>

#include "trackrdrd.h"
#include "config_common.h"

#include "vas.h"

#define DEFAULT_USER "nobody"

static const int facilitynum[8] =
    { LOG_LOCAL0, LOG_LOCAL1, LOG_LOCAL2, LOG_LOCAL3, LOG_LOCAL4, LOG_LOCAL5,
      LOG_LOCAL6, LOG_LOCAL7 };

static int
conf_getFacility(const char *facility) {
    int localnum;
    
    if (strcasecmp(facility, "USER") == 0)
        return LOG_USER;
    if (strlen(facility) != 6
        || strncasecmp(facility, "LOCAL", 5) != 0
        || !isdigit(facility[5]))
        return(-1);
    localnum = atoi(&facility[5]);
    if (localnum > 7)
        return(-1);
    return(facilitynum[localnum]);
}

static int
conf_getUnsignedInt(const char *rval, unsigned *i)
{
    long n;
    char *p;

    errno = 0;
    n = strtoul(rval, &p, 10);
    if (errno)
        return(errno);
    if (strlen(p) != 0)
        return(EINVAL);
    if (n < 0 || n > UINT_MAX)
        return(ERANGE);
    *i = (unsigned int) n;
    return(0);
}

#define confString(name,fld)         \
    if (strcmp(lval, (name)) == 0) { \
        strcpy((config.fld), rval);  \
        return(0);                   \
    }

#define confUnsigned(name,fld)                   \
    if (strcmp(lval, name) == 0) {               \
        unsigned int i;                          \
        int err = conf_getUnsignedInt(rval, &i); \
        if (err != 0)                            \
            return err;                          \
        config.fld = i;                          \
        return(0);                               \
    }

int
CONF_Add(const char *lval, const char *rval)
{
    int ret;
    static bool qlen_configured = false;

    if (strcmp(lval, "qlen.goal") == 0)
        qlen_configured = true;
    
    confString("pid.file", pid_file);
    confString("varnish.name", varnish_name);
    confString("log.file", log_file);
    confString("varnish.bindump", varnish_bindump);
    confString("mq.module", mq_module);
    confString("mq.config_file", mq_config_file);

    confUnsigned("maxdata", maxdata);
    confUnsigned("maxkeylen", maxkeylen);
    confUnsigned("qlen.goal", qlen_goal);
    confUnsigned("nworkers", nworkers);
    confUnsigned("restarts", restarts);
    confUnsigned("restart.pause", restart_pause);
    confUnsigned("thread.restarts", thread_restarts);
    confUnsigned("monitor.interval", monitor_interval);

    if (strcmp(lval, "max.records") == 0) {
        unsigned int i;
        int err = conf_getUnsignedInt(rval, &i);
        if (err != 0)
            return err;
        config.max_records = i;
        if (!qlen_configured)
            config.qlen_goal = config.max_records >> 1;
        return(0);
    }

    if (strcmp(lval, "syslog.facility") == 0) {
        if ((ret = conf_getFacility(rval)) < 0)
            return EINVAL;
        config.syslog_facility = ret;
        strcpy(config.syslog_facility_name, rval);
        char *p = &config.syslog_facility_name[0];
        do { *p = toupper(*p); } while (*++p);
        return(0);
    }

    if (strcmp(lval, "user") == 0) {
        struct passwd *pw;
        
        pw = getpwnam(rval);
        if (pw == NULL)
            return(EINVAL);
        strcpy(config.user_name, pw->pw_name);
        config.uid = pw->pw_uid;
        config.gid = pw->pw_gid;
        return(0);
    }

    if (strcmp(lval, "monitor.workers") == 0) {
        if (strcasecmp(rval, "true") == 0
            || strcasecmp(rval, "on") == 0
            || strcasecmp(rval, "yes") == 0
            || strcmp(rval, "1") == 0) {
            config.monitor_workers = true;
            return(0);
        }
        if (strcasecmp(rval, "false") == 0
            || strcasecmp(rval, "off") == 0
            || strcasecmp(rval, "no") == 0
            || strcmp(rval, "0") == 0) {
            config.monitor_workers = false;
            return(0);
        }
        return(EINVAL);
    }

    if (strcmp(lval, "idle.pause") == 0) {
        char *p;
        errno = 0;
        double d = strtod(rval, &p);
        if (errno == ERANGE)
            return errno;
        if (p[0] != '\0' || d < 0 || isnan(d) || !finite(d))
            return EINVAL;
        config.idle_pause = d;
        return 0;
    }

    return EINVAL;
}

void
CONF_Init(void)
{
    struct passwd *pw;

    strcpy(config.pid_file, "/var/run/trackrdrd.pid");
    config.varnish_name[0] = '\0';
    config.log_file[0] = '\0';
    config.varnish_bindump[0] = '\0';
    config.syslog_facility = LOG_LOCAL0;
    config.monitor_interval = 30;
    config.monitor_workers = false;
    config.max_records = DEF_MAX_RECORDS;
    config.maxdata = DEF_MAXDATA;
    config.maxkeylen = DEF_MAXKEYLEN;
    config.qlen_goal = DEF_QLEN_GOAL;
    config.idle_pause = DEF_IDLE_PAUSE;

    config.mq_module[0] = '\0';
    config.mq_config_file[0] = '\0';
    config.nworkers = 1;
    config.restarts = 1;
    config.restart_pause = 1;
    config.thread_restarts = 1;
    
    pw = getpwnam(DEFAULT_USER);
    if (pw == NULL)
        pw = getpwuid(getuid());
    AN(pw);
    strcpy(config.user_name, pw->pw_name);
    config.uid = pw->pw_uid;
    config.gid = pw->pw_gid;
}

/* XXX: stdout is /dev/null in child process */
int
CONF_ReadDefault(void)
{
    if (access(DEFAULT_CONFIG, F_OK) == 0) {
        if (access(DEFAULT_CONFIG, R_OK) != 0)
            return(errno);
        printf("Reading config from %s\n", DEFAULT_CONFIG);
        if (CONF_ReadFile(DEFAULT_CONFIG, CONF_Add) != 0)
            return -1;
    }
    return 0;
}

#define confdump(level,str,val)                 \
    LOG_Log((level), "config: " str, (val))

void
CONF_Dump(int level)
{
    confdump(level, "pid.file = %s", config.pid_file);
    confdump(level, "varnish.name = %s", config.varnish_name);
    confdump(level, "log.file = %s",
             strcmp(config.log_file,"-") == 0 ? "stdout" : config.log_file);
    confdump(level, "varnish.bindump = %s", config.varnish_bindump);
    confdump(level, "syslog.facility = %s", config.syslog_facility_name);
    confdump(level, "monitor.interval = %u", config.monitor_interval);
    confdump(level, "monitor.workers = %s",
             config.monitor_workers ? "true" : "false");
    confdump(level, "max.records = %u", config.max_records);
    confdump(level, "maxdata = %u", config.maxdata);
    confdump(level, "maxkeylen = %u", config.maxkeylen);
    confdump(level, "qlen.goal = %u", config.qlen_goal);

    confdump(level, "mq.module = %s", config.mq_module);
    confdump(level, "mq.config_file = %s", config.mq_config_file);
    confdump(level, "nworkers = %u", config.nworkers);
    confdump(level, "restarts = %u", config.restarts);
    confdump(level, "restart.pause = %u", config.restart_pause);
    confdump(level, "idle.pause = %f", config.idle_pause);
    confdump(level, "thread.restarts = %u", config.thread_restarts);
    confdump(level, "user = %s", config.user_name);
}
