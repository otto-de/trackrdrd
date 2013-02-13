/*-
 * Copyright (c) 2012 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012 Otto Gmbh & Co KG
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
#if 0
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <stdbool.h>

#include "libvarnish.h"
#include "miniobj.h"

#include "trackrdrd.h"

#define INDEX(u) ((u) & (htbl.len - 1))

#ifdef UNUSED
static const char *statename[2] = { "EMPTY", "OPEN" };
#endif

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
hash_Cleanup(void)
{
    free(htbl.entry);
}

/*
 * all hash functions must only ever be called by the
 * vsl reader thread!
 */
int
HASH_Init(void)
{
    hashentry *entryptr;
    
    int entries = 1 << config.maxopen_scale;

    entryptr = (hashentry *) calloc(entries, sizeof(hashentry));
    if (entryptr == NULL)
        return(errno);

    memset(&htbl, 0, sizeof(hashtable));
    htbl.magic	= HASHTABLE_MAGIC;
    htbl.len	= entries;
    htbl.entry	= entryptr;

    VTAILQ_INIT(&htbl.insert_head);

    htbl.max_probes = config.hash_max_probes;
    htbl.ttl = config.hash_ttl;
    htbl.mlt = config.hash_mlt;

    /* entries init */
    for (int i = 0; i < entries; i++) {
	htbl.entry[i].magic = HASH_MAGIC;
        htbl.entry[i].state = HASH_EMPTY;
    }
    atexit(hash_Cleanup);
    return(0);
}

static inline void
hash_free(hashentry *he)
{
	VTAILQ_REMOVE(&htbl.insert_head, he, insert_list);
	he->state = HASH_EMPTY;
	he->de = NULL;
	htbl.open--;
}

static inline void
hash_submit(hashentry *he)
{
	dataentry *de = he->de;

	assert(he->xid == de->xid);
	DATA_noMT_Submit(de);
}

static inline void
incomplete(hashentry *he)
{
	dataentry *de;
	de = he->de;
	CHECK_OBJ_NOTNULL(de, DATA_MAGIC);
	de->incomplete = true;
        MON_StatsUpdate(STATS_DONE);
	de->state = DATA_DONE;
}

void
HASH_Exp(float limit)
{
	hashentry *he;

	float p_insert_time = 0.0;

	while ((he = VTAILQ_FIRST(&htbl.insert_head))) {
		CHECK_OBJ_NOTNULL(he, HASH_MAGIC);
		assert(he->state == HASH_OPEN);

		if (he->insert_time > limit)
			return;

		assert(p_insert_time <= he->insert_time);
		p_insert_time = he->insert_time;

		LOG_Log(LOG_DEBUG, "expire: hash=%u insert_time=%f limit=%f",
		    he->xid, he->insert_time, limit);
		htbl.expired++;

		incomplete(he);
		hash_submit(he);
		hash_free(he);
	}
}

void
HASH_Submit(hashentry *he)
{
	CHECK_OBJ_NOTNULL(he, HASH_MAGIC);
	assert(he->state == HASH_OPEN);

	LOG_Log(LOG_DEBUG, "submit: hash=%u", he->xid);

	hash_submit(he);
	hash_free(he);
}

/* like Submit, but for recrods in HASH_OPEN */
void
HASH_Evacuate(hashentry *he)
{
	CHECK_OBJ_NOTNULL(he, HASH_MAGIC);
	assert(he->state == HASH_OPEN);

	LOG_Log(LOG_DEBUG, "evacuate: hash=%u insert_time=%f",
	    he->xid, he->insert_time);
	htbl.evacuated++;

	incomplete(he);
	hash_submit(he);
	hash_free(he);
}

hashentry
*HASH_Insert(const unsigned xid, dataentry *de, const float t)
{
    hashentry *he, *oldest;
    unsigned probes	= 0;
    uint32_t h  	= h1(xid);

    const uint32_t h2	= h2(xid);

    he = &htbl.entry[INDEX(h)];
    if (he->state == HASH_EMPTY)
	    goto ok;

    htbl.collisions++;
    
    oldest = he;
    do {
	    h += h2;
	    he = &htbl.entry[INDEX(h)];
	    probes++;

	    if (he->state == HASH_EMPTY)
		    goto ok;
	    if (he->insert_time < oldest->insert_time)
		    oldest = he;
    } while (probes <= htbl.max_probes);

    /* none eligable for evacuation */
    if ((oldest->insert_time + htbl.mlt) > t) {
	    htbl.fail++;
	    htbl.insert_probes += probes;
	    return (NULL);
    }

    HASH_Evacuate(oldest);
    he = oldest;

  ok:
    htbl.insert_probes += probes;

    he->state		= HASH_OPEN;
    he->xid		= xid;
    he->insert_time	= t;
    VTAILQ_INSERT_TAIL(&htbl.insert_head, he, insert_list);
    he->de		= de;

    /* stats */
    htbl.open++;
    if (htbl.open > htbl.occ_hi)
	    htbl.occ_hi = htbl.open;
    if (htbl.open > htbl.occ_hi_this)
	    htbl.occ_hi_this = htbl.open;

    return(he);
}

hashentry
*HASH_Find(const unsigned xid)
{
    hashentry *he;
    unsigned probes	= 0;
    uint32_t h  	= h1(xid);

    const uint32_t h2	= h2(xid);

    he = &htbl.entry[INDEX(h)];
    if (he->xid == xid)
	    return (he);

    do {
	    h += h2;
	    he = &htbl.entry[INDEX(h)];
	    probes++;

	    if (he->xid == xid)
		    break;
    } while (probes <= htbl.max_probes);

    htbl.find_probes += probes;
    if (probes > htbl.max_probes)
	    return NULL;
    return (he);
}

void
HASH_Dump1(hashentry *entry, int i)
{
        if (entry->state == HASH_EMPTY)
		return;
        LOG_Log(LOG_INFO, "Hash entry %d: XID=%d",
            i, entry->xid);
	DATA_Dump1(entry->de, 0);
	assert(entry->xid == entry->de->xid);
}

void
HASH_Dump(void)
{
	for (int i = 0; i < htbl.len; i++)
		HASH_Dump1(&htbl.entry[i], i);
}
#endif
