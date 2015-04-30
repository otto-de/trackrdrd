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

static void
data_Cleanup(void)
{
    free(dtbl.entry);
    free(dtbl.buf);
    AZ(pthread_mutex_destroy(&dtbl.freelist_lock));
}

int
DATA_Init(void)
{
    dataentry *entryptr;
    char *bufptr;
    
    unsigned bufsize = config.maxdata + config.maxkeylen;
    
    /*
     * we want enough space to accomodate all open and done records
     *
     */
    entryptr = (dataentry *) calloc(config.maxdone, sizeof(dataentry));
    if (entryptr == NULL)
        return(errno);

    bufptr = (char *) calloc(config.maxdone, bufsize);
    if (bufptr == NULL) {
        free(entryptr);
        return(errno);
    }

    memset(&dtbl, 0, sizeof(datatable));
    dtbl.magic	= DATATABLE_MAGIC;
    dtbl.len	= config.maxdone;

    VSTAILQ_INIT(&dtbl.freehead);
    AZ(pthread_mutex_init(&dtbl.freelist_lock, NULL));

    dtbl.entry	= entryptr;
    dtbl.buf	= bufptr;
    dtbl.nfree  = 0;

    for (unsigned i = 0; i < config.maxdone; i++) {
        dtbl.entry[i].magic = DATA_MAGIC;
        dtbl.entry[i].data = &dtbl.buf[i * bufsize];
        dtbl.entry[i].key = &dtbl.buf[(i * bufsize) + config.maxdata];
        VSTAILQ_INSERT_TAIL(&dtbl.freehead, &dtbl.entry[i], freelist);
        dtbl.nfree++;
    }
    assert(dtbl.nfree == config.maxdone);
    assert(VSTAILQ_FIRST(&dtbl.freehead));

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
    entry->xid = 0;
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

    AZ(pthread_mutex_lock(&dtbl.freelist_lock));
    nfree = dtbl.nfree;
    dtbl.nfree = 0;
    VSTAILQ_PREPEND(dst, &dtbl.freehead);
    AZ(pthread_mutex_unlock(&dtbl.freelist_lock));
    return nfree;
}

/*
 * return to dtbl.freehead
 *
 * returned must be locked by caller, if required
 */
void
DATA_Return_Freelist(struct freehead_s *returned, unsigned nreturned)
{
    AZ(pthread_mutex_lock(&dtbl.freelist_lock));
    VSTAILQ_PREPEND(&dtbl.freehead, returned);
    dtbl.nfree += nreturned;
    AZ(pthread_mutex_unlock(&dtbl.freelist_lock));
}

void
DATA_Dump(void)
{
    for (int i = 0; i < dtbl.len; i++) {
        dataentry *entry = &dtbl.entry[i];

        if (!OCCUPIED(entry))
            continue;
        LOG_Log(LOG_INFO,
                "Data entry %d: XID=%u data=[%.*s] key=[%.*s] reqend_t=%u.%06u",
                i, entry->xid, entry->end, entry->data, entry->keylen,
                entry->key, entry->reqend_t.tv_sec, entry->reqend_t.tv_usec);
    }
}
