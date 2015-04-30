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

#include <string.h>

#include "minunit.h"

#include "../trackrdrd.h"
#include "../data.h"

int tests_run = 0;

static unsigned nfree = 0;

static struct freehead_s local_freehead
    = VSTAILQ_HEAD_INITIALIZER(local_freehead);

/* N.B.: Always run this test first */
static char
*test_data_init(void)
{
    int err;

    printf("... testing data table initialization\n");
    
    config.max_records = DEF_MAX_RECORDS;
    config.max_reclen = DEF_MAX_RECLEN;
    config.maxkeylen = DEF_MAXKEYLEN;
    err = DATA_Init();
    VMASSERT(err == 0, "DATA_Init: %s", strerror(err));

    for (int i = 0; i < config.max_records; i++) {
        MCHECK_OBJ_NOTNULL(&entrytbl[i], DATA_MAGIC);
        MASSERT(!OCCUPIED(&entrytbl[i]));
        MAZ(entrytbl[i].hasdata);
        MAN(entrytbl[i].data);
        MAN(entrytbl[i].key);
        MAZ(entrytbl[i].end);
        MAZ(entrytbl[i].keylen);
    }

    return NULL;
}

static const char
*test_data_set_get(void)
{
    char data[DEF_MAX_RECLEN], key[DEF_MAXKEYLEN];
    
    printf("... testing data write and read\n");

    for (int i = 0; i < config.max_records; i++) {
        memset(entrytbl[i].data, 'd', DEF_MAX_RECLEN);
        memset(entrytbl[i].key,  'k', DEF_MAXKEYLEN);
    }

    memset(data, 'd', DEF_MAX_RECLEN);
    memset(key,  'k', DEF_MAXKEYLEN);

    for (int i = 0; i < config.max_records; i++) {
        MASSERT(memcmp(entrytbl[i].data, data, DEF_MAX_RECLEN) == 0);
        MASSERT(memcmp(entrytbl[i].key,  key,  DEF_MAXKEYLEN) == 0);
    }

    return NULL;
}

static const char
*test_data_take(void)
{
    printf("... testing freelist take\n");

    nfree = DATA_Take_Freelist(&local_freehead);

    MASSERT(nfree == config.max_records);

    MASSERT0(!VSTAILQ_EMPTY(&local_freehead),
             "Local freelist empty after take");
    
    VMASSERT(global_nfree == 0, "Global free count non-zero after take (%u)",
             global_nfree);

    MASSERT0(VSTAILQ_EMPTY(&freehead),
             "Global free list non-empty after take");

    return NULL;
}

static const char
*test_data_return(void)
{
    printf("... testing freelist return\n");

    DATA_Return_Freelist(&local_freehead, nfree);

    MASSERT0(VSTAILQ_EMPTY(&local_freehead),
             "Local freelist non-empty after return");
    
    MASSERT(global_nfree == DEF_MAX_RECORDS);

    MASSERT0(!VSTAILQ_EMPTY(&freehead),
             "Global free list empty after take");

    return NULL;
}
    
static const char
*all_tests(void)
{
    mu_run_test(test_data_init);
    mu_run_test(test_data_set_get);
    mu_run_test(test_data_take);
    mu_run_test(test_data_return);
    return NULL;
}

TEST_RUNNER
