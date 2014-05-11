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

int tests_run = 0;
static char errmsg[BUFSIZ];

static struct freehead_s local_freehead
    = VSTAILQ_HEAD_INITIALIZER(local_freehead);

/* N.B.: Always run this test first */
static char
*test_data_init(void)
{
    int err;

    printf("... testing data table initialization\n");
    
    config.maxopen_scale = 10;
    config.maxdone = 1024;
    err = DATA_Init();
    sprintf(errmsg, "DATA_Init: %s", strerror(err));
    mu_assert(errmsg, err == 0);
    sprintf(errmsg, "DATA_Init: expected table length 2048, got %d", dtbl.len);
    mu_assert(errmsg, dtbl.len == 2048);

    return NULL;
}

static const char
*test_data_take(void)
{
    printf("... testing freelist take\n");

    DATA_Take_Freelist(&local_freehead);
    
    mu_assert("Local freelist empty after take",
        !VSTAILQ_EMPTY(&local_freehead));
    
    sprintf(errmsg, "Global free count non-zero after take (%u)", dtbl.nfree);
    mu_assert(errmsg, dtbl.nfree == 0);

    mu_assert("Global free list non-empty after take",
        VSTAILQ_EMPTY(&dtbl.freehead));

    return NULL;
}

static const char
*test_data_return(void)
{
    printf("... testing freelist return\n");

    DATA_Return_Freelist(&local_freehead, 2048);

    mu_assert("Local freelist non-empty after return",
        VSTAILQ_EMPTY(&local_freehead));
    
    sprintf(errmsg, "Expected global free count == 2048 after return (%u)",
        dtbl.nfree);
    mu_assert(errmsg, dtbl.nfree == 2048);

    mu_assert("Global free list empty after take",
        !VSTAILQ_EMPTY(&dtbl.freehead));

    return NULL;
}
    
static const char
*all_tests(void)
{
    mu_run_test(test_data_init);
    mu_run_test(test_data_take);
    mu_run_test(test_data_return);
    return NULL;
}

TEST_RUNNER
