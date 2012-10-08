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

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <errno.h>

#include "minunit.h"

#include "../trackrdrd.h"

#define MAX_RANDOM_XIDS (1 << 17)

int tests_run = 0;
static char errmsg[BUFSIZ];

static char
*test_random_xids(void)
{
    char s[10];
    int len, err;
    
    srand48(time(NULL));
    unsigned xid = (unsigned int) lrand48();
    if (xid == 0)
        xid = 1;
    printf("Parsing %d sequential XIDs from random start\n", MAX_RANDOM_XIDS);
    
    for (int i = 0; i < MAX_RANDOM_XIDS; i++) {
        sprintf(s, "%d%c", xid, (char) random());
        len = (int) log10(xid) + 1;

        unsigned result;
        err = Parse_XID(s, len, &result);
        sprintf(errmsg, "%d: %s", xid, strerror(err));
        mu_assert(errmsg, err == 0);
        sprintf(errmsg, "%d: Parse_XID returned %d", xid, result);
        mu_assert(errmsg, xid == result);
    }
    return NULL;
}

static char
*test_xid_corners(void)
{
    unsigned xid;
    int err;
    
    printf("Testing XID corner cases\n");

    err = Parse_XID("0", 1, &xid);
    sprintf(errmsg, "0: %s", strerror(err));
    mu_assert(errmsg, err == 0);
    sprintf(errmsg, "0: Parse_XID returned %d", xid);
    mu_assert(errmsg, xid == 0);

    err = Parse_XID("4294967295", 10, &xid);
    sprintf(errmsg, "4294967295: %s", strerror(err));
    mu_assert(errmsg, err == 0);
    sprintf(errmsg, "4294967295: Parse_XID returned %d", xid);
    mu_assert(errmsg, xid == 4294967295);
    
    err = Parse_XID("-1", 2, &xid);
    sprintf(errmsg, "-1: Expected EINVAL, got %d", err);
    mu_assert(errmsg, err == EINVAL);

    err = Parse_XID("foo", 3, &xid);
    sprintf(errmsg, "foo: Expected EINVAL, got %d", err);
    mu_assert(errmsg, err == EINVAL);

    err = Parse_XID("4294967296", 10, &xid);
    sprintf(errmsg, "2^32: Expected ERANGE, got %d", err);
    mu_assert(errmsg, err == ERANGE);

    return(NULL);
}

static char
*test_reqstart(void)
{
    unsigned xid;
    int err;

    printf("Testing Parse_ReqStart\n");

    #define REQSTART "127.0.0.1 40756 1253687608"
    err = Parse_ReqStart(REQSTART, strlen(REQSTART), &xid);
    sprintf(errmsg, "ReqStart %s: %s", REQSTART, strerror(err));
    mu_assert(errmsg, err == 0);
    sprintf(errmsg, "ReqStart %s: returned XID=%d", REQSTART, xid);
    mu_assert(errmsg, xid == 1253687608);

    err = Parse_ReqStart("1253687608", 10, &xid);
    sprintf(errmsg, "ReqStart 1253687608: expected EINVAL, got %d", err);
    mu_assert(errmsg, err == EINVAL);

    return NULL;
}

static char
*test_reqend(void)
{
    unsigned xid;
    int err;

    printf("Testing Parse_ReqEnd\n");

    #define REQEND "1253687608 1348291555.658257008 1348291555.670388222 -0.012122154 NaN NaN"

    err = Parse_ReqEnd(REQEND, strlen(REQEND), &xid);
    sprintf(errmsg, "ReqEnd %s: %s", REQEND, strerror(err));
    mu_assert(errmsg, err == 0);
    sprintf(errmsg, "ReqEnd %s: returned XID=%d", REQEND, xid);
    mu_assert(errmsg, xid == 1253687608);

    err = Parse_ReqEnd("1253687608", 10, &xid);
    sprintf(errmsg, "ReqEnd 1253687608: expected EINVAL, got %d", err);
    mu_assert(errmsg, err == EINVAL);

    return NULL;
}

static char
*test_vcl_log(void)
{
    unsigned xid;
    int err, len;
    char *data;

    printf("Testing Parse_VCL_Log\n");

    #define VCLLOG "1253687608 url=%2Frdrtestapp%2F"
    err = Parse_VCL_Log(VCLLOG, strlen(VCLLOG), &xid, &data, &len);
    sprintf(errmsg, "VCL_Log %s: %s", VCLLOG, strerror(err));
    mu_assert(errmsg, err == 0);
    sprintf(errmsg, "VCL_Log %s: returned XID=%d", VCLLOG, xid);
    mu_assert(errmsg, xid == 1253687608);
    sprintf(errmsg, "VCL_Log %s: returned length=%d", VCLLOG, len);
    mu_assert(errmsg, len == 20);
    sprintf(errmsg, "VCL_Log %s: returned data=[%.*s]", VCLLOG, len, data);
    mu_assert(errmsg, strncmp(data, "url=%2Frdrtestapp%2F", 20) == 0);

    err = Parse_VCL_Log("foo", 3, &xid, &data, &len);
    sprintf(errmsg, "VCL_Log foo: expected EINVAL, got %d", err);
    mu_assert(errmsg, err == EINVAL);

    #define VCLLOG_INVALID "foo url=%2Frdrtestapp%2F"
    err = Parse_VCL_Log(VCLLOG_INVALID, 3, &xid, &data, &len);
    sprintf(errmsg, "VCL_Log %s: expected EINVAL, got %d", VCLLOG_INVALID, err);
    mu_assert(errmsg, err == EINVAL);

    /* 1024 chars */
    #define LONG_STRING \
        "1234567890123456789012345678901234567890123456789012345678901234" \
        "1234567890123456789012345678901234567890123456789012345678901234" \
        "1234567890123456789012345678901234567890123456789012345678901234" \
        "1234567890123456789012345678901234567890123456789012345678901234" \
        "1234567890123456789012345678901234567890123456789012345678901234" \
        "1234567890123456789012345678901234567890123456789012345678901234" \
        "1234567890123456789012345678901234567890123456789012345678901234" \
        "1234567890123456789012345678901234567890123456789012345678901234" \
        "1234567890123456789012345678901234567890123456789012345678901234" \
        "1234567890123456789012345678901234567890123456789012345678901234" \
        "1234567890123456789012345678901234567890123456789012345678901234" \
        "1234567890123456789012345678901234567890123456789012345678901234" \
        "1234567890123456789012345678901234567890123456789012345678901234" \
        "1234567890123456789012345678901234567890123456789012345678901234" \
        "1234567890123456789012345678901234567890123456789012345678901234" \
        "1234567890123456789012345678901234567890123456789012345678901234"
    #define VCLLOG_LONG "1253687608 foo=" LONG_STRING
    err = Parse_VCL_Log(VCLLOG_LONG, 1039, &xid, &data, &len);
    sprintf(errmsg, "VCL_Log long string: %s", strerror(err));
    mu_assert(errmsg, err == 0);
    sprintf(errmsg, "VCL_Log long string: returned XID=%d", xid);
    mu_assert(errmsg, xid == 1253687608);
    sprintf(errmsg, "VCL_Log long string: returned length=%d", len);
    mu_assert(errmsg, len == 1028);
    sprintf(errmsg, "VCL_Log long string: returned data=[%.*s]", len, data);
    mu_assert(errmsg, strncmp(data, "foo=" LONG_STRING, 1028) == 0);

    return NULL;
}

static char
*all_tests(void)
{
    mu_run_test(test_random_xids);
    mu_run_test(test_xid_corners);
    mu_run_test(test_reqstart);
    mu_run_test(test_reqend);
    mu_run_test(test_vcl_log);
    return NULL;
}

int
main(int argc, char **argv)
{
    (void) argc;
    
    printf("%s: running\n", argv[0]);
    char *result = all_tests();
    printf("%s: %d tests run\n", argv[0],  tests_run);
    if (result != NULL) {
        printf("%s\n", result);
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}
