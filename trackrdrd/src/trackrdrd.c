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
#include <sys/fcntl.h>

#include "compat/daemon.h"

#include "vsb.h"
#include "vpf.h"

#include "libvarnish.h"
#include "vsl.h"
#include "varnishapi.h"
#include "miniobj.h"

#include "trackrdrd.h"
#include "revision.h"
#include "usage.h"

#define TRACK_TAGS "ReqStart,VCL_log,ReqEnd"
#define TRACKLOG_PREFIX "track "
#define TRACKLOG_PREFIX_LEN (sizeof(TRACKLOG_PREFIX)-1)

#define DEFAULT_CONFIG "/etc/trackrdrd.conf"

/*--------------------------------------------------------------------*/

/* XXX: Temporary, for testing */
static void
submit(unsigned xid)
{
    dataentry *entry;
    
    entry = DATA_Find(xid);
    CHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
    assert(entry->state == DATA_DONE);
    LOG_Log(LOG_DEBUG, "submit: data=[%.*s]", entry->end, entry->data);
    tbl.done--;
    tbl.submitted++;
    entry->state = DATA_EMPTY;
}

static void
sigusr1(int sig)
{
    DATA_Dump();
    signal(sig, sigusr1);
}

static int
OSL_Track(void *priv, enum VSL_tag_e tag, unsigned fd, unsigned len,
          unsigned spec, const char *ptr, uint64_t bitmap)
{
    unsigned xid;
    dataentry *entry;
    int err, datalen;
    char *data;

    (void) priv;
    (void) bitmap;

    /* spec != 'c' */
    if ((spec & VSL_S_CLIENT) == 0)
        LOG_Log(LOG_WARNING, "%s: Client bit ('c') not set", VSL_tags[tag]);
    
    switch (tag) {
    case SLT_ReqStart:

        err = Parse_ReqStart(ptr, len, &xid);
        AZ(err);
        LOG_Log(LOG_DEBUG, "%s: XID=%d", VSL_tags[tag], xid);

        tbl.seen++;
        entry = DATA_Insert(xid);
        if (entry == NULL) {
            LOG_Log(LOG_ALERT,
                "%s: Cannot insert data, XID=%d tid=%d DISCARDED",
                VSL_tags[tag], xid, fd);
            break;
        }
        CHECK_OBJ(entry, DATA_MAGIC);
        
        entry->state = DATA_OPEN;
        entry->xid = xid;
        entry->tid = fd;
        sprintf(entry->data, "XID=%d", xid);
        entry->end = strlen(entry->data);
        if (entry->end > tbl.data_hi)
            tbl.data_hi = entry->end;
        tbl.open++;
        if (tbl.open + tbl.done > tbl.occ_hi)
            tbl.occ_hi = tbl.open + tbl.done;
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
        
        /* assert((hash(XID) exists) && hash(XID).tid == fd
                  && !hash(XID).done); */
        entry = DATA_Find(xid);
        CHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
        assert(entry->xid == xid);
        assert(entry->tid == fd);
        assert(entry->state == DATA_OPEN);

        /* Data overflow */
        /* XXX: Encapsulate (1 << (config.maxdata_scale+10)) */
        if (entry->end + datalen + 1 > (1 << (config.maxdata_scale+10))) {
            LOG_Log(LOG_ALERT,
                "%s: Data too long, XID=%d, current length=%d, "
                "DISCARDING data=[%.*s]", VSL_tags[tag], xid, entry->end,
                datalen, data);
            break;
        }
        
        entry->data[entry->end] = '&';
        entry->end++;
        memcpy(&entry->data[entry->end], data, datalen);
        entry->end += datalen;
        if (entry->end > tbl.data_hi)
            tbl.data_hi = entry->end;
        break;

    case SLT_ReqEnd:
        /* assert(regex.match() && (hash(XID) exists) && hash(XID).tid == fd
                  && !hash(XID).done); */
        err = Parse_ReqEnd(ptr, len, &xid);
        AZ(err);
        LOG_Log(LOG_DEBUG, "%s: XID=%d", VSL_tags[tag], xid);
        
        entry = DATA_Find(xid);
        CHECK_OBJ_NOTNULL(entry, DATA_MAGIC);
        assert(entry->xid == xid);
        assert(entry->tid == fd);
        assert(entry->state == DATA_OPEN);

        /*hash(XID).done = TRUE;*/
        entry->state = DATA_DONE;
        tbl.done++;
        tbl.open--;
        submit(xid);
        break;

    default:
        /* Unreachable */
        AN(NULL);
        return(1);
    }
    return(0);
}

/*--------------------------------------------------------------------*/

static void
usage(int status)
{
    fprintf(stderr, "Usage:\n%s\n%s\n", synopsis, options);
    exit(status);
}

int
main(int argc, char * const *argv)
{
        int c, d_flag = 0;
	const char *P_arg = NULL, *l_arg = NULL, *n_arg = NULL, *f_arg = NULL,
            *y_arg = NULL, *c_arg = NULL;
	struct vpf_fh *pfh = NULL;
	struct VSM_data *vd;

	vd = VSM_New();
	VSL_Setup(vd);

        if (access(DEFAULT_CONFIG, F_OK) == 0) {
            if (access(DEFAULT_CONFIG, R_OK) != 0) {
                perror(DEFAULT_CONFIG);
                exit(EXIT_FAILURE);
            }
            printf("Reading config from %s\n", DEFAULT_CONFIG);
            if (CONF_ReadFile(DEFAULT_CONFIG) != 0)
                exit(EXIT_FAILURE);
        }

        /* XXX: When we can demonize, add an option to run as non-demon */
	while ((c = getopt(argc, argv, "P:Vn:hl:df:y:c:")) != -1) {
		switch (c) {
		case 'P':
                    P_arg = optarg;
                    break;
		case 'V':
                    printf(PACKAGE_STRING " revision " REVISION "\n");
                    exit(EXIT_SUCCESS);
                case 'n':
                    n_arg = optarg;
                    break;
                case 'l':
                    l_arg = optarg;
                    break;
                case 'd':
                    d_flag = 1;
                    break;
                case 'f':
                    f_arg = optarg;
                    break;
                case 'y':
                    y_arg = optarg;
                    break;
                case 'c':
                    c_arg = optarg;
                    break;
                case 'h':
                    usage(EXIT_SUCCESS);
		default:
                    usage(EXIT_FAILURE);
		}
	}

	if ((argc - optind) > 0)
            usage(EXIT_FAILURE);

        if (c_arg) {
            printf("Reading config from %s\n", c_arg);
            if (CONF_ReadFile(c_arg) != 0)
                exit(EXIT_FAILURE);
        }
        
        if (f_arg && n_arg)
            usage(EXIT_FAILURE);
        if (l_arg && y_arg)
            usage(EXIT_FAILURE);
        
        if (P_arg)
            strcpy(config.pid_file, P_arg);
        if (n_arg)
            strcpy(config.varnish_name, n_arg);
        if (l_arg)
            strcpy(config.log_file, l_arg);
        if (y_arg)
            CONF_Add("syslog.facility", y_arg);
        
        if (f_arg && VSL_Arg(vd, 'r', f_arg) <= 0)
            exit(EXIT_FAILURE);
        else if (!EMPTY(config.varnish_name)
                 && VSL_Arg(vd, 'n', config.varnish_name) <= 0)
            exit(EXIT_FAILURE);
        
        if (LOG_Open(PACKAGE_NAME) != 0) {
            exit(EXIT_FAILURE);
        }
        
        if (d_flag)
            LOG_SetLevel(LOG_DEBUG);
        LOG_Log0(LOG_INFO, "starting");

        CONF_Dump();
        
        /* XXX: Parent/child setup
           Write the pid in the parent, open VSL in the child
        */
	if (!EMPTY(config.pid_file)
            && (pfh = VPF_Open(config.pid_file, 0644, NULL)) == NULL) {
		perror(config.pid_file);
		exit(EXIT_FAILURE);
	}
	if (pfh != NULL)
		VPF_Write(pfh);

        /*
	if (!D_flag && varnish_daemon(0, 0) == -1) {
		perror("daemon()");
		if (pfh != NULL)
			VPF_Remove(pfh);
		exit(1);
	}
        */

        /* XXX: child inits data table, opens and reads VSL */
        if (DATA_Init() != 0) {
            LOG_Log(LOG_ALERT, "Cannot init data table: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        /* XXX: Install this signal handler in the child */
        if (signal(SIGUSR1, sigusr1) == SIG_ERR) {
            perror("Signal handler USR1:");
            exit(EXIT_FAILURE);
        }
        
	if (VSL_Open(vd, 1))
            exit(EXIT_FAILURE);

        /* Only read the VSL tags relevant to tracking */
        assert(VSL_Arg(vd, 'i', TRACK_TAGS) > 0);
        
	while (VSL_Dispatch(vd, OSL_Track, NULL) >= 0)
            ;
        
        /* XXX: Parent removes PID */
	if (pfh != NULL)
		VPF_Remove(pfh);

        LOG_Log0(LOG_INFO, "exiting");
	exit(EXIT_SUCCESS);
}
