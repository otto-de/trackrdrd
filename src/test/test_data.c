/*-
 * Copyright (c) 2012-2015 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012-2015 Otto Gmbh & Co KG
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

static struct rechead_s local_freerechead
    = VSTAILQ_HEAD_INITIALIZER(local_freerechead);
static chunkhead_t local_freechunk = VSTAILQ_HEAD_INITIALIZER(local_freechunk);

unsigned nchunks;

/* N.B.: Always run this test first */
static char
*test_data_init(void)
{
    int err;
    unsigned chunks_per_rec, free_chunk = 0, free_rec = 0;
    chunk_t *chunk;
    dataentry *entry;

    printf("... testing data table initialization\n");
    
    config.max_records = DEF_MAX_RECORDS;
    config.max_reclen = DEF_MAX_RECLEN;
    config.maxkeylen = DEF_MAXKEYLEN;
    config.chunk_size = DEF_CHUNK_SIZE;
    err = DATA_Init();
    VMASSERT(err == 0, "DATA_Init: %s", strerror(err));

    MAN(entrytbl);
    MAN(chunktbl);
    MASSERT(!VSTAILQ_EMPTY(&freerechead));
    MASSERT(!VSTAILQ_EMPTY(&freechunkhead));

    chunks_per_rec = (DEF_MAX_RECLEN + DEF_CHUNK_SIZE - 1) / DEF_CHUNK_SIZE;
    nchunks = chunks_per_rec * DEF_MAX_RECORDS;

    MASSERT(global_nfree_chunk == nchunks);
    MASSERT(global_nfree_rec == DEF_MAX_RECORDS);

    for (int i = 0; i < nchunks; i++) {
        MCHECK_OBJ_NOTNULL(&chunktbl[i], CHUNK_MAGIC);
        MASSERT(!OCCUPIED(&chunktbl[i]));
        MAN(chunktbl[i].data);
    }

    for (int i = 0; i < config.max_records; i++) {
        MCHECK_OBJ_NOTNULL(&entrytbl[i], DATA_MAGIC);
        MASSERT(!OCCUPIED(&entrytbl[i]));
        MASSERT(VSTAILQ_EMPTY(&entrytbl[i].chunks));
        MAN(entrytbl[i].key);
        MAZ(entrytbl[i].end);
        MAZ(entrytbl[i].keylen);
        MAZ(entrytbl[i].curchunk);
        MAZ(entrytbl[i].curchunkidx);
    }

    VSTAILQ_FOREACH(chunk, &freechunkhead, freelist) {
        MCHECK_OBJ_NOTNULL(chunk, CHUNK_MAGIC);
        free_chunk++;
    }
    MASSERT(free_chunk == global_nfree_chunk);

    VSTAILQ_FOREACH(entry, &freerechead, freelist) {
        MCHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
        free_rec++;
    }
    MASSERT(free_rec == global_nfree_rec);

    return NULL;
}

static const char
*test_data_take_rec(void)
{
    unsigned nfree, cfree = 0;
    dataentry *entry;

    printf("... testing record freelist take\n");

    nfree = DATA_Take_Freerec(&local_freerechead);

    MASSERT(nfree == config.max_records);
    MASSERT(!VSTAILQ_EMPTY(&local_freerechead));
    MAZ(global_nfree_rec);
    MASSERT(VSTAILQ_EMPTY(&freerechead));
    VSTAILQ_FOREACH(entry, &local_freerechead, freelist) {
        MCHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
        cfree++;
    }
    MASSERT(nfree == cfree);

    return NULL;
}

static const char
*test_data_take_chunk(void)
{
    unsigned nfree, cfree = 0;
    chunk_t *chunk;

    printf("... testing chunk freelist take\n");

    nfree = DATA_Take_Freechunk(&local_freechunk);

    MASSERT(nfree == nchunks);
    MASSERT(!VSTAILQ_EMPTY(&local_freechunk));
    MAZ(global_nfree_chunk);
    MASSERT(VSTAILQ_EMPTY(&freechunkhead));
    VSTAILQ_FOREACH(chunk, &local_freechunk, freelist) {
        MCHECK_OBJ_NOTNULL(chunk, CHUNK_MAGIC);
        cfree++;
    }
    MASSERT(nfree == cfree);

    return NULL;
}

static const char
*test_data_return_rec(void)
{
    unsigned cfree = 0;
    dataentry *entry;

    printf("... testing record freelist return\n");

    DATA_Return_Freerec(&local_freerechead, config.max_records);

    MASSERT(VSTAILQ_EMPTY(&local_freerechead));
    MASSERT(global_nfree_rec == DEF_MAX_RECORDS);
    MASSERT(!VSTAILQ_EMPTY(&freerechead));
    VSTAILQ_FOREACH(entry, &freerechead, freelist) {
        MCHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
        cfree++;
    }
    MASSERT(global_nfree_rec == cfree);

    return NULL;
}

static const char
*test_data_return_chunk(void)
{
    unsigned cfree = 0;
    chunk_t *chunk;

    printf("... testing chunk freelist return\n");

    DATA_Return_Freechunk(&local_freechunk, nchunks);

    MASSERT(VSTAILQ_EMPTY(&local_freechunk));
    MASSERT(global_nfree_chunk == nchunks);
    MASSERT(!VSTAILQ_EMPTY(&freechunkhead));
    VSTAILQ_FOREACH(chunk, &freechunkhead, freelist) {
        MCHECK_OBJ_NOTNULL(chunk, CHUNK_MAGIC);
        cfree++;
    }
    MASSERT(global_nfree_chunk == cfree);

    return NULL;
}

static const char
*test_data_prepend(void)
{
    dataentry *entry;
    int n = 0;

    printf("... testing freelist prepend\n");

    MASSERT(VSTAILQ_EMPTY(&local_freerechead));
    /* Return an empty list */
    DATA_Return_Freerec(&local_freerechead, 0);
    MASSERT(VSTAILQ_EMPTY(&local_freerechead));
    MASSERT(global_nfree_rec == config.max_records);

    DATA_Take_Freerec(&local_freerechead);
    VSTAILQ_INIT(&local_freerechead);
    /* insert the first 10 records to the local list */
    for (int i = 0; i < 10; i++)
        VSTAILQ_INSERT_TAIL(&local_freerechead, &entrytbl[i], freelist);
    /* Prepend them to the global free list */
    DATA_Return_Freerec(&local_freerechead, 10);
    /* insert the next 10 records */
    VSTAILQ_INIT(&local_freerechead);
    for (int i = 10; i < 20; i++)
        VSTAILQ_INSERT_TAIL(&local_freerechead, &entrytbl[i], freelist);
    /* Prepend them to the global list */
    DATA_Return_Freerec(&local_freerechead, 10);
    /*
     * Take the global list, and verify that records 10-19 are at the front,
     * followed by records 0-9.
     */
    DATA_Take_Freerec(&local_freerechead);
    VSTAILQ_FOREACH(entry, &local_freerechead, freelist) {
        if (n < 10)
            MASSERT(entry == &entrytbl[n + 10]);
        else
            MASSERT(entry == &entrytbl[n - 10]);
        n++;
    }
    MASSERT(n == 20);

    return NULL;
}

static const char
*test_data_clear(void)
{
#define CHUNKS_PER_REC 4
    dataentry entry;
    chunk_t c[CHUNKS_PER_REC], *chunk;
    int n = 0;
    unsigned nfree_chunks;

    printf("... testing data record clear\n");

    VSTAILQ_INIT(&local_freechunk);

    entry.magic = DATA_MAGIC;
    VSTAILQ_INIT(&entry.chunks);
    entry.end = 4711;
    entry.keylen = 815;
    entry.key = (char *) malloc(config.maxkeylen);
    for (int i = 0; i < CHUNKS_PER_REC; i++) {
        VSTAILQ_INSERT_TAIL(&entry.chunks, &c[i], chunklist);
        c[i].magic = CHUNK_MAGIC;
        c[i].data = (char *) malloc(config.chunk_size);
    }

    nfree_chunks = DATA_Reset(&entry, &local_freechunk);

    MASSERT(nfree_chunks == CHUNKS_PER_REC);

    MCHECK_OBJ(&entry, DATA_MAGIC);
    MASSERT(!OCCUPIED(&entry));
    MAZ(entry.end);
    MAZ(entry.keylen);
    MAZ(entry.curchunk);
    MAZ(entry.curchunkidx);
    MASSERT(EMPTY(entry.key));
    MASSERT(VSTAILQ_EMPTY(&entry.chunks));
    free(entry.key);

    MASSERT(!VSTAILQ_EMPTY(&local_freechunk));
    VSTAILQ_FOREACH(chunk, &local_freechunk, freelist) {
        MCHECK_OBJ_NOTNULL(chunk, CHUNK_MAGIC);
        MASSERT(!OCCUPIED(chunk));
        MAZ(chunk->data[0]);
        n++;
        free(chunk->data);
    }
    MASSERT(n == CHUNKS_PER_REC);

    return NULL;
}

static const char
*all_tests(void)
{
    mu_run_test(test_data_init);
    mu_run_test(test_data_take_rec);
    mu_run_test(test_data_take_chunk);
    mu_run_test(test_data_return_rec);
    mu_run_test(test_data_return_chunk);
    mu_run_test(test_data_prepend);
    mu_run_test(test_data_clear);

    return NULL;
}

TEST_RUNNER
