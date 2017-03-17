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

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <errno.h>

#include "minunit.h"

#include "../trackrdrd.h"

int tests_run = 0;

static char
*test_vcl_log(void)
{
    int err, len;
    const char *data;
    vcl_log_t type;

    printf("... testing Parse_VCL_Log\n");

    #define VCLLOG_LEGACY "1253687608 url=%2Frdrtestapp%2F"
    err = Parse_VCL_Log(VCLLOG_LEGACY, strlen(VCLLOG_LEGACY), &data, &len,
                        &type);
    VMASSERT(err == 0, "VCL_Log %s: %s", VCLLOG_LEGACY, strerror(err));
    MASSERT(len == 20);
    MASSERT(type == VCL_LOG_DATA);
    VMASSERT(strncmp(data, "url=%2Frdrtestapp%2F", 20) == 0,
             "VCL_Log %s: returned data=[%.*s]", VCLLOG_LEGACY, len, data);

    #define VCLLOG "url=%2Frdrtestapp%2F"
    err = Parse_VCL_Log(VCLLOG, strlen(VCLLOG), &data, &len, &type);
    VMASSERT(err == 0, "VCL_Log %s: %s", VCLLOG, strerror(err));
    MASSERT(len == 20);
    MASSERT(type == VCL_LOG_DATA);
    VMASSERT(strncmp(data, "url=%2Frdrtestapp%2F", 20) == 0,
             "VCL_Log %s: returned data=[%.*s]", VCLLOG, len, data);

    err = Parse_VCL_Log("foo", 3, &data, &len, &type);
    VMASSERT(err == 0, "VCL_Log foo: %s", strerror(err));
    MASSERT(len == 3);
    MASSERT(type == VCL_LOG_DATA);
    VMASSERT(strncmp(data, "foo", 3) == 0,
             "VCL_Log foo: returned data=[%.*s]", len, data);

    #define VCLLOG_SPACE "foo url=%2Frdrtestapp%2F"
    err = Parse_VCL_Log(VCLLOG_SPACE, 3, &data, &len, &type);
    VMASSERT(err == 0, "VCL_Log %s: %s", VCLLOG_SPACE, strerror(err));
    MASSERT(len == 3);
    MASSERT(type == VCL_LOG_DATA);
    VMASSERT(strncmp(data, "foo", 3) == 0,
             "VCL_Log %s: returned data=[%.*s]", VCLLOG_SPACE, len, data);

    err = Parse_VCL_Log(VCLLOG_SPACE, strlen(VCLLOG_SPACE), &data, &len, &type);
    VMASSERT(err == 0, "VCL_Log foo: %s", strerror(err));
    MASSERT(len == strlen(VCLLOG_SPACE));
    MASSERT(type == VCL_LOG_DATA);
    VMASSERT(strncmp(data, VCLLOG_SPACE, strlen(VCLLOG_SPACE)) == 0,
             "VCL_Log %s: returned data=[%.*s]", VCLLOG_SPACE, len, data);

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
    #define VCLLOG_LONG_LEGACY "1253687608 foo=" LONG_STRING
    err = Parse_VCL_Log(VCLLOG_LONG_LEGACY, 1039, &data, &len, &type);
    VMASSERT(err == 0, "VCL_Log legacy long string: %s", strerror(err));
    MASSERT(len == 1028);
    MASSERT(type == VCL_LOG_DATA);
    VMASSERT(strncmp(data, "foo=" LONG_STRING, 1028) == 0,
             "VCL_Log legacy long string: returned data=[%.*s]", len, data);

    #define VCLLOG_LONG "foo=" LONG_STRING
    err = Parse_VCL_Log(VCLLOG_LONG, strlen(VCLLOG_LONG), &data, &len, &type);
    VMASSERT(err == 0, "VCL_Log long string: %s", strerror(err));
    MASSERT(len == 1028);
    MASSERT(type == VCL_LOG_DATA);
    VMASSERT(strncmp(data, "foo=" LONG_STRING, 1028) == 0,
             "VCL_Log long string: returned data=[%.*s]", len, data);

    #define VCLKEY_LEGACY "1253687608 key foobarbazquux"
    err = Parse_VCL_Log(VCLKEY_LEGACY, strlen(VCLKEY_LEGACY), &data, &len,
                        &type);
    VMASSERT(err == 0, "VCL_Log %s: %s", VCLKEY_LEGACY, strerror(err));
    MASSERT(len == 13);
    MASSERT(type == VCL_LOG_KEY);
    VMASSERT(strncmp(data, "foobarbazquux", 13) == 0,
             "VCL_Log %s: returned data=[%.*s]", VCLKEY_LEGACY, len, data);

    #define VCLKEY "key foobarbazquux"
    err = Parse_VCL_Log(VCLKEY, strlen(VCLKEY), &data, &len, &type);
    VMASSERT(err == 0, "VCL_Log %s: %s", VCLKEY, strerror(err));
    MASSERT(len == 13);
    MASSERT(type == VCL_LOG_KEY);
    VMASSERT(strncmp(data, "foobarbazquux", 13) == 0,
             "VCL_Log %s: returned data=[%.*s]", VCLKEY, len, data);

    #define VCLKEY_LONG_LEGACY "1253687608 key " LONG_STRING
    err = Parse_VCL_Log(VCLKEY_LONG_LEGACY, strlen(VCLKEY_LONG_LEGACY), &data,
                        &len, &type);
    VMASSERT(err == 0, "VCL_Log legacy long key: %s", strerror(err));
    MASSERT(len == 1024);
    MASSERT(type == VCL_LOG_KEY);
    VMASSERT(strncmp(data, LONG_STRING, 1024) == 0,
             "VCL_Log legacy long key: returned data=[%.*s]", len, data);

    #define VCLKEY_LONG "key " LONG_STRING
    err = Parse_VCL_Log(VCLKEY_LONG, 1028, &data, &len, &type);
    VMASSERT(err == 0, "VCL_Log long key: %s", strerror(err));
    MASSERT(len == 1024);
    MASSERT(type == VCL_LOG_KEY);
    VMASSERT(strncmp(data, LONG_STRING, 1024) == 0,
             "VCL_Log long key: returned data=[%.*s]", len, data);

    return NULL;
}

static char
*test_timestamp(void)
{
    int err;
    struct timeval tv;

    printf("... testing Parse_Timestamp\n");

    #define TS "Resp: 1430176881.682097 0.000281 0.000082"
    err = Parse_Timestamp(TS, strlen(TS), &tv);
    VMASSERT(err == 0, "Parse_Timestamp %s: %s", TS, strerror(err));
    MASSERT(tv.tv_sec == 1430176881);
    MASSERT(tv.tv_usec == 682097);

    #define TS0 "Resp: 1430176881.000001 0.000281 0.000082"
    err = Parse_Timestamp(TS0, strlen(TS0), &tv);
    VMASSERT(err == 0, "Parse_Timestamp %s: %s", TS0, strerror(err));
    MASSERT(tv.tv_sec == 1430176881);
    MASSERT(tv.tv_usec == 1);

    return NULL;
}

static const char
*all_tests(void)
{
    mu_run_test(test_vcl_log);
    mu_run_test(test_timestamp);
    return NULL;
}

TEST_RUNNER
