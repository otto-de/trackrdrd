/*-
 * Copyright (c) 2014 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2014 Otto Gmbh & Co KG
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

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <assert.h>

#include "mq.h"
#include "config_common.h"
#include "miniobj.h"

#define ERRMSG_MAX (LINE_MAX + PATH_MAX + 1)

#define xstr(X) #X
#define str(X) xstr(X)

#if defined(CURRENT) && defined(REVISION) && defined(AGE)
#define SO_VERSION (str(CURRENT) "." str(REVISION) "." str(AGE))
#elif defined(VERSION)
#define SO_VERSION VERSION
#else
#define SO_VERSION "unknown version"
#endif

typedef struct wrk_s {
    unsigned magic;
#define FILE_WRK_MAGIC 0x50bff5f0
    int n;
    char errmsg[ERRMSG_MAX];
} wrk_t;

static FILE *out;

static int append = 1;
static unsigned nwrk;
static wrk_t *workers;
static char fname[PATH_MAX + 1] = "";
static char errmsg[ERRMSG_MAX];
static char _version[LINE_MAX];

static int
conf_add(const char *lval, const char *rval)
{
    if (strcmp(lval, "output.file") == 0) {
        strncpy(fname, rval, PATH_MAX);
        return 0;
    }
    if (strcmp(lval, "append") == 0) {
        if (strcasecmp(rval, "yes") == 0
            || strcasecmp(rval, "true") == 0
            || strcmp(rval, "1") == 0) {
            append = 1;
            return 0;
        }
        else if (strcasecmp(rval, "no") == 0
                 || strcasecmp(rval, "false") == 0
                 || strcmp(rval, "0") == 0) {
            append = 0;
            return 0;
        }
        return EINVAL;
    }
    return EINVAL;
}

const char *
MQ_GlobalInit(unsigned nworkers, const char *config_fname)
{
    int errnum;

    nwrk = nworkers;

    if ((errnum = CONF_ReadFile(config_fname, conf_add)) != 0) {
        snprintf(errmsg, ERRMSG_MAX,
                 "Error reading config file %s for the file plugin: %s",
                 config_fname, strerror(errnum));
        return errmsg;
    }

    if (strcmp(fname, "-") == 0)
        out = stdout;
    else {
        errno = 0;
        out = fopen(fname, append ? "a" : "w");
        if (out == NULL) {
            snprintf(errmsg, ERRMSG_MAX, "Cannot open output file %s: %s",
                     fname, strerror(errno));
            return errmsg;
        }
    }
    snprintf(_version, LINE_MAX, "libtrackrdr-file %s", SO_VERSION);
    return NULL;
}

const char *
MQ_InitConnections(void)
{
    workers = (wrk_t *) calloc(nwrk, sizeof(wrk_t));
    for (int i = 0; i < nwrk; i++)
        workers[i].magic = FILE_WRK_MAGIC;
    return NULL;
}

const char *
MQ_WorkerInit(void **priv, int wrk_num)
{
    wrk_t *wrk;

    assert(wrk_num >= 1 && wrk_num <= nwrk);
    wrk = &workers[wrk_num - 1];
    wrk->n = wrk_num;
    *priv = (void *) wrk;
    return NULL;
}

int
MQ_Send(void *priv, const char *data, unsigned len, const char *key,
        unsigned keylen, const char **error)
{
    wrk_t *wrk;

    if (priv == NULL) {
        *error = "MQ_Send() called with NULL worker object";
        return -1;
    }

    CAST_OBJ(wrk, priv, FILE_WRK_MAGIC);
    if (fprintf(out, "key=%.*s: %.*s\n", keylen, key, len, data) < 0) {
        snprintf(wrk->errmsg, ERRMSG_MAX, "worker %d: error writing output",
                 wrk->n);
        *error = wrk->errmsg;
        return 1;
    }
    return 0;
}

const char *
MQ_Reconnect(void **priv)
{
    wrk_t *wrk;

    CAST_OBJ_NOTNULL(wrk, *priv, FILE_WRK_MAGIC);
    assert(wrk->n > 0 && wrk->n <= nwrk);
    return NULL;
}

const char *
MQ_Version(void *priv, char *version, size_t len)
{
    (void) priv;
    strncpy(version, _version, len);
    return NULL;
}

const char *
MQ_ClientID(void *priv, char *clientID, size_t len)
{
    wrk_t *wrk;
    CAST_OBJ_NOTNULL(wrk, priv, FILE_WRK_MAGIC);
    snprintf(clientID, len, "worker %d", wrk->n);
    return NULL;
}

const char *
MQ_WorkerShutdown(void **priv, int wrk_num)
{
    wrk_t *wrk;

    (void) wrk_num;
    CAST_OBJ_NOTNULL(wrk, *priv, FILE_WRK_MAGIC);
    assert(wrk->n > 0 && wrk->n <= nwrk);
    *priv = NULL;

    return NULL;
}

const char *
MQ_GlobalShutdown(void)
{
    free(workers);

    if (out != stdout) {
        errno = 0;
        if (fclose(out) != 0) {
            snprintf(errmsg, ERRMSG_MAX, "Error closing output file %s: %s",
                     fname, strerror(errno));
            return errmsg;
        }
    }
    return NULL;
}
