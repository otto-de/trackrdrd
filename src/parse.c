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

#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

#include "trackrdrd.h"

#include "vas.h"

static int
parse_UnsignedDec(const char *str, int len, unsigned *num)
{
    unsigned new;
    *num = 0;
    for (int i = 0; i < len; i++) {
        if (!isdigit(str[i]))
            return EINVAL;
        new = *num * 10 + (str[i] - '0');
        if (new < *num)
            return ERANGE;
        *num = new;
    }
    return 0;
}

/* ptr points to the first char after "track "
   len is length from that char to end of data
*/
int
Parse_VCL_Log(const char *ptr, int len, const char **data, int *datalen,
              vcl_log_t *type)
{
    const char *c = ptr;

    *type = VCL_LOG_DATA;
    while (isdigit(*c))
        c++;
    if (*c == ' ' && (c - ptr + 1 < len))
        c++;
    else
        c = ptr;
    if (strncmp(c, "key ", 4) == 0) {
        c += 4;
        *type = VCL_LOG_KEY;
    }
    *data = c;
    *datalen = ptr + len - *data;
    return(0);
}

int
Parse_Timestamp(const char *ptr, int len, struct timeval *t)
{
    unsigned num;
    const char *p = ptr + (sizeof("Resp: ") - 1);

    char *dot = memchr(p, '.', len);
    AZ(parse_UnsignedDec(p, dot - p, &num));
    t->tv_sec = num;
    char *blank = memchr(dot + 1, ' ', len - (dot - p));
    AZ(parse_UnsignedDec(dot + 1, blank - dot - 1, &num));
    assert(num < 1000000);
    t->tv_usec = num;
    return(0);
}
