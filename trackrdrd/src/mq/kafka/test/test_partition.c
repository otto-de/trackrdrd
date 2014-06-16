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

#include <stdint.h>

#include "../mq_kafka.h"
#include "../../../test/minunit.h"

int tests_run = 0;

static char
*test_partitioner(void)
{
    int32_t partition;

    printf("... testing partitioner function\n");

    partition = TEST_Partition((const void *) "5ff1b68d", 8, 4);
    VMASSERT(partition == 1, "key 5ff1b68d, expected 1, got %d", partition);

    partition = TEST_Partition((const void *) "5f9f78d5", 8, 4);
    VMASSERT(partition == 1, "key 5f9f78d5, expected 1, got %d", partition);

    partition = TEST_Partition((const void *) "7c735b38", 8, 4);
    VMASSERT(partition == 0, "key 7c735b38, expected 1, got %d", partition);

    partition = TEST_Partition((const void *) "80ffd2a7", 8, 4);
    VMASSERT(partition == 3, "key 80ffd2a7, expected 1, got %d", partition);

    partition = TEST_Partition((const void *) "a3d0b3e9", 8, 4);
    VMASSERT(partition == 1, "key a3d0b3e9, expected 1, got %d", partition);

    partition = TEST_Partition((const void *) "c923ca00", 8, 4);
    VMASSERT(partition == 0, "key c923ca00, expected 0, got %d", partition);

    return NULL;
}

static const char
*all_tests(void)
{
    mu_run_test(test_partitioner);
    return NULL;
}

TEST_RUNNER
