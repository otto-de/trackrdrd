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

#define Parse_UnsignedDec(str,len,xid) Parse_XID((str),(len),(xid))

int
Parse_XID(const char *str, int len, unsigned *xid)
{
    unsigned new;
    *xid = 0;
    for (int i = 0; i < len; i++) {
        if (!isdigit(str[i]))
            return EINVAL;
        new = *xid * 10 + (str[i] - '0');
        if (new < *xid)
            return ERANGE;
        else
            *xid = new;
    }
    return 0;
}

int
Parse_ReqStart(const char *ptr, int len, unsigned *xid)
{
    int i;
    for (i = len-1; ptr[i] != ' '; i--)
        if (i == 0)
            return EINVAL;
    return Parse_XID(&ptr[i+1], len-i-1, xid);
}

int
Parse_ReqEnd(const char *ptr, unsigned len, unsigned *xid,
    struct timespec *reqend_t)
{
    int err;
    char *blank, *reqend_tstr, *dot;
    
    blank = memchr(ptr, ' ', len);
    if (blank == NULL)
        return EINVAL;
    err = Parse_XID(ptr, blank-ptr, xid);
    if (err != 0)
        return err;
    
    reqend_tstr = memchr(blank + 1, ' ', len-(blank-ptr));
    if (reqend_tstr == NULL)
        return EINVAL;
    else
        reqend_tstr++;
    dot = memchr(reqend_tstr, '.', len-(reqend_tstr-ptr));
    if (dot == NULL)
        return EINVAL;
    blank = memchr(dot + 1, ' ', len-(dot-ptr));
    if (blank == NULL)
        return EINVAL;
    err = Parse_UnsignedDec(reqend_tstr, dot-reqend_tstr,
        (unsigned *)&reqend_t->tv_sec);
    reqend_t->tv_sec &= 0x0ffffffff;
    if (err != 0)
        return err;
    err = Parse_UnsignedDec(dot+1, blank-dot-1,
        (unsigned *) &reqend_t->tv_nsec);
    if (err != 0)
        return err;
    reqend_t->tv_nsec &= 0x0ffffffff;
    return 0;
}

/* ptr points to the first char after "track "
   len is length from that char to end of data
*/
int
Parse_VCL_Log(const char *ptr, int len, unsigned *xid,
              char **data, int *datalen)
{
    char *blank = memchr(ptr, ' ', len);
    if (blank == NULL)
        return EINVAL;
    int err = Parse_XID(ptr, blank-ptr, xid);
    if (err != 0)
        return err;
    *data = blank + 1;
    *datalen = ptr + len - blank - 1;
    return(0);
}

