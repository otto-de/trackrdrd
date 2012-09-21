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
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>

#include "trackrdrd.h"

#include "compat/daemon.h"

#include "vsb.h"
#include "vpf.h"

#include "libvarnish.h"
#include "vsl.h"
#include "varnishapi.h"


static int	b_flag, c_flag;

/* Ordering-----------------------------------------------------------*/

static struct vsb	*ob[65536];
static unsigned char	flg[65536];
static enum VSL_tag_e   last[65536];
static uint64_t       bitmap[65536];
#define F_INVCL		(1 << 0)

static void
h_order_finish(int fd, const struct VSM_data *vd)
{

	AZ(VSB_finish(ob[fd]));
	if (VSB_len(ob[fd]) > 1 && VSL_Matched(vd, bitmap[fd])) {
		printf("%s", VSB_data(ob[fd]));
	}
	bitmap[fd] = 0;
	VSB_clear(ob[fd]);
}

static void
clean_order(const struct VSM_data *vd)
{
	unsigned u;

	for (u = 0; u < 65536; u++) {
		if (ob[u] == NULL)
			continue;
		AZ(VSB_finish(ob[u]));
		if (VSB_len(ob[u]) > 1 && VSL_Matched(vd, bitmap[u])) {
			printf("%s\n", VSB_data(ob[u]));
		}
		flg[u] = 0;
		bitmap[u] = 0;
		VSB_clear(ob[u]);
	}
}

static int
h_order(void *priv, enum VSL_tag_e tag, unsigned fd, unsigned len,
    unsigned spec, const char *ptr, uint64_t bm)
{
	char type;

	struct VSM_data *vd = priv;

	/* XXX: Just ignore any fd not inside the bitmap */
	if (fd >= sizeof bitmap / sizeof bitmap[0])
		return (0);

	bitmap[fd] |= bm;

	type = (spec & VSL_S_CLIENT) ? 'c' :
	    (spec & VSL_S_BACKEND) ? 'b' : '-';

	if (!(spec & (VSL_S_CLIENT|VSL_S_BACKEND))) {
		if (!b_flag && !c_flag)
			(void)VSL_H_Print(stdout, tag, fd, len, spec, ptr, bm);
		return (0);
	}
	if (ob[fd] == NULL) {
		ob[fd] = VSB_new_auto();
		assert(ob[fd] != NULL);
	}
	if ((tag == SLT_BackendOpen || tag == SLT_SessionOpen ||
		(tag == SLT_ReqStart &&
		    last[fd] != SLT_SessionOpen &&
		    last[fd] != SLT_VCL_acl) ||
		(tag == SLT_BackendXID &&
		    last[fd] != SLT_BackendOpen)) &&
	    VSB_len(ob[fd]) != 0) {
		/*
		 * This is the start of a new request, yet we haven't seen
		 * the end of the previous one.  Spit it out anyway before
		 * starting on the new one.
		 */
		if (last[fd] != SLT_SessionClose)
			VSB_printf(ob[fd], "%5d %-12s %c %s\n",
			    fd, "Interrupted", type, VSL_tags[tag]);
		h_order_finish(fd, vd);
	}

	last[fd] = tag;

	switch (tag) {
	case SLT_VCL_call:
		if (flg[fd] & F_INVCL)
			VSB_cat(ob[fd], "\n");
		else
			flg[fd] |= F_INVCL;
		VSB_printf(ob[fd], "%5d %-12s %c %.*s",
		    fd, VSL_tags[tag], type, len, ptr);
		return (0);
	case SLT_VCL_trace:
	case SLT_VCL_return:
		if (flg[fd] & F_INVCL) {
			VSB_cat(ob[fd], " ");
			VSB_bcat(ob[fd], ptr, len);
			return (0);
		}
		break;
	default:
		break;
	}
	if (flg[fd] & F_INVCL) {
		VSB_cat(ob[fd], "\n");
		flg[fd] &= ~F_INVCL;
	}
	VSB_printf(ob[fd], "%5d %-12s %c %.*s\n",
	    fd, VSL_tags[tag], type, len, ptr);
	switch (tag) {
	case SLT_ReqEnd:
	case SLT_BackendClose:
	case SLT_BackendReuse:
	case SLT_StatSess:
		h_order_finish(fd, vd);
		break;
	default:
		break;
	}
	return (0);
}

/*--------------------------------------------------------------------*/

static volatile sig_atomic_t reopen;

static void
sighup(int sig)
{

	(void)sig;
	reopen = 1;
}

static int
open_log(const char *w_arg, int a_flag)
{
	int fd, flags;

	flags = (a_flag ? O_APPEND : O_TRUNC) | O_WRONLY | O_CREAT;
#ifdef O_LARGEFILE
	flags |= O_LARGEFILE;
#endif
	if (!strcmp(w_arg, "-"))
		fd = STDOUT_FILENO;
	else
		fd = open(w_arg, flags, 0644);
	if (fd < 0) {
		perror(w_arg);
		exit(1);
	}
	return (fd);
}

static void
do_write(const struct VSM_data *vd, const char *w_arg, int a_flag)
{
	int fd, i, l;
	uint32_t *p;

	fd = open_log(w_arg, a_flag);
	XXXAN(fd >= 0);
	(void)signal(SIGHUP, sighup);
	while (1) {
		i = VSL_NextLog(vd, &p, NULL);
		if (i < 0)
			break;
		if (i > 0) {
			l = VSL_LEN(p);
			i = write(fd, p, 8L + VSL_WORDS(l) * 4L);
			if (i < 0) {
				perror(w_arg);
				exit(1);
			}
		}
		if (reopen) {
			AZ(close(fd));
			fd = open_log(w_arg, a_flag);
			XXXAN(fd >= 0);
			reopen = 0;
		}
	}
	exit(0);
}

/*--------------------------------------------------------------------*/

int
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

    syslog(LOG_DEBUG, "%s: %.*s", VSL_tags[tag], len, ptr);

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

        syslog(LOG_DEBUG, "%s: XID=%d", VSL_tags[tag], xid);
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
        syslog(LOG_DEBUG, "%s: XID=%d, datalen=%d, data=%.*s", VSL_tags[tag],
               xid, datalen, datalen, data);
        
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
        syslog(LOG_DEBUG, "%s: XID=%d", VSL_tags[tag], xid);
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
	int a_flag = 0, D_flag = 0, O_flag = 0, u_flag = 0, m_flag = 0;
	const char *P_arg = NULL;
	const char *w_arg = NULL;
	struct vpf_fh *pfh = NULL;
	struct VSM_data *vd;

	vd = VSM_New();
	VSL_Setup(vd);

        openlog(PACKAGE_NAME, LOG_PID | LOG_CONS | LOG_NDELAY | LOG_NOWAIT,
            LOG_USER);
        atexit(closelog);

	while ((c = getopt(argc, argv, VSL_ARGS "aDP:uVw:oO")) != -1) {
		switch (c) {
		case 'a':
			a_flag = 1;
			break;
		case 'b':
			b_flag = 1;
			AN(VSL_Arg(vd, c, optarg));
			break;
		case 'c':
			c_flag = 1;
			AN(VSL_Arg(vd, c, optarg));
			break;
		case 'D':
			D_flag = 1;
			break;
		case 'o': /* ignored for compatibility with older versions */
			break;
		case 'O':
			O_flag = 1;
			break;
		case 'P':
			P_arg = optarg;
			break;
		case 'u':
			u_flag = 1;
			break;
		case 'V':
			VCS_Message("varnishlog");
			exit(0);
		case 'w':
			w_arg = optarg;
			break;
		case 'm':
			m_flag = 1;
			/* FALLTHROUGH */
		default:
			if (VSL_Arg(vd, c, optarg) > 0)
				break;
			usage();
		}
	}

	if (O_flag && m_flag)
		usage();

	if ((argc - optind) > 0)
		usage();

	if (VSL_Open(vd, 1))
		exit(1);

	if (P_arg && (pfh = VPF_Open(P_arg, 0644, NULL)) == NULL) {
		perror(P_arg);
		exit(1);
	}
        /*
	if (D_flag && varnish_daemon(0, 0) == -1) {
		perror("daemon()");
		if (pfh != NULL)
			VPF_Remove(pfh);
		exit(1);
	}
        */
	if (pfh != NULL)
		VPF_Write(pfh);

	if (w_arg != NULL)
		do_write(vd, w_arg, a_flag);

	if (u_flag)
		setbuf(stdout, NULL);

        if (VSL_Arg(vd, 'i', TRACK_TAGS) <= 0) {
            fprintf(stderr, "VSL_Arg(i)\n");
            exit(1);
        }
        
	while (VSL_Dispatch(vd, OSL_Track, stdout) >= 0) {            
		if (fflush(stdout) != 0) {
			perror("stdout");
			break;
		}
	}

	if (pfh != NULL)
		VPF_Remove(pfh);
	exit(0);
}
