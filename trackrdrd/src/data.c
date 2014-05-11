/*-
 * Copyright (c) 2012-2014 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012-2014 Otto Gmbh & Co KG
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
#include "trackrdrd.h"
#include "vas.h"
#include "miniobj.h"

static const char *statename[3] = { "EMPTY", "OPEN", "DONE" };

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
    
    unsigned bufsize = config.maxdata;
    
    /*
     * we want enough space to accomodate all open and done records
     *
     */
    unsigned entries = (1 << config.maxopen_scale) + config.maxdone;

    entryptr = (dataentry *) calloc(entries, sizeof(dataentry));
    if (entryptr == NULL)
        return(errno);

    bufptr = (char *) calloc(entries, bufsize);
    if (bufptr == NULL) {
        free(entryptr);
        return(errno);
    }

    memset(&dtbl, 0, sizeof(datatable));
    dtbl.magic	= DATATABLE_MAGIC;
    dtbl.len	= entries;

    VSTAILQ_INIT(&dtbl.freehead);
    AZ(pthread_mutex_init(&dtbl.freelist_lock, &attr_lock));

    dtbl.entry	= entryptr;
    dtbl.buf	= bufptr;
    dtbl.nfree  = 0;

    for (unsigned i = 0; i < entries; i++) {
        dtbl.entry[i].magic = DATA_MAGIC;
        dtbl.entry[i].state = DATA_EMPTY;
        dtbl.entry[i].hasdata = false;
        dtbl.entry[i].data = &dtbl.buf[i * bufsize];
        VSTAILQ_INSERT_TAIL(&dtbl.freehead, &dtbl.entry[i], freelist);
        dtbl.nfree++;
    }
    assert(dtbl.nfree == entries);
    assert(VSTAILQ_FIRST(&dtbl.freehead));

    atexit(data_Cleanup);
    return(0);
}

/* 
 * take all free entries from the datatable for lockless
 * allocation
 */

void
DATA_Take_Freelist(struct freehead_s *dst)
{
    AZ(pthread_mutex_lock(&dtbl.freelist_lock));
    VSTAILQ_CONCAT(dst, &dtbl.freehead);
    dtbl.nfree = 0;
    AZ(pthread_mutex_unlock(&dtbl.freelist_lock));
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
    VSTAILQ_CONCAT(&dtbl.freehead, returned);
    dtbl.nfree += nreturned;
    AZ(pthread_mutex_unlock(&dtbl.freelist_lock));
}

void
DATA_Dump1(dataentry *entry, int i)
{
    if (entry->state == DATA_EMPTY)
        return;
    LOG_Log(LOG_INFO, "Data entry %d: XID=%d tid=%d state=%s data=[%.*s]",
        i, entry->xid, entry->tid, statename[entry->state], entry->end,
        entry->data);
}

void
DATA_Dump(void)
{
    for (int i = 0; i < dtbl.len; i++)
        DATA_Dump1(&dtbl.entry[i], i);
}
