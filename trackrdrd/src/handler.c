/*-
 * Copyright (c) 2012 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012 Otto Gmbh & Co KG
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
 */

#include "config.h"

#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>

#ifndef HAVE_EXECINFO_H
#include "compat/execinfo.h"
#else
#include <execinfo.h>
#endif

#include "vas.h"

#include "trackrdrd.h"

/* XXX: configurable? */
#define MAX_STACK_DEPTH 100

/*--------------------------------------------------------------------*/

void
HNDL_Terminate(int sig)
{
    (void) sig;
    term = 1;
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

void
HNDL_Abort(int sig)
{
    AZ(sigaction(SIGABRT, &default_action, NULL));
    LOG_Log(LOG_ALERT, "Received signal %d (%s), stacktrace follows", sig,
        strsignal(sig));
    stacktrace();
    MON_Output();
    LOG_Log0(LOG_ALERT, "Aborting");
    abort();
}
