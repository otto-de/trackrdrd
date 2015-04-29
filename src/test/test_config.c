/*-
 * Copyright (c) 2012-2014 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012-2014 Otto Gmbh & Co KG
 * All rights reserved
 * Use only with permission
 *
 * Author: Michael Meyling <michael@meyling.com>
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

/***** includes ***************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "minunit.h"
#include "test_utils.h"

#include "../trackrdrd.h"
#include "config_common.h"


/***** defines ****************************************************************/

#define DEFAULT_USER "nobody"
#define DEFAULT_PID_FILE "/var/run/trackrdrd.pid"
#define DEFAULT_RESTART_PAUSE 1

#define confdump(str,val) \
    i += sprintf(verbose_buffer + i, str"\n", (val))


/***** variables **************************************************************/

int tests_run = 0;
char verbose_buffer[9000];


/***** functions **************************************************************/

static const char *
getConfigContent(void)
{
    int i = 0;
    confdump("pid.file = %s", config.pid_file);
    confdump("varnish.name = %s", config.varnish_name);
    confdump("log.file = %s",
        strcmp(config.log_file,"-") == 0 ? "stdout" : config.log_file);
    confdump("varnish.bindump = %s", config.varnish_bindump);
    confdump("syslog.facility = %s", config.syslog_facility_name);
    confdump("monitor.interval = %u", config.monitor_interval);
    confdump("monitor.workers = %s", config.monitor_workers ? "true" : "false");
    confdump("maxdone = %u", config.maxdone);
    confdump("maxdata = %u", config.maxdata);
    confdump("maxkeylen = %u", config.maxkeylen);
    confdump("qlen.goal = %u", config.qlen_goal);

    confdump("mq.module = %s", config.mq_module);
    confdump("mq.config_file = %s", config.mq_config_file);
    confdump("nworkers = %u", config.nworkers);
    confdump("restarts = %u", config.restarts);
    confdump("restart.pause = %u", config.restart_pause);
    confdump("thread.restarts = %u", config.thread_restarts);
    confdump("user = %s", config.user_name);
    return verbose_buffer;
}

static int
saveConfig(const char * fname)
{
    FILE *fp;

    fp = fopen( fname,  "w" );
    if ( fp == NULL ) {
       perror(fname);
       return 4;
    }
    fprintf(fp, getConfigContent());
    fclose ( fp );
    return 0;
}

static char
*test_CONF_Init(void)
{
    printf("... testing CONF_Init\n");

    CONF_Init();

    VMASSERT(!strcmp(DEFAULT_USER, config.user_name),
        "Default user name expected: \"%s\", but found: \"%s\"",
         DEFAULT_USER, config.user_name);
    VMASSERT(!strcmp(DEFAULT_PID_FILE, config.pid_file),
        "Default pid file name expected: \"%s\", but found: \"%s\"",
        DEFAULT_PID_FILE, config.user_name);
    MASSERT(DEFAULT_RESTART_PAUSE == config.restart_pause);
    return NULL;
}


static char
*readAndCompare(const char * confName)
{
    int err;
    char confNameNew[512];

    err = CONF_ReadDefault();
    VMASSERT(err == 0, "Error code during reading default config: %i", err);
    err = CONF_ReadFile(confName, CONF_Add);
    VMASSERT(err == 0,
             "Error code during reading config \"%s\": %i", confName, err);
//    verbose("Config is:\n%s", getConfigContent());

    strcpy(confNameNew, confName);
    strcat(confNameNew, ".new");
    saveConfig(confNameNew);
    VMASSERT(!TEST_compareFiles(confName, confNameNew),
        "Files are not equal: \"%s\" and \"%s\"", confName, confNameNew);
//  CONF_Dump();

    return NULL;
}

static const char
*test_CONF_ReadDefault(void)
{
    printf("... testing CONF_ReadDefault\n");

    strcpy(config.log_file, "testing.log");
    LOG_Open("trackrdrd");
    LOG_SetLevel(7);

    returnIfNotNull(readAndCompare("trackrdrd_001.conf"));
    returnIfNotNull(readAndCompare("trackrdrd_002.conf"));
    returnIfNotNull(readAndCompare("trackrdrd_003.conf"));

    return NULL;
}

static char
*readAndFindError(const char * confName, const char * errmsg)
{
    int err;

    CONF_Init();
    TEST_catchStderrStart();
    err = CONF_ReadFile(confName, CONF_Add);
    TEST_catchStderrEnd();
    VMASSERT(err < 0, "Wrong error code during reading config \"%s\": %i",
             confName, err);
    err = TEST_stderrEquals(errmsg);
    VMASSERT(err == 0,
             "stderr output during parsing config \"%s\" other than expected. "
             "Expected:\n%s",
             confName, errmsg);

    return NULL;
}

static const char
*test_CONF_ReadFile(void)
{
    printf("... testing CONF_ReadFile\n");

    returnIfNotNull(
        readAndFindError("trackrdrd_010.conf",
                         "Error in trackrdrd_010.conf line 12 "
                         "(Invalid argument): "
                         "'unknown.module = /my/path/module.so'\n"));
    return NULL;
}

static const char *
all_tests(void)
{
    mu_run_test(test_CONF_Init);
    mu_run_test(test_CONF_ReadDefault);
    mu_run_test(test_CONF_ReadFile);
    return NULL;
}

TEST_RUNNER
