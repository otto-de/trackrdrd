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
#include "libvarnish.h"

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
    confString("mq.qname", mq_qname);

    confUnsigned("maxopen.scale", maxopen_scale);
    confUnsigned("maxdata", maxdata);
    confUnsigned("qlen.goal", qlen_goal);
    confUnsigned("hash.max_probes", hash_max_probes);
    confUnsigned("hash.ttl", hash_ttl);
    confUnsigned("hash.mlt", hash_mlt);
    confUnsigned("nworkers", nworkers);
    confUnsigned("restarts", restarts);
    confUnsigned("thread.restarts", restarts);
    confUnsigned("monitor.interval", monitor_interval);

    if (strcmp(lval, "maxdone") == 0) {
        unsigned int i;
        int err = conf_getUnsignedInt(rval, &i);
        if (err != 0)
            return err;
        config.maxdone = i;
        if (!qlen_configured)
            config.qlen_goal = config.maxdone >> 1;
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

    if (strcmp(lval, "mq.uri") == 0) {
        int n = config.n_mq_uris++;
        config.mq_uri = (char **) realloc(config.mq_uri,
            config.n_mq_uris * sizeof(char **));
        if (config.mq_uri == NULL)
            return(errno);
        config.mq_uri[n] = (char *) malloc(strlen(rval) + 1);
        if (config.mq_uri[n] == NULL)
            return(errno);
        strcpy(config.mq_uri[n], rval);
        return(0);
    }

    return EINVAL;
}

static int
conf_ParseLine(char *ptr, char **lval, char **rval)
{
    char *endlval;
    
    *lval = ptr;
    while(*++ptr && !isspace(*ptr) && *ptr != '=')
        ;
    if (*ptr == '\0')
        return(1);
    endlval = ptr;
    while(isspace(*ptr) && *++ptr)
        ;
    if (ptr == '\0' || *ptr != '=')
        return(1);
    while(*++ptr && isspace(*ptr))
        ;
    if (ptr == '\0')
        return(1);
    *endlval = '\0';
    *rval = ptr;
    return(0);
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
    config.maxopen_scale = DEF_MAXOPEN_SCALE;
    config.maxdone = DEF_MAXDONE;
    config.maxdata = DEF_MAXDATA;
    config.qlen_goal = DEF_QLEN_GOAL;
    config.hash_max_probes = DEF_HASH_MAX_PROBES;
    config.hash_ttl = DEF_HASH_TTL;
    config.hash_mlt = DEF_HASH_MLT;

    config.n_mq_uris = 0;
    config.mq_uri = (char **) malloc (sizeof(char **));
    AN(config.mq_uri);
    config.mq_qname[0] = '\0';
    config.nworkers = 1;
    config.restarts = 1;
    config.thread_restarts = 1;
    
    pw = getpwnam(DEFAULT_USER);
    if (pw == NULL)
        pw = getpwuid(getuid());
    AN(pw);
    strcpy(config.user_name, pw->pw_name);
    config.uid = pw->pw_uid;
    config.gid = pw->pw_gid;
}

int
CONF_ReadFile(const char *file) {
    FILE *in;
    char line[BUFSIZ];
    int linenum = 0;

    in = fopen(file, "r");
    if (in == NULL) {
        perror(file);
        return(-1);
    }
    
    while (fgets(line, BUFSIZ, in) != NULL) {
        char orig[BUFSIZ];
        
        linenum++;
        char *comment = strchr(line, '#');
        if (comment != NULL)
            *comment = '\0';
        if (strlen(line) == 0)
            continue;
    
        char *ptr = line + strlen(line) - 1;
        while (ptr != line && isspace(*ptr))
            --ptr;
        ptr[isspace(*ptr) ? 0 : 1] = '\0';
        if (strlen(line) == 0)
            continue;

        ptr = line;
        while (isspace(*ptr) && *++ptr)
            ;
        strcpy(orig, ptr);
        char *lval, *rval;
        if (conf_ParseLine(ptr, &lval, &rval) != 0) {
            fprintf(stderr, "Cannot parse %s line %d: '%s'\n", file, linenum,
                    orig);
            return(-1);
        }

        int ret;
        if ((ret = CONF_Add((const char *) lval, (const char *) rval)) != 0) {
            fprintf(stderr, "Error in %s line %d (%s): '%s'\n", file, linenum,
                strerror(ret), orig);
            return(-1);
        }
    }
    return(0);
}

/* XXX: stdout is /dev/null in child process */
int
CONF_ReadDefault(void)
{
    if (access(DEFAULT_CONFIG, F_OK) == 0) {
        if (access(DEFAULT_CONFIG, R_OK) != 0)
            return(errno);
        printf("Reading config from %s\n", DEFAULT_CONFIG);
        if (CONF_ReadFile(DEFAULT_CONFIG) != 0)
            return -1;
    }
    return 0;
}

#define confdump(str,val) \
    LOG_Log(LOG_DEBUG, "config: " str, (val))

void
CONF_Dump(void)
{
    confdump("pid.file = %s", config.pid_file);
    confdump("varnish.name = %s", config.varnish_name);
    confdump("log.file = %s",
        strcmp(config.log_file,"-") == 0 ? "stdout" : config.log_file);
    confdump("varnish.bindump = %s", config.varnish_bindump);
    confdump("syslog.facility = %s", config.syslog_facility_name);
    confdump("monitor.interval = %u", config.monitor_interval);
    confdump("monitor.workers = %s", config.monitor_workers ? "true" : "false");
    confdump("maxopen.scale = %u", config.maxopen_scale);
    confdump("maxdone = %u", config.maxdone);
    confdump("maxdata = %u", config.maxdata);
    confdump("qlen.goal = %u", config.qlen_goal);
    confdump("hash.max_probes = %u", config.hash_max_probes);
    confdump("hash.ttl = %u", config.hash_ttl);
    confdump("hash.mlt = %u", config.hash_mlt);

    if (config.n_mq_uris > 0)
        for (int i = 0; i < config.n_mq_uris; i++)
            confdump("mq.uri = %s", config.mq_uri[i]);
    else
        LOG_Log0(LOG_DEBUG, "config: mq.uri = ");
    
    confdump("mq.qname = %s", config.mq_qname);
    confdump("nworkers = %u", config.nworkers);
    confdump("restarts = %u", config.restarts);
    confdump("thread.restarts = %u", config.thread_restarts);
    confdump("user = %s", config.user_name);
}
