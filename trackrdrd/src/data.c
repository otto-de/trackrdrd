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

#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

#include "libvarnish.h"
#include "miniobj.h"

#include "trackrdrd.h"

#define MIN_TABLE_SCALE 10
#define MIN_DATA_SCALE 10

#define INDEX(u) ((u) & (tbl.len - 1))

static const char *statename[3] = { "EMPTY", "OPEN", "DONE" };

/*
 * N.B.: Hash functions defined for XIDs, which are declared in Varnish as
 * unsigned int, assuming that they are 32 bit.
 */
#if UINT_MAX != UINT32_MAX
#error "Unsigned ints are not 32 bit"
#endif

#define rotr(v,n) (((v) >> (n)) | ((v) << (32 - (n))))
#define USE_JENKMULVEY1
#define h1(k) jenkmulvey1(k)
#define h2(k) wang(k)

#ifdef USE_JENKMULVEY1
/*
 * http://home.comcast.net/~bretm/hash/3.html
 * Bret Mulvey ascribes this to Bob Jenkins, but I can't find any
 * reference to it by Jenkins himself.
 */
static uint32_t
jenkmulvey1(uint32_t n)
{
    n += (n << 12);
    n ^= (n >> 22);
    n += (n << 4);
    n ^= (n >> 9);
    n += (n << 10);
    n ^= (n >> 2);
    n += (n << 7);
    n ^= (n >> 12);
    return(n);
}
#endif

#ifdef USE_JENKMULVEY2
/*
 * http://home.comcast.net/~bretm/hash/4.html
 * Mulvey's modification of the (alleged) Jenkins algorithm
 */
static uint32_t
jenkmulvey2(uint32_t n)
{
    n += (n << 16);
    n ^= (n >> 13);
    n += (n << 4);
    n ^= (n >> 7);
    n += (n << 10);
    n ^= (n >> 5);
    n += (n << 8);
    n ^= (n >> 16);
    return(n);
}
#endif

/*
 * http://www.cris.com/~Ttwang/tech/inthash.htm
 */
static uint32_t
wang(uint32_t n)
{
  n  = ~n + (n << 15); // n = (n << 15) - n - 1;
  n ^= rotr(n,12);
  n += (n << 2);
  n ^= rotr(n,4);
  n  = (n + (n << 3)) + (n << 11);
  n ^= rotr(n,16);
  return n;
}

static void
data_Cleanup(void)
{
    free(tbl.entry);
    free(tbl.buf);
}

static void
data_logstats(void)
{
    LOG_Log(LOG_INFO,
        "Data table: len=%d collisions=%d insert_probes=%d find_probes=%d "
        "open=%d done=%d load=%.2f occ_hi=%d seen=%d submitted=%d data_hi=%d",
        tbl.len, tbl.collisions, tbl.insert_probes, tbl.find_probes,
        tbl.open, tbl.done, 100.0 * ((float) tbl.open + tbl.done) / tbl.len,
        tbl.occ_hi, tbl.seen, tbl.submitted, tbl.data_hi);
}

int
DATA_Init(void)
{
    dataentry *entryptr;
    char *bufptr;
    
    int bufsize = 1 << (config.maxdata_scale + MIN_DATA_SCALE);
    int entries = 1 << (config.maxopen_scale + MIN_TABLE_SCALE);

    entryptr = (dataentry *) calloc(entries, sizeof(dataentry));
    if (entryptr == NULL)
        return(errno);

    bufptr = (char *) calloc(entries, bufsize);
    if (bufptr == NULL) {
        free(entryptr);
        return(errno);
    }

    datatable init_tbl =
        { .magic = DATATABLE_MAGIC, .len = entries, .collisions = 0,
          .insert_probes = 0, .find_probes = 0, .seen = 0, .open = 0, .done = 0,
          .submitted = 0, .occ_hi = 0, .data_hi = 0, .entry = entryptr,
          .buf = bufptr };
    memcpy(&tbl, &init_tbl, sizeof(datatable));

    for (int i = 0; i < entries; i++) {
        tbl.entry[i].magic = DATA_MAGIC;
        tbl.entry[i].state = DATA_EMPTY;
        tbl.entry[i].data = &tbl.buf[i * bufsize];
    }
    atexit(data_Cleanup);
    return(0);
}

dataentry 
*DATA_Insert(unsigned xid)
{
    uint32_t h = h1(xid);
    if (tbl.entry[INDEX(h)].state == DATA_EMPTY)
        return(&tbl.entry[INDEX(h)]);
    
    unsigned probes = 0;
    tbl.collisions++;
    h += h2(xid);
    while (++probes <= tbl.len && tbl.entry[INDEX(h)].state != DATA_EMPTY)
        h++;
    tbl.insert_probes += probes;
    if (probes > tbl.len)
        return(NULL);
    return(&tbl.entry[INDEX(h)]);
}

dataentry
*DATA_Find(unsigned xid)
{
    uint32_t h = h1(xid);
    if (tbl.entry[INDEX(h)].xid == xid)
        return &tbl.entry[INDEX(h)];
    h += h2(xid);
    unsigned probes = 0;
    while (++probes <= tbl.len && tbl.entry[INDEX(h)].xid != xid)
        h++;
    tbl.find_probes += probes;
    if (probes > tbl.len)
        return NULL;
    return &tbl.entry[INDEX(h)];
}

void
DATA_Dump(void)
{
    for (int i = 0; i < tbl.len; i++) {
        dataentry entry = tbl.entry[i];
        if (entry.state == DATA_EMPTY)
            continue;
        LOG_Log(LOG_INFO, "Data entry %d: XID=%d tid=%d state=%s data=[%.*s]",
            i, entry.xid, entry.tid, statename[entry.state], entry.end,
            entry.data);
    }
    data_logstats();
}
