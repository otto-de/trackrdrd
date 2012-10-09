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

#include <string.h>

#include "minunit.h"

#include "../trackrdrd.h"

int tests_run = 0;
static char errmsg[BUFSIZ];

/* N.B.: Always run this test first */
static char
*test_data_init(void)
{
    int err;

    printf("... testing data table initialization\n");
    
    config.maxopen_scale = 0;
    config.maxdata_scale = 0;
    err = DATA_Init();
    sprintf(errmsg, "DATA_Init: %s", strerror(err));
    mu_assert(errmsg, err == 0);
    sprintf(errmsg, "DATA_Init: expected table length 1024, got %d", tbl.len);
    mu_assert(errmsg, tbl.len == 1024);

    return NULL;
}

static const char
*test_data_insert(void)
{
    dataentry *entry;

    printf("... testing data insert\n");

    entry = DATA_Insert(1234567890);
    mu_assert("DATA_Insert returned NULL", entry != NULL);
    sprintf(errmsg, "DATA_Insert: invalid magic number %d", entry->magic);
    mu_assert(errmsg, entry->magic == DATA_MAGIC);
    mu_assert("DATA_Insert: entry not empty", entry->state == DATA_EMPTY);

    unsigned xid = 1234567890;
    for (int i = 0; i < tbl.len; i++) {
        entry = DATA_Insert(xid + i);
        mu_assert("DATA_Insert returned NULL, table not full", entry != NULL);
        entry->state = DATA_OPEN;
    }

    xid++;
    entry = DATA_Insert(xid);
    mu_assert("DATA_Insert: table full, expected NULL", entry == NULL);

    /* Cleanup */
    for (int i = 0; i < tbl.len; i++)
        tbl.entry[i].state = DATA_EMPTY;
    
    return NULL;
}

static const char
*test_data_find(void)
{
    dataentry *entry1, *entry2;
    unsigned xid;

    printf("... testing data find\n");

    entry1 = DATA_Insert(1234567890);
    entry1->state = DATA_OPEN;
    entry1->xid = 1234567890;
    entry2 = DATA_Find(1234567890);
    mu_assert("DATA_Find: returned NULL", entry2 != NULL);
    sprintf(errmsg, "DATA_Find: invalid magic number %d", entry2->magic);
    mu_assert(errmsg, entry2->magic == DATA_MAGIC);
    sprintf(errmsg, "DATA_Find: expected XID=1234567890, got %d", entry2->xid);
    mu_assert(errmsg, entry2->xid == 1234567890);
    /* Cleanup */
    entry1->state = DATA_EMPTY;
    entry1->xid = 0;

    entry2 = DATA_Find(1234567890);
    mu_assert("DATA_Find: expected NULL", entry2 == NULL);

    xid = 1234567890;
    for (int i = 0; i < tbl.len; i++) {
        entry1 = DATA_Insert(xid);
        entry1->state = DATA_OPEN;
        entry1->xid = xid++;
    }
    entry2 = DATA_Find(xid);
    mu_assert("DATA_Find: expected NULL", entry2 == NULL);

    for (int i = 0; i < tbl.len; i++) {
        entry2 = DATA_Find(1234567890 + i);
        sprintf(errmsg, "DATA_Find: %d, returned NULL", 1234567890 + i);
        mu_assert(errmsg, entry2 != NULL);
    }
    
    return NULL;
}

static const char
*all_tests(void)
{
    mu_run_test(test_data_init);
    mu_run_test(test_data_insert);
    mu_run_test(test_data_find);
    return NULL;
}

TEST_RUNNER
