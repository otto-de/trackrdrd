/*-
 * Copyright (c) 2012-2014 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012-2014 Otto Gmbh & Co KG
 * All rights reserved
 * Use only with permission
 *
 * Authors: Geoffrey Simmons <geoffrey.simmons@uplex.de>
 *	    Nils Goroll <nils.goroll@uplex.de>
 *
 * Portions adopted from varnishlog.c from the Varnish project
 *	Author: Poul-Henning Kamp <phk@phk.freebsd.dk>
 * 	Copyright (c) 2006 Verdens Gang AS
 * 	Copyright (c) 2006-2011 Varnish Software AS
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
 * read tracking data from the Varnish SHM-log and send records to the
 * processor
 */

#include "config.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <pwd.h>
#include <limits.h>
#include <stdarg.h>
#include <dlfcn.h>

#include "vsb.h"
#include "vpf.h"
#include "vmb.h"

#include "libvarnish.h"
#include "vsl.h"
#include "varnishapi.h"
#include "miniobj.h"

#include "trackrdrd.h"
#include "config_common.h"

#define TRACK_TAGS "ReqStart,VCL_log,ReqEnd"

/* XXX: should these be configurable ? */
#define TRACKLOG_PREFIX "track "
#define TRACKLOG_PREFIX_LEN (sizeof(TRACKLOG_PREFIX)-1)
#define REQEND_T_VAR "req_endt"

static struct sigaction dump_action;

static int wrk_running = 0;

/* Local freelist */
static struct freehead_s reader_freelist = 
    VSTAILQ_HEAD_INITIALIZER(reader_freelist);

typedef enum {
    HASH_EMPTY = 0,
    /* OPEN when the main thread is filling data, ReqEnd not yet seen. */
    HASH_OPEN
    /* hashes become HASH_EMPTY for DATA_DONE */
} hash_state_e;

struct hashentry_s {
    unsigned magic;
#define HASH_MAGIC 0xf8e12130

    /* set in HASH_Insert */
    hash_state_e state;
    unsigned xid; /* == de->xid */
    float insert_time;
    VTAILQ_ENTRY(hashentry_s) insert_list;
    
    dataentry *de;
};

typedef struct hashentry_s hashentry;

VTAILQ_HEAD(insert_head_s, hashentry_s);

struct hashtable_s {
    unsigned	magic;
#define HASHTABLE_MAGIC	0x89ea1d00
    const unsigned	len;
    hashentry	*entry;

    struct insert_head_s	insert_head;

    /* config */
    unsigned	max_probes;
    float	ttl;		/* max age for a record */
    float	mlt;		/* min life time */

    /* == stats == */
    unsigned    seen;		/* Records (ReqStarts) seen */
    /* 
     * records we have dropped because of no hash, no data
     * or no entry
     */
    unsigned	drop_reqstart;
    unsigned	drop_vcl_log;
    unsigned	drop_reqend;

    unsigned	expired;
    unsigned	evacuated;
    unsigned    open;
    
    unsigned	collisions;
    unsigned	insert_probes;
    unsigned	find_probes;
    unsigned	fail;		/* failed to get record - no space */

    unsigned	occ_hi;		/* Occupancy high water mark */ 
    unsigned	occ_hi_this;	/* Occupancy high water mark this reporting
                                   interval */
};

typedef struct hashtable_s hashtable;

static hashtable htbl;

#ifdef WITHOUT_ASSERTS
#define entry_assert(e, cond) do { (void)(e);(void)(cond);} while(0)
#else /* WITH_ASSERTS */
#define entry_assert(e, cond)						\
    do {								\
        if (!(cond))                                                    \
            entry_assert_failure(__func__, __FILE__, __LINE__, #cond, (e), \
                errno, 0);                                              \
    } while (0)

static void
entry_assert_failure(const char *func, const char *file, int line,
    const char *cond, hashentry *he, int err, int xxx)
{
    dataentry *de = he->de;
    LOG_Log(LOG_ALERT,
        "Hashentry %p magic %0x state %u xid %u insert_time %f de %p",
        (he), (he)->magic, (he)->state, (he)->xid, (he)->insert_time, (he)->de);
    if (de)
        LOG_Log(LOG_ALERT,
            "Dataentry %p magic %0x state %u xid %u tid %u end %u",
            (de), (de)->magic, (de)->state, (de)->xid, (de)->tid, (de)->end);
    else
        LOG_Log(LOG_ALERT, "Dataentry %p NULL!", (de));
    ASRT_Fail(func, file, line, cond, err, xxx);
}
#endif

/*--------------------------------------------------------------------*/

static inline void
check_entry(hashentry *he, unsigned xid, unsigned fd)
{
    dataentry *de;
    CHECK_OBJ_NOTNULL(he, HASH_MAGIC);
    entry_assert(he, he->xid == xid);
    entry_assert(he, he->state == HASH_OPEN);

    de = he->de;
    entry_assert(he, de != NULL);
    entry_assert(he, de->magic == DATA_MAGIC);
    entry_assert(he, de->xid == xid);
    if (fd != (unsigned int) -1)
        entry_assert(he, de->tid == fd);
}

/*--------------------------------------------------------------------*/

/* efficiently retrieve a single data entry */

static inline dataentry
*data_get(void)
{
    dataentry *data;

    while (VSTAILQ_EMPTY(&reader_freelist)) {
        while (dtbl.nfree == 0) {
            dtbl.w_stats.wait_room++;
            spmcq_wait(room);
        }
        DATA_Take_Freelist(&reader_freelist);
    }
    data = VSTAILQ_FIRST(&reader_freelist);
    VSTAILQ_REMOVE_HEAD(&reader_freelist, freelist);
    assert(data->state == DATA_EMPTY);
    return (data);
}

/* return to our own local cache */

static inline void
data_free(dataentry *de)
{
    assert(de->state == DATA_EMPTY);
    VSTAILQ_INSERT_HEAD(&reader_freelist, de, freelist);
}

static inline void
data_submit(dataentry *de)
{
    CHECK_OBJ_NOTNULL(de, DATA_MAGIC);
    assert(de->state == DATA_DONE);
    LOG_Log(LOG_DEBUG, "submit: data=[%.*s]", de->end, de->data);
    if (de->hasdata == false) {
        de->state = DATA_EMPTY;
        MON_StatsUpdate(STATS_NODATA);
        data_free(de);
        return;
    }

    SPMCQ_Enq(de);
    dtbl.w_stats.submitted++;

    /* should we wake up another worker? */
    if (SPMCQ_NeedWorker(wrk_running))
        spmcq_signal(data);

    /*
     * base case: wake up a worker if all are sleeping
     *
     * this is an un-synced access to spmcq_data_waiter, but
     * if we don't wake them up now, we will next time around
     */
    if (wrk_running == spmcq_datawaiter)
        spmcq_signal(data);
}

/*--------------------------------------------------------------------*/

#define INDEX(u) ((u) & (htbl.len - 1))

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

void
HASH_Stats(void)
{
    LOG_Log(LOG_INFO,
        "Hash table: "
        "len=%u "
        "seen=%u "
        "drop_reqstart=%u "
        "drop_vcl_log=%u "
        "drop_reqend=%u "
        "expired=%u "
        "evacuated=%u "
        "open=%u "
        "load=%.2f "    
        "collisions=%u "
        "insert_probes=%u "
        "find_probes=%u "
        "fail=%u "
        "occ_hi=%u "
        "occ_hi_this=%u ",
        htbl.len,
        htbl.seen,
        htbl.drop_reqstart,
        htbl.drop_vcl_log,
        htbl.drop_reqend,
        htbl.expired,
        htbl.evacuated,
        htbl.open,
        100.0 * htbl.open / htbl.len,
        htbl.collisions,
        htbl.insert_probes,
        htbl.find_probes,
        htbl.fail,
        htbl.occ_hi,
        htbl.occ_hi_this);

    htbl.occ_hi_this = 0;
}

static void
hash_cleanup(void)
{
    free(htbl.entry);
}

static int
hash_init(void)
{
    hashentry *entryptr;
    
    int entries = 1 << config.maxopen_scale;

    entryptr = (hashentry *) calloc(entries, sizeof(hashentry));
    if (entryptr == NULL)
        return(errno);

    /* Struct initializer makes it possible to set len as a const field;
       trying to hint the compiler to use a constant when translating the
       INDEX() code.
    */
    hashtable init_tbl
        = { .magic = HASHTABLE_MAGIC, .len = entries, .entry = entryptr,
            .max_probes = config.hash_max_probes, .ttl = config.hash_ttl,
            .mlt = config.hash_mlt, .seen = 0, .drop_reqstart = 0,
            .drop_vcl_log = 0, .drop_reqend = 0, .expired = 0,
            .evacuated = 0, .open = 0, .collisions = 0, .insert_probes = 0,
            .find_probes = 0, .fail = 0, .occ_hi = 0, .occ_hi_this = 0
    };
    memcpy(&htbl, &init_tbl, sizeof(hashtable));

    VTAILQ_INIT(&htbl.insert_head);

    /* entries init */
    for (int i = 0; i < entries; i++) {
        htbl.entry[i].magic = HASH_MAGIC;
        htbl.entry[i].state = HASH_EMPTY;
    }
    atexit(hash_cleanup);
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
    data_submit(de);
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

static void
hash_exp(float limit)
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

static inline void
submit(hashentry *he)
{
    CHECK_OBJ_NOTNULL(he, HASH_MAGIC);
    assert(he->state == HASH_OPEN);

    LOG_Log(LOG_DEBUG, "submit: hash=%u", he->xid);

    hash_submit(he);
    hash_free(he);
}

/* like Submit, but for records in HASH_OPEN */
static void
hash_evacuate(hashentry *he)
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

static hashentry
*hash_insert(const unsigned xid, dataentry *de, const float t)
{
    hashentry *he, *oldest;
    unsigned probes	= 0;
    uint32_t h  	= h1(xid);

    he = &htbl.entry[INDEX(h)];
    if (he->state != HASH_EMPTY) {
        htbl.collisions++;
        oldest = he;

        h = h2(xid);
        unsigned n = 0;
        do {
            he = &htbl.entry[INDEX(h)];
            probes++;
            
            if (he->state == HASH_EMPTY)
                break;
            if (he->insert_time < oldest->insert_time)
                oldest = he;
            n++;
            h += n * n;
        } while (probes <= htbl.max_probes);

        if (he->state != HASH_EMPTY) {
            if ((oldest->insert_time + htbl.mlt) > t) {
                /* none eligible for evacuation */
                htbl.fail++;
                htbl.insert_probes += probes;
                return (NULL);
            }

            hash_evacuate(oldest);
            he = oldest;
        }
    }

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

static hashentry
*hash_find(const unsigned xid)
{
    hashentry *he;
    unsigned probes	= 0;
    uint32_t h  	= h1(xid);

    he = &htbl.entry[INDEX(h)];
    if (he->xid == xid && he->state == HASH_OPEN)
        return (he);

    h = h2(xid);
    unsigned n = 0;
    do {
        he = &htbl.entry[INDEX(h)];
        if (he->xid == xid && he->state == HASH_OPEN)
            break;
        probes++;
        n++;
        h += n * n;
    } while (probes <= htbl.max_probes);

    htbl.find_probes += probes;
    if (probes > htbl.max_probes)
        return NULL;
    return (he);
}

static void
hash_dump(void)
{
    for (int i = 0; i < htbl.len; i++) {
        if (htbl.entry[i].state == HASH_EMPTY)
            continue;
        LOG_Log(LOG_INFO, "Hash entry %d: XID=%d", i, htbl.entry[i].xid);
        DATA_Dump1(htbl.entry[i].de, 0);
    }
}

/*--------------------------------------------------------------------*/

static inline dataentry
*insert(unsigned xid, unsigned fd, float tim)
{
    dataentry *de = data_get();
    CHECK_OBJ_NOTNULL(de, DATA_MAGIC);
    assert(de->state == DATA_EMPTY);
    hashentry *he = hash_insert(xid, de, tim);
    CHECK_OBJ(he, HASH_MAGIC);

    if (! he) {
        LOG_Log(LOG_WARNING, "Insert: Could not insert hash for XID %d",
            xid);
        data_free(de);
        return (NULL);
    }

    /* he being filled out by Hash_Insert, we need to look after de */
    de->xid	= xid;
    de->state	= DATA_OPEN;
    de->tid	= fd;
    de->hasdata	= false;

    sprintf(de->data, "XID=%u", xid);
    de->end = strlen(de->data);
    if (de->end > dtbl.w_stats.data_hi)
        dtbl.w_stats.data_hi = de->end;
    MON_StatsUpdate(STATS_OCCUPANCY);
    
    return (de);
}

static inline void
append(dataentry *entry, enum VSL_tag_e tag, unsigned xid, char *data,
    int datalen)
{
    CHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
    /* Data overflow */
    if (entry->end + datalen + 1 > config.maxdata) {
        LOG_Log(LOG_ALERT,
            "%s: Data too long, XID=%d, current length=%d, "
            "DISCARDING data=[%.*s]", VSL_tags[tag], xid, entry->end,
            datalen, data);
        dtbl.w_stats.data_overflows++;
        return;
    }
        
    entry->data[entry->end] = '&';
    entry->end++;
    memcpy(&entry->data[entry->end], data, datalen);
    entry->end += datalen;
    if (entry->end > dtbl.w_stats.data_hi)
        dtbl.w_stats.data_hi = entry->end;
    return;
}

static inline void
addkey(dataentry *entry, enum VSL_tag_e tag, unsigned xid, char *key,
       int keylen)
{
    CHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
    if (keylen > config.maxkeylen) {
        LOG_Log(LOG_ALERT,
                "%s: Key too long, XID=%d, length=%d, "
                "DISCARDING key=[%.*s]", VSL_tags[tag], xid, keylen,
                keylen, key);
        dtbl.w_stats.key_overflows++;
        return;
    }
        
    memcpy(entry->key, key, keylen);
    entry->keylen = keylen;
    if (keylen > dtbl.w_stats.key_hi)
        dtbl.w_stats.key_hi = keylen;
    return;
}

/*
 * rules for reading VSL:
 *
 * Under all circumstances do we need to avoid to fall behind reading the VSL:
 * - if we miss ReqEnd, we will clobber our hash, which has a bunch of negative
 *   consequences:
 *   - hash lookups become inefficient
 *   - inserts become more likely to fail
 *   - before we had hash_Exp, the hash would become useless
 * - if the VSL wraps, we will see corrupt data
 *
 * so if we really cannot create an entry at ReqStart time, we need to thow
 * it away, and process the next log/end records to make room
 *
 */
static int
OSL_Track(void *priv, enum VSL_tag_e tag, unsigned fd, unsigned len,
          unsigned spec, const char *ptr, uint64_t bitmap)
{
    unsigned xid;
    hashentry *he;
    dataentry *de;
    int err, datalen;
    char *data, reqend_str[strlen(REQEND_T_VAR)+22];
    struct timespec reqend_t;
    float tim;
    vcl_log_t data_type;

    static float tim_exp_check = 0.0;

    /* wrap detection statistics */
    static const char *pptr = (const char *) UINTPTR_MAX;

    static unsigned wrap_start_xid = 0;
    static unsigned wrap_end_xid = 0;

    static unsigned last_start_xid = 0;
    static unsigned last_end_xid = 0;

    static unsigned xid_spread_sum = 0;
    static unsigned xid_spread_count = 0;

    (void) priv;
    (void) bitmap;

    if (term && htbl.open == 0)
        return 1;

    wrk_running = WRK_Running();
    
    /* spec != 'c' */
    if ((spec & VSL_S_CLIENT) == 0)
        LOG_Log(LOG_WARNING, "%s: Client bit ('c') not set [%.*s]",
            VSL_tags[tag], len, ptr);

    if (fd == (unsigned int) -1)
        LOG_Log(LOG_WARNING, "%s: File descriptor not set [%.*s]",
            VSL_tags[tag], len, ptr);
    
    switch (tag) {
    case SLT_ReqStart:
        if (term) return 0;
        
        htbl.seen++;
        err = Parse_ReqStart(ptr, len, &xid);
        AZ(err);
        LOG_Log(LOG_DEBUG, "%s: XID=%u", VSL_tags[tag], xid);

        if (xid > last_start_xid)
            last_start_xid = xid;

        tim = TIM_mono();
        if (! insert(xid, fd, tim)) {
            htbl.drop_reqstart++;
            break;
        }

        /* configurable ? */
        if ((tim - tim_exp_check) > 10) {
            hash_exp(tim - htbl.ttl);
            tim_exp_check = tim;
        }
        break;

    case SLT_VCL_Log:
        /* Skip VCL_Log entries without the "track " prefix. */
        if (strncmp(ptr, TRACKLOG_PREFIX, TRACKLOG_PREFIX_LEN) != 0)
            break;
        
        err = Parse_VCL_Log(&ptr[TRACKLOG_PREFIX_LEN], len-TRACKLOG_PREFIX_LEN,
                            &xid, &data, &datalen, &data_type);
        if (err != 0) {
            LOG_Log(LOG_ERR,
                    "Cannot parse VCL_Log entry, DISCARDING [%.*s]: %s",
                    datalen, data, strerror(err));
            htbl.drop_vcl_log++;
        }

        LOG_Log(LOG_DEBUG, "%s: XID=%u, %s=[%.*s]", VSL_tags[tag],
                xid, data_type == VCL_LOG_DATA ? "data" : "key", datalen, data);

        he = hash_find(xid);
        if (he == NULL) {
            if (!term) {
                LOG_Log(LOG_WARNING, "%s: XID %d not found",
                    VSL_tags[tag], xid);
                htbl.drop_vcl_log++;
            }
            break;
        }

        check_entry(he, xid, fd);
        de = he->de;

        if (data_type == VCL_LOG_DATA) {
            append(de, tag, xid, data, datalen);
            de->hasdata = true;
        }
        else
            addkey(de, tag, xid, data, datalen);

        break;

    case SLT_ReqEnd:

        err = Parse_ReqEnd(ptr, len, &xid, &reqend_t);
        AZ(err);
        LOG_Log(LOG_DEBUG, "%s: XID=%u req_endt=%u.%09lu", VSL_tags[tag], xid,
            (unsigned) reqend_t.tv_sec, reqend_t.tv_nsec);

        if (xid > last_end_xid)
            last_end_xid = xid;

        xid_spread_sum += (last_end_xid - last_start_xid);
        xid_spread_count++;

        he = hash_find(xid);
        if (he == NULL) {
            if (!term) {
                LOG_Log(LOG_WARNING, "%s: XID %d not found",
                    VSL_tags[tag], xid);
                htbl.drop_reqend++;
            }
            break;
        }
        check_entry(he, xid, fd);
        de = he->de;

        sprintf(reqend_str, "%s=%u.%09lu", REQEND_T_VAR,
            (unsigned) reqend_t.tv_sec, reqend_t.tv_nsec);
        append(de, tag, xid, reqend_str, strlen(reqend_str));
        VWMB();
        de->state = DATA_DONE;
        MON_StatsUpdate(STATS_DONE);
        submit(he);
        break;

    default:
        /* Unreachable */
        AN(NULL);
        return(1);
    }

    /* 
     * log when the vsl ptr wraps, to try to monitor lost records
     * XXX: this doesn't work when XIDs wrap at UINT_MAX
     */
    if (ptr < pptr) {
        if (wrap_start_xid) {
            LOG_Log(LOG_INFO,
                "VSL wrap: delta_start=%u delta_end=%u delta_avg=%f",
                last_start_xid - wrap_start_xid, last_end_xid - wrap_end_xid,
                1.0 * xid_spread_sum / xid_spread_count);
            xid_spread_count = xid_spread_sum = 0;
        }
        wrap_start_xid = last_start_xid;
        wrap_end_xid = last_end_xid;
    }
    pptr = ptr;

    if (WRK_Exited() - dtbl.w_stats.abandoned > 0
        || (config.nworkers > 0 && dtbl.w_stats.abandoned == config.nworkers))
        return 1;
    
    return 0;
}

/*--------------------------------------------------------------------*/

static void
dump(int sig)
{
    (void) sig;
    hash_dump();
}

/* Matches typedef VSM_diag_f in include/varnishapi.h
   Log error messages from VSL_Open and VSL_Arg */
static void
vsl_diag(void *priv, const char *fmt, ...)
{
    (void) priv;
    
    va_list ap;
    va_start(ap, fmt);
    logconf.log(LOG_ERR, fmt, ap);
    va_end(ap);
}

static const char *
vsm_name(struct VSM_data *vd)
{
    const char * name = VSM_Name(vd);
    if (name && *name)
        return name;
    else
        return "default";
}

void
CHILD_Main(struct VSM_data *vd, int endless, int readconfig)
{
    int errnum, giveup = 0;;
    const char *errmsg;
    pthread_t monitor;
    struct passwd *pw;
    void *mqh;

    AZ(pthread_mutexattr_init(&attr_lock));
    AZ(pthread_condattr_init(&attr_cond));
    // important to make mutex/cv efficient
    AZ(pthread_mutexattr_setpshared(&attr_lock, PTHREAD_PROCESS_PRIVATE));
    AZ(pthread_condattr_setpshared(&attr_cond,  PTHREAD_PROCESS_PRIVATE)); 

    MON_StatsInit();
        
    LOG_Log0(LOG_INFO, "Worker process starting");

    /* XXX: does not re-configure logging. Feature or bug? */
    if (readconfig) {
        LOG_Log0(LOG_INFO, "Re-reading config");
        CONF_Init();
        CONF_ReadDefault();
        if (! EMPTY(cli_config_filename))
            LOG_Log(LOG_INFO, "Reading config from %s", cli_config_filename);
            /* XXX: CONF_ReadFile prints err messages to stderr */
        if (CONF_ReadFile(cli_config_filename, CONF_Add) != 0) {
                LOG_Log(LOG_ERR, "Error reading config from %s",
                    cli_config_filename);
                exit(EXIT_FAILURE);
            }
    }
    
    PRIV_Sandbox();
    pw = getpwuid(geteuid());
    AN(pw);
    LOG_Log(LOG_INFO, "Running as %s", pw->pw_name);

    /* read messaging module */
    if (config.mq_module[0] == '\0') {
        LOG_Log0(LOG_ALERT, "mq.module not found in config (required)");
        exit(EXIT_FAILURE);
    }
    dlerror(); // to clear errors
    mqh = dlopen(config.mq_module, RTLD_NOW);
    if ((errmsg = dlerror()) != NULL) {
        LOG_Log(LOG_ALERT, "error reading mq module %s: %s", config.mq_module,
                errmsg);
        exit(EXIT_FAILURE);
    }

#define METHOD(instm, intfm)                                            \
    mqf.instm = dlsym(mqh, #intfm);                                     \
    if ((errmsg = dlerror()) != NULL) {                                 \
        LOG_Log(LOG_ALERT, "error loading mq method %s: %s", #intfm, errmsg); \
        exit(EXIT_FAILURE);                                             \
    }
#include "methods.h"
#undef METHOD

    /* install signal handlers */
    dump_action.sa_handler = dump;
    AZ(sigemptyset(&dump_action.sa_mask));
    dump_action.sa_flags |= SA_RESTART;

#define CHILD(SIG,disp) SIGDISP(SIG,disp)
#define PARENT(SIG,disp) ((void) 0)
#include "signals.h"
#undef PARENT
#undef CHILD

    if (DATA_Init() != 0) {
        LOG_Log(LOG_ERR, "Cannot init data table: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (hash_init() != 0) {
        LOG_Log(LOG_ERR, "Cannot init hash table: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    VSM_Diag(vd, vsl_diag, NULL);
    if (VSL_Open(vd, 1))
        exit(EXIT_FAILURE);
    LOG_Log(LOG_INFO, "Reading varnish instance %s", vsm_name(vd));

    /* Only read the VSL tags relevant to tracking */
    assert(VSL_Arg(vd, 'i', TRACK_TAGS) > 0);

    /* Start the monitor thread */
    if (config.monitor_interval > 0.0) {
        if (pthread_create(&monitor, NULL, MON_StatusThread,
                (void *) &config.monitor_interval) != 0) {
            LOG_Log(LOG_ERR, "Cannot start monitoring thread: %s\n",
                strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    else
        LOG_Log0(LOG_INFO, "Monitoring thread not running");

    errmsg = mqf.global_init(config.nworkers, config.mq_config_file);
    if (errmsg != NULL) {
        LOG_Log(LOG_ERR, "Cannot initialize message broker access: %s", errmsg);
        exit(EXIT_FAILURE);
    }

    errmsg = mqf.init_connections();
    if (errmsg != NULL) {
        LOG_Log(LOG_ERR, "Cannot initialize message broker connections: %s",
            errmsg);
        exit(EXIT_FAILURE);
    }

    errnum = WRK_Init();
    if (errnum != 0) {
        LOG_Log(LOG_ERR, "Cannot prepare worker threads: %s",
            strerror(errnum));
        exit(EXIT_FAILURE);
    }
    if ((errnum = SPMCQ_Init()) != 0) {
        LOG_Log(LOG_ERR, "Cannot initialize internal worker queue: %s",
            strerror(errnum));
        exit(EXIT_FAILURE);
    }

    if (config.nworkers > 0) {
        WRK_Start();
        /* XXX: wrk_wait & sleep interval configurable */
        int wrk_wait = 0;
        while ((wrk_running = WRK_Running()) == 0) {
            if (wrk_wait++ > 10) {
                LOG_Log0(LOG_ALERT,
                    "Worker threads not starting, shutting down");
                exit(EXIT_FAILURE);
            }
            TIM_sleep(1);
        }
        LOG_Log(LOG_INFO, "%d worker threads running", wrk_running);
    }
    else
        LOG_Log0(LOG_INFO, "Worker threads not running");
        
    /* Main loop */
    term = 0;
    /* XXX: TERM not noticed until request received */
    while (VSL_Dispatch(vd, OSL_Track, NULL) > 0)
        if (term || !endless)
            break;
        else if (dtbl.w_stats.abandoned == config.nworkers) {
            LOG_Log0(LOG_ALERT, "All worker threads abandoned, giving up");
            giveup = 1;
            break;
        }
        else if (WRK_Exited() - dtbl.w_stats.abandoned > 0) {
            if ((errnum = WRK_Restart()) != 0) {
                LOG_Log(LOG_ALERT, "Cannot restart worker threads, giving up "
                    "(%s)", strerror(errnum));
                giveup = 1;
                break;
            }
        }
        else {
            LOG_Log0(LOG_WARNING, "Log read interrupted, continuing");
            continue;
        }

    if (term)
        LOG_Log0(LOG_INFO, "Termination signal received");
    else if (giveup)
        LOG_Log0(LOG_INFO, "Terminating due to worker thread failure");
    else if (endless)
        LOG_Log0(LOG_WARNING, "Varnish log closed");

    WRK_Halt();
    WRK_Shutdown();
    if ((errmsg = mqf.global_shutdown()) != NULL)
        LOG_Log(LOG_ALERT, "Message queue shutdown failed: %s", errmsg);
    if (dlclose(mqh) != 0)
        LOG_Log(LOG_ALERT, "Error closing mq module %s: %s", config.mq_module,
                dlerror());
    if (config.monitor_interval > 0.0)
        MON_StatusShutdown(monitor);
    LOG_Log0(LOG_INFO, "Worker process exiting");
    LOG_Close();
    exit(EXIT_SUCCESS);
}

#ifdef TEST_DRIVER

#include <float.h>

#include "minunit.h"

int tests_run = 0;
static char errmsg[BUFSIZ];

static char
*test_hash_init(void)
{
    int err;

    printf("... testing hash table initialization\n");
    
    config.maxopen_scale = 10;
    config.maxdone = 1024;
    /* Set max_probes to maximum for testing */
    config.hash_max_probes = 1024;
    /* Deactivates evacuation */
    config.hash_mlt = UINT_MAX;
    AZ(DATA_Init());
    err = hash_init();
    sprintf(errmsg, "hash init: %s", strerror(err));
    mu_assert(errmsg, err == 0);

    return NULL;
}

static const char
*test_hash_insert(void)
{
    dataentry entry;
    hashentry *he;

    printf("... testing hash insert\n");

    he = hash_insert(1234567890, &entry, FLT_MAX);
    mu_assert("hash_insert returned NULL", he != NULL);
    mu_assert("hash_insert: invalid magic number", he->magic == HASH_MAGIC);
    mu_assert("hash_insert: entry not open", he->state == HASH_OPEN);
    mu_assert("hash_insert: XID not set", he->xid == 1234567890);
    mu_assert("hash_insert: insert time not set", he->insert_time == FLT_MAX);
    mu_assert("hash_insert: data pointer not set", he->de == &entry);

    /* Cleanup */
    hash_free(he);
    he->xid = 0;
    mu_assert("hash_insert: open != 0 after freeing only insert",
        htbl.open == 0);
    
    unsigned xid = 1234567890;
    for (int i = 0; i < htbl.len; i++) {
        he = hash_insert(xid, &entry, 0);
        sprintf(errmsg, "hash_insert: failed at %u", xid);
        mu_assert(errmsg, he != NULL);
        xid++;
    }

    /* Cleanup */
    for (int i = 0; i < htbl.len; i++)
        hash_free(&htbl.entry[i]);
    sprintf(errmsg, "hash_insert: open = %u after freeing all elements",
        htbl.open);
    mu_assert(errmsg, htbl.open == 0);
    
    return NULL;
}

static const char
*test_hash_find(void)
{
    hashentry *entry1, *entry2;
    dataentry data;
    unsigned xid;

    printf("... testing hash find\n");

    data.magic = DATA_MAGIC;
    data.state = DATA_OPEN;
    
    entry1 = hash_insert(1234567890, &data, FLT_MAX);
    entry2 = hash_find(1234567890);
    mu_assert("hash_find: returned NULL", entry2 != NULL);
    mu_assert("hash_find: invalid magic number", entry2->magic == HASH_MAGIC);
    mu_assert("hash_find: invalid data pointer", entry2->de == &data);
    sprintf(errmsg, "hash_find: expected XID=1234567890, got %u", entry2->xid);
    mu_assert(errmsg, entry2->xid == 1234567890);
    sprintf(errmsg, "hash_find: expected insert time %f, got %f", FLT_MAX,
        entry2->insert_time);
    mu_assert(errmsg, entry2->insert_time == FLT_MAX);
    /* Cleanup */
    entry1->xid = 0;
    hash_free(entry1);
    mu_assert("hash_free: open != 0 after freeing only insert", htbl.open == 0);

    entry2 = hash_find(1234567890);
    mu_assert("hash_find: empty entry, expected NULL", entry2 == NULL);

    xid = 1234567890;
    for (int i = 0; i < htbl.len; i++) {
        entry1 = hash_insert(xid++, &data, 0);
        sprintf(errmsg, "hash_insert: failed at %u", xid - 1);
        if (entry1 == NULL)
            hash_dump();
        mu_assert(errmsg, entry1 != NULL);
    }
    entry2 = hash_find(xid);
    mu_assert("hash_find: non-existent XID, expected NULL", entry2 == NULL);

    for (int i = 0; i < htbl.len; i++) {
        entry2 = hash_find(1234567890 + i);
        sprintf(errmsg, "hash_find: expected %u, returned NULL",
            1234567890 + i);
        mu_assert(errmsg, entry2 != NULL);
    }

    entry2 = hash_find(1234567890 + htbl.len);
    mu_assert("hash_find: XID corner case, expected NULL", entry2 == NULL);
    for (int i = 0; i < htbl.len/2; i++) {
        entry1 = hash_find(1234567890 + i);
        entry1->xid = 0;
        hash_free(entry1);
    }

    for (int i = 0; i < htbl.len; i++) {
        entry2 = hash_find(1234567890 + i);
        if (i < htbl.len/2)
            mu_assert("hash_find: emptied entry, expected NULL",
                entry2 == NULL);
        else {
            sprintf(errmsg, "hash_find: expected %u, returned NULL",
                1234567890 + i);
            mu_assert(errmsg, entry2 != NULL);
            sprintf(errmsg, "hash_find: expected %u, found %u", 1234567890 + i,
                    entry2->xid);
            mu_assert(errmsg, entry2->xid == 1234567890 + i);
        }
    }
    
    return NULL;
}

static const char
*all_tests(void)
{
    mu_run_test(test_hash_init);
    mu_run_test(test_hash_insert);
    mu_run_test(test_hash_find);
    return NULL;
}

TEST_RUNNER

#endif
