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

#include <syslog.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "mq_kafka.h"

static int lvl = LOG_INFO;
static const char *level2name[LOG_DEBUG+1];
static FILE *out = NULL;

static void
init_lvlnames(void)
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

void
MQ_LOG_Log(int level, const char *msg, ...)
{
    time_t t;
    char timestr[26];
    va_list ap;
    
    if (level > lvl || out == NULL)
        return;

    t = time(NULL);
    ctime_r(&t, timestr);
    timestr[24] = '\0';

    flockfile(out);
    fprintf(out, "%s [%s]: ", timestr, level2name[level]);
    va_start(ap, msg);
    (void) vfprintf(out, msg, ap);
    va_end(ap);
    fprintf(out, "\n");
    fflush(out);
    funlockfile(out);
}

void
MQ_LOG_SetLevel(int level)
{
    lvl = level;
}

void
MQ_LOG_Close(void)
{
    if (out != NULL)
        fclose(out);
}

int
MQ_LOG_Open(const char *path)
{
    AN(path);
    if (path[0] == '\0')
        return EINVAL;

    if (strcmp(path, "-") == 0)
        out = stdout;
    else {
        out = fopen(path, "a");
        if (out == NULL)
            return errno;
    }
    init_lvlnames();
    
    return(0);
}
