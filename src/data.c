/*-
 * Copyright (c) 2012-2015 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012-2015 Otto Gmbh & Co KG
 * All rights reserved
 * Use only with permission
 *
 * Authors: Geoffrey Simmons <geoffrey.simmons@uplex.de>
 *	    Nils Goroll <nils.goroll@uplex.de>
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
#include "data.h"

#include "vas.h"
#include "miniobj.h"
#include "vdef.h"
#include "vsb.h"

/* Preprend head2 before head1, result in head1, head2 empty afterward */
#define	VSTAILQ_PREPEND(head1, head2) do {                      \
        if (VSTAILQ_EMPTY((head2)))                             \
            break;                                              \
	if (VSTAILQ_EMPTY((head1)))                             \
            (head1)->vstqh_last = (head2)->vstqh_last;          \
	else                                                    \
            *(head2)->vstqh_last = VSTAILQ_FIRST((head1));      \
        VSTAILQ_FIRST((head1)) = VSTAILQ_FIRST((head2));        \
        VSTAILQ_INIT((head2));                                  \
} while (0)

static pthread_mutex_t freerec_lock, freechunk_lock;
static char *buf, *keybuf;

static void
data_Cleanup(void)
{
    free(chunktbl);
    free(entrytbl);
    free(keybuf);
    free(buf);
    AZ(pthread_mutex_destroy(&freerec_lock));
    AZ(pthread_mutex_destroy(&freechunk_lock));
}

int
DATA_Init(void)
{
    unsigned chunks_per_rec
        = (config.max_reclen + config.chunk_size - 1) / config.chunk_size;
    unsigned nchunks = chunks_per_rec * config.max_records;

    entrytbl = (dataentry *) calloc(config.max_records, sizeof(dataentry));
    if (entrytbl == NULL)
        return(errno);

    chunktbl = (chunk_t *) calloc(nchunks, sizeof(chunk_t));
    if (chunktbl == NULL) {
        free(entrytbl);
        return(errno);
    }

    buf = (char *) calloc(nchunks, config.chunk_size);
    if (buf == NULL) {
        free(entrytbl);
        free(chunktbl);
        return(errno);
    }

    keybuf = (char *) calloc(config.max_records, config.maxkeylen);
    if (keybuf == NULL) {
        free(entrytbl);
        free(chunktbl);
        free(buf);
        return(errno);
    }

    VSTAILQ_INIT(&freechunkhead);
    VSTAILQ_INIT(&freerechead);
    AZ(pthread_mutex_init(&freerec_lock, NULL));
    AZ(pthread_mutex_init(&freechunk_lock, NULL));

    global_nfree_rec = config.max_records;
    global_nfree_chunk = nchunks;

    for (int i = 0; i < nchunks; i++) {
        chunktbl[i].magic = CHUNK_MAGIC;
        chunktbl[i].data = &buf[i * config.chunk_size];
        VSTAILQ_INSERT_TAIL(&freechunkhead, &chunktbl[i], freelist);
    }

    for (unsigned i = 0; i < config.max_records; i++) {
        entrytbl[i].magic = DATA_MAGIC;
        entrytbl[i].key = &keybuf[(i * config.maxkeylen)];
        VSTAILQ_INIT(&entrytbl[i].chunks);
        VSTAILQ_INSERT_TAIL(&freerechead, &entrytbl[i], freelist);
    }

    atexit(data_Cleanup);
    return(0);
}

unsigned
DATA_Reset(dataentry *entry, chunkhead_t * const freechunk)
{
    chunk_t *chunk;
    unsigned nchunk = 0;

    CHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
    entry->occupied = 0;
    entry->end = 0;
    entry->keylen = 0;
    *entry->key = '\0';
    entry->curchunk = NULL;
    entry->curchunkidx = 0;

    while ((chunk = VSTAILQ_FIRST(&entry->chunks)) != NULL) {
        CHECK_OBJ(chunk, CHUNK_MAGIC);
        chunk->occupied = 0;
        *chunk->data = '\0';
        VSTAILQ_REMOVE_HEAD(&entry->chunks, chunklist);
        VSTAILQ_INSERT_HEAD(freechunk, chunk, freelist);
        nchunk++;
    }
    assert(VSTAILQ_EMPTY(&entry->chunks));
    return nchunk;
}

/* 
 * prepend a global freelist to the reader's freelist for access with rare
 * locking
 */
#define DATA_Take_Free(type)                            \
unsigned                                                \
DATA_Take_Free##type(struct type##head_s *dst)          \
{                                                       \
    unsigned nfree;                                     \
                                                        \
    AZ(pthread_mutex_lock(&free##type##_lock));         \
    VSTAILQ_PREPEND(dst, &free##type##head);            \
    nfree = global_nfree_##type;                        \
    global_nfree_##type = 0;                            \
    AZ(pthread_mutex_unlock(&free##type##_lock));       \
    return nfree;                                       \
}

DATA_Take_Free(rec)
DATA_Take_Free(chunk)

/*
 * return to global freelist
 * returned must be locked by caller, if required
 */
#define DATA_Return_Free(type)                                          \
void                                                                    \
DATA_Return_Free##type(struct type##head_s *returned, unsigned nreturned) \
{                                                                       \
    AZ(pthread_mutex_lock(&free##type##_lock));                         \
    VSTAILQ_PREPEND(&free##type##head, returned);                       \
    global_nfree_##type += nreturned;                                   \
    AZ(pthread_mutex_unlock(&free##type##_lock));                       \
}

DATA_Return_Free(rec)
DATA_Return_Free(chunk)

void
DATA_Dump(void)
{
    struct vsb *data = VSB_new_auto();

    for (int i = 0; i < config.max_records; i++) {
        dataentry *entry = &entrytbl[i];

        if (entry == NULL)
            continue;
        if (entry->magic != DATA_MAGIC) {
            LOG_Log(LOG_ERR, "Invalid data entry at index %d, magic = 0x%08x, "
                    "expected 0x%08x", i, entry->magic, DATA_MAGIC);
        }
        if (!OCCUPIED(entry))
            continue;

        VSB_clear(data);
        if (entry->end) {
            int n = entry->end;
            chunk_t *chunk = VSTAILQ_FIRST(&entry->chunks);
            while (n > 0 && chunk != NULL) {
                if (chunk->magic != CHUNK_MAGIC) {
                    LOG_Log(LOG_ERR,
                            "Invalid chunk at index %d, magic = 0x%08x, "
                            "expected 0x%08x",
                            i, chunk->magic, CHUNK_MAGIC);
                    continue;
                }
                int cp = n;
                if (cp > config.chunk_size)
                    cp = config.chunk_size;
                VSB_bcat(data, chunk->data, cp);
                n -= cp;
                chunk = VSTAILQ_NEXT(chunk, chunklist);
            }
        }
        VSB_finish(data);
        LOG_Log(LOG_INFO,
                "Data entry %d: data=[%.*s] key=[%.*s]",
                i, entry->end, VSB_data(data), entry->keylen, entry->key);
    }
}
