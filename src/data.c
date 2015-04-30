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

static pthread_mutex_t freelist_lock;
static char *buf;

static void
data_Cleanup(void)
{
    free(entrytbl);
    free(buf);
    AZ(pthread_mutex_destroy(&freelist_lock));
}

int
DATA_Init(void)
{
    unsigned bufsize = config.max_reclen + config.maxkeylen;
    
    /*
     * we want enough space to accomodate all open and done records
     *
     */
    entrytbl = (dataentry *) calloc(config.max_records, sizeof(dataentry));
    if (entrytbl == NULL)
        return(errno);

    buf = (char *) calloc(config.max_records, bufsize);
    if (buf == NULL) {
        free(entrytbl);
        return(errno);
    }

    VSTAILQ_INIT(&freehead);
    AZ(pthread_mutex_init(&freelist_lock, NULL));

    global_nfree  = 0;

    for (unsigned i = 0; i < config.max_records; i++) {
        entrytbl[i].magic = DATA_MAGIC;
        entrytbl[i].data = &buf[i * bufsize];
        entrytbl[i].key = &buf[(i * bufsize) + config.max_reclen];
        VSTAILQ_INSERT_TAIL(&freehead, &entrytbl[i], freelist);
        global_nfree++;
    }
    assert(global_nfree == config.max_records);
    assert(VSTAILQ_FIRST(&freehead));

    atexit(data_Cleanup);
    return(0);
}

void
DATA_Reset(dataentry *entry)
{
    CHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
    entry->occupied = 0;
    entry->end = 0;
    *entry->data = '\0';
    entry->keylen = 0;
    *entry->key = '\0';
    entry->hasdata = 0;
    entry->reqend_t.tv_sec = 0;
    entry->reqend_t.tv_usec = 0;
}

/* 
 * take all free entries from the datatable for lockless
 * allocation
 */

unsigned
DATA_Take_Freelist(struct freehead_s *dst)
{
    unsigned nfree;

    AZ(pthread_mutex_lock(&freelist_lock));
    nfree = global_nfree;
    global_nfree = 0;
    VSTAILQ_PREPEND(dst, &freehead);
    AZ(pthread_mutex_unlock(&freelist_lock));
    return nfree;
}

/*
 * return to freehead
 *
 * returned must be locked by caller, if required
 */
void
DATA_Return_Freelist(struct freehead_s *returned, unsigned nreturned)
{
    AZ(pthread_mutex_lock(&freelist_lock));
    VSTAILQ_PREPEND(&freehead, returned);
    global_nfree += nreturned;
    AZ(pthread_mutex_unlock(&freelist_lock));
}

void
DATA_Dump(void)
{
    for (int i = 0; i < config.max_records; i++) {
        dataentry *entry = &entrytbl[i];

        if (!OCCUPIED(entry))
            continue;
        LOG_Log(LOG_INFO,
                "Data entry %d: data=[%.*s] key=[%.*s] reqend_t=%u.%06u",
                i, entry->end, entry->data, entry->keylen, entry->key,
                entry->reqend_t.tv_sec, entry->reqend_t.tv_usec);
    }
}
