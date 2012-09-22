/*-
 * Copyright (c) 2012 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012 Otto Gmbh & Co KG
 * All rights reserved
 * Use only with permission
 *
 * Author: Geoffrey Simmons <geoffrey.simmons@uplex.de>
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

#include "compat/daemon.h"

#include "vsb.h"
#include "vpf.h"

#include "libvarnish.h"
#include "vsl.h"
#include "varnishapi.h"

#include "trackrdrd.h"

#define TRACK_TAGS "ReqStart,VCL_log,ReqEnd"
#define TRACKLOG_PREFIX "track "
#define TRACKLOG_PREFIX_LEN (sizeof(TRACKLOG_PREFIX)-1)

/*--------------------------------------------------------------------*/

static int
OSL_Track(void *priv, enum VSL_tag_e tag, unsigned fd, unsigned len,
          unsigned spec, const char *ptr, uint64_t bitmap)
{
    unsigned xid;
#if 0    
    hashentry *entry;
#endif    
    int err;
    int datalen;
    char *data;

    (void) bitmap;
#if 1
    (void) spec;
    (void) priv;
    (void) fd;
#endif
#if 0
    /* if spec != 'c', we may have errors reading from SHM */
    if (spec & VSL_S_CLIENT == 0)
        WARN();
#endif
    
    switch (tag) {
    case SLT_ReqStart:

        /* May not be able to have the assert()s, if we can't guarantee
           that data arrive correctly and in the right order.
           In which case we'll have to have lots of error checking,
           and ERROR & WARN messages. */

        err = Parse_ReqStart(ptr, len, &xid);
        AZ(err);
        LOG_Log(LOG_DEBUG, "%s: XID=%d", VSL_tags[tag], xid);
#if 0
        /* assert(!(hash(XID) exists)); */
        /* init hash(XID); */
        /* HSH_Insert() should return err if hash(XID) exists */
        err = HSH_Insert(hashtable, xid, &entry);
        CHECK_OBJ_NOTNULL(entry, HASHENTRY_MAGIC);
        /* Rather than assert, arrange for an error message.
           We may have hashtable at MAX, etc. */
        AZ(err);

        /* hash(XID).tid = fd; */
        entry->tid = fd;
#endif        
        break;

    case SLT_VCL_Log:
        /* Skip VCL_Log entries without the "track " prefix. */
        if (strncmp(ptr, TRACKLOG_PREFIX, TRACKLOG_PREFIX_LEN) != 0)
            break;
        
        /* assert(regex captures XID and data); */
        err = Parse_VCL_Log(&ptr[TRACKLOG_PREFIX_LEN], len-TRACKLOG_PREFIX_LEN,
                            &xid, &data, &datalen);
        AZ(err);
        LOG_Log(LOG_DEBUG, "%s: XID=%d, data=[%.*s]", VSL_tags[tag],
            xid, datalen, data);
#if 0
        
        /* assert((hash(XID) exists) && hash(XID).tid == fd
                  && !hash(XID).done); */
        entry = HSH_Find(hashtable, xid);
        CHECK_OBJ_NOTNULL(entry, HASHENTRY_MAGIC);
        assert(entry->xid == xid);
        assert(entry->tid == fd);
        assert(!entry->done);
        /* Data overflow should be an error message, not assert. */
        assert(entry->s + datalen + 1 <= MAX_DATA);
        
        /* append data to hash(XID).data; */
        if (entry->s != 0) {
            entry->data[entry->s] = '&';
            entry->s++;
        }
        memcpy(&entry->data[entry->s], data, datalen);
        entry->s += datalen;
#endif        
        break;

    case SLT_ReqEnd:
        /* assert(regex.match() && (hash(XID) exists) && hash(XID).tid == fd
                  && !hash(XID).done); */
        err = Parse_ReqEnd(ptr, len, &xid);
        AZ(err);
        LOG_Log(LOG_DEBUG, "%s: XID=%d", VSL_tags[tag], xid);
        
#if 0        
        entry = HSH_Find(hashtable, xid);
        CHECK_OBJ_NOTNULL(entry, HASHENTRY_MAGIC);
        assert(entry->xid == xid);
        assert(entry->tid == fd);
        assert(!entry->done);

        /*hash(XID).done = TRUE;*/
        entry->done = TRUE;
        submit(XID);
#endif        
        break;

    default:
#if 0        
        /* Should never get here */
        ERROR();
#endif        
        return(1);
    }
    return(0);
}

/*--------------------------------------------------------------------*/

static void
usage(void)
{
	fprintf(stderr, "usage: varnishlog "
	    "%s [-aDV] [-o [tag regex]] [-n varnish_name] [-P file] [-w file]\n", VSL_USAGE);
	exit(1);
}

int
main(int argc, char * const *argv)
{
	int c;
	int D_flag = 0, d_flag = 0;
	const char *P_arg = NULL, *l_arg = NULL;
	struct vpf_fh *pfh = NULL;
	struct VSM_data *vd;

	vd = VSM_New();
	VSL_Setup(vd);

	while ((c = getopt(argc, argv, "DP:Vn:hl:df:")) != -1) {
		switch (c) {
		case 'D':
                    D_flag = 1;
                    break;
		case 'P':
                    P_arg = optarg;
                    break;
		case 'V':
                    printf(PACKAGE_STRING "\n");
                    exit(0);
                case 'n':
                    if (VSL_Arg(vd, c, optarg) <= 0)
                        exit(1);
                case 'l':
                    l_arg = optarg;
                    break;
                case 'd':
                    d_flag = 1;
                    break;
                case 'f':
                    if (VSL_Arg(vd, 'r', optarg) <= 0)
                        exit(1);
                    break;
                case 'h':
		default:
                    usage();
		}
	}

	if ((argc - optind) > 0)
		usage();

        if (LOG_Open(PACKAGE_NAME, l_arg) != 0) {
            perror(l_arg);
            exit(1);
        }
        if (d_flag)
            LOG_SetLevel(LOG_DEBUG);
        LOG_Log0(LOG_INFO, "starting");
        
        /*
	if (D_flag && varnish_daemon(0, 0) == -1) {
		perror("daemon()");
		if (pfh != NULL)
			VPF_Remove(pfh);
		exit(1);
	}
        */

        /* XXX: Parent/child setup
           Write the pid in the parent, open VSL in the child
        */
	if (P_arg && (pfh = VPF_Open(P_arg, 0644, NULL)) == NULL) {
		perror(P_arg);
		exit(1);
	}
	if (pfh != NULL)
		VPF_Write(pfh);

        /* XXX: child opens and reads VSL */
	if (VSL_Open(vd, 1))
		exit(1);

        /* Only read the VSL tags relevant to tracking */
        assert(VSL_Arg(vd, 'i', TRACK_TAGS) > 0);
        
	while (VSL_Dispatch(vd, OSL_Track, stdout) >= 0) {            
		if (fflush(stdout) != 0) {
			perror("stdout");
			break;
		}
	}

        /* XXX: Parent removes PID */
	if (pfh != NULL)
		VPF_Remove(pfh);

        LOG_Log0(LOG_INFO, "exiting");
	exit(0);
}
