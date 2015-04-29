/*-
 * Copyright (c) 2012-2015 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012-2015 Otto Gmbh & Co KG
 * All rights reserved
 * Use only with permission
 *
 * Authors: Geoffrey Simmons <geoffrey.simmons@uplex.de>
 *	    Nils Goroll <nils.goroll@uplex.de>
 *
 * Portions adopted from vas.c from the Varnish project
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

#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

#include "trackrdrd.h"

#include "vas.h"

static void __attribute__((__noreturn__))
VAS_Fail_default(const char *func, const char *file, int line, const char *cond,
                 int err, enum vas_e type)
{
    switch(type) {
    case VAS_ASSERT:
        LOG_Log(LOG_ALERT, "Condition (%s) failed in %s(), %s line %d",
                cond, func, file, line);
        break;
    case VAS_MISSING:
        LOG_Log(LOG_ALERT, "Missing error handling code, condition (%s) failed "
                "in %s(), %s line %d", cond, func, file, line);
        break;
    case VAS_INCOMPLETE:
        LOG_Log(LOG_ALERT, "Incomplete code in %s(), %s line %d",
                cond, func, file, line);
        break;
    case VAS_WRONG:
        LOG_Log(LOG_ALERT, "Wrong turn in %s(), %s line %d: %s",
                func, file, line, cond);
        break;
    default:
        assert(0 != 0);
        break;
    }
    if (err)
        LOG_Log(LOG_ALERT, "errno = %d (%s)", err, strerror(err));
    abort();
}

vas_f *VAS_Fail __attribute__((__noreturn__)) = VAS_Fail_default;
