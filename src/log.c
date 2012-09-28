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

#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>

#include "trackrdrd.h"

#include "libvarnish.h"

static const char *level2name[LOG_DEBUG];
static const int facilitynum[8] =
    { LOG_LOCAL0, LOG_LOCAL1, LOG_LOCAL2, LOG_LOCAL3, LOG_LOCAL4, LOG_LOCAL5,
      LOG_LOCAL6, LOG_LOCAL7 };

static void
syslog_setlevel(int level)
{
    setlogmask(LOG_UPTO(level));
}

/* XXX: is this safe? */
static void
stdio_initnames(void)
{
    level2name[LOG_EMERG]	= "EMERG";
    level2name[LOG_ALERT]	= "ALERT";
    level2name[LOG_CRIT]	= "CRIT";
    level2name[LOG_ERR]		= "ERR";
    level2name[LOG_WARNING]	= "WARNING";
    level2name[LOG_NOTICE]	= "NOTICE";
    level2name[LOG_INFO]	= "INFO";
    level2name[LOG_DEBUG]	= "DEBUG";
}

static void
stdio_log(int level, const char *msg, ...)
{
    va_list ap;
    
    if (level > logconf.level)
        return;
    fprintf(logconf.out, "%s: ", level2name[level]);
    va_start(ap, msg);
    (void) vfprintf(logconf.out, msg, ap);
    va_end(ap);
    fprintf(logconf.out, "\n");
}

static void
stdio_setlevel(int level)
{
    logconf.level = level;
}

static void
stdio_close(void)
{
    fclose(logconf.out);
}

int LOG_Open(const char *progname)
{
    if (EMPTY(config.log_file)) {
        /* syslog */
        logconf.log = syslog;
        logconf.setlevel = syslog_setlevel;
        logconf.close = closelog;
        openlog(progname, LOG_PID | LOG_CONS | LOG_NDELAY | LOG_NOWAIT,
                config.syslog_facility);
        setlogmask(LOG_UPTO(LOG_INFO));
        atexit(closelog);
        return(0);
    }

    if (strcmp(config.log_file, "-") == 0)
        logconf.out = stdout;
    else {
        logconf.out = fopen(config.log_file, "a");
        if (logconf.out == NULL) {
            perror(config.log_file);
            return(-1);
        }
    }
    logconf.level = LOG_INFO;
    logconf.log = stdio_log;
    logconf.setlevel = stdio_setlevel;
    logconf.close = stdio_close;
    stdio_initnames();
    
    return(0);
}
