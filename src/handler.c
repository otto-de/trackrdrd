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
 */

#include "config.h"

#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#include "vas.h"
#include "vsb.h"

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

#ifdef HAVE_EXECINFO_H

/*
 * This hack is almost verbatim from varnishd.c -- attempt to run nm(1) on
 * ourselves at startup to get a mapping from lib pointers to symbolic
 * function names for stack traces.
 *
 * +1 to phk's rant in varnishd.c about the lack of a standard for this.
 */

struct symbols {
    uintptr_t               a;
    char                    *n;
    VTAILQ_ENTRY(symbols)   list;
};

static VTAILQ_HEAD(,symbols) symbols = VTAILQ_HEAD_INITIALIZER(symbols);

static int
symbol_lookup(struct vsb *vsb, void *ptr)
{
    struct symbols *s, *s0;
    uintptr_t pp;

    pp = (uintptr_t)ptr;
    s0 = NULL;
    VTAILQ_FOREACH(s, &symbols, list) {
        if (s->a > pp)
            continue;
        if (s0 == NULL || s->a > s0->a)
            s0 = s;
    }
    if (s0 == NULL)
        return (-1);
    VSB_printf(vsb, "%p: %s+%jx", ptr, s0->n, (uintmax_t)pp - s0->a);
    return (0);
}

static void
symbol_hack(const char *a0)
{
    char buf[BUFSIZ], *p, *e;
    FILE *fi;
    uintptr_t a;
    struct symbols *s;

    sprintf(buf, "nm -an %s 2>/dev/null", a0);
    fi = popen(buf, "r");
    if (fi == NULL)
        return;
    while (fgets(buf, sizeof buf, fi)) {
        if (buf[0] == ' ')
            continue;
        p = NULL;
        a = strtoul(buf, &p, 16);
        if (p == NULL)
            continue;
        if (a == 0)
            continue;
        if (*p++ != ' ')
            continue;
        p++;
        if (*p++ != ' ')
            continue;
        if (*p <= ' ')
            continue;
        e = strchr(p, '\0');
        AN(e);
        while (e > p && isspace(e[-1]))
            e--;
        *e = '\0';
        s = malloc(sizeof *s + strlen(p) + 1);
        AN(s);
        s->a = a;
        s->n = (void*)(s + 1);
        strcpy(s->n, p);
        VTAILQ_INSERT_TAIL(&symbols, s, list);
    }
    (void)pclose(fi);
}

static void
stacktrace(void)
{
    void *buf[MAX_STACK_DEPTH];
    int depth, i;
    struct vsb *sb = VSB_new_auto();

    depth = backtrace (buf, MAX_STACK_DEPTH);
    if (depth == 0) {
        LOG_Log0(LOG_ERR, "Stacktrace empty");
        return;
    }
    for (i = 0; i < depth; i++) {
        VSB_clear(sb);
        if (symbol_lookup(sb, buf[i]) < 0) {
            char **strings;
            strings = backtrace_symbols(&buf[i], 1);
            if (strings != NULL && strings[0] != NULL)
                VSB_printf(sb, "%p: %s", buf[i], strings[0]);
            else
                VSB_printf(sb, "%p: (?)", buf[i]);
        }
        VSB_finish(sb);
        LOG_Log(LOG_ERR, "%s", VSB_data(sb));
    }
}

#endif

void
HNDL_Init(const char *a0)
{
#ifdef HAVE_EXECINFO_H
    symbol_hack(a0);
#else
    (void) a0;
#endif
}

void
HNDL_Abort(int sig)
{
    AZ(sigaction(SIGABRT, &default_action, NULL));
    LOG_Log(LOG_ALERT, "Received signal %d (%s)", sig, strsignal(sig));
#ifdef HAVE_EXECINFO_H
    LOG_Log0(LOG_NOTICE, "Stacktrace follows");
    stacktrace();
#endif
    DATA_Dump();
    MON_Output();
    LOG_Log0(LOG_ALERT, "Aborting");
    abort();
}
