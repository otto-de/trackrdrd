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

/**
 * Module:       test_utils.c
 * Description:  Contains utilities for testing.
 */

/***** includes ***************************************************************/

#include "test_utils.h"
#include <unistd.h>
#include <fcntl.h>

/***** constants **************************************************************/

const char * FILE_NAME_STDOUT = "stdout.txt";

const char * FILE_NAME_STDERR = "stderr.txt";


/***** functions **************************************************************/

int
TEST_compareFiles(const char * fname1, const char * fname2)
{
    FILE *fp1, *fp2;
    int ch1, ch2;
    int line = 1;
    int col = 1;

    verbose("comparing files %s : %s", fname1, fname2);
    fp1 = fopen( fname1,  "r" );
    if ( fp1 == NULL )
    {
       perror(fname1);
       return -2;
    }

    fp2 = fopen( fname2,  "r" ) ;
    if (fp2 == NULL)
    {
       perror(fname2);
       return -3;
    }
    do
    {
        col++;
        ch1 = getc( fp1 );
        ch2 = getc( fp2 );
        if ( ch1 == '\n' && ch2 == '\n')
        {
            line++;
            col = 0;
        }
    } while ((ch1 != EOF) && (ch2 != EOF) && (ch1 == ch2));
    fclose ( fp1 );
    fclose ( fp2 );
    if ( ch1 != ch2 ) {
        printf("  files differ at line: %i col: %i \n", line, col);
        return line;
    } else {
        return 0;
    }
}

int
TEST_compareFileWithString(const char * fname, const char * text)
{
    FILE *fp1;
    int ch1, ch2;
    int line = 1;
    int col = 1;
    int i = 0;

    verbose("comparing file %s with text", fname);
    fp1 = fopen( fname,  "r" );
    if ( fp1 == NULL )
    {
       perror(fname);
       return -2;
    }

    do
    {
        col++;
        ch1 = getc( fp1 );
        ch2 = *(text + i++);
        if ( ch1 == '\n' && ch2 == '\n')
        {
            line++;
            col = 0;
        }
    } while ((ch1 != EOF) && (ch2 != '\0') && (ch1 == ch2));
    fclose ( fp1 );
    if ( ch1 != EOF || ch2 != '\0' ) {
        printf("  file differs at line: %i col: %i \n", line, col);
        return line;
    } else {
        return 0;
    }
}

int stdoutBak;
int stdoutNew;
fpos_t stdoutPos;

int
TEST_catchStdoutStart()
{
    fflush(stdout);
    fgetpos(stdout, &stdoutPos);
    stdoutBak = dup(fileno(stdout));
    stdoutNew = open(FILE_NAME_STDOUT, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | \
                                       S_IROTH | S_IWOTH);
    dup2(stdoutNew, fileno(stdout));
    close(stdoutNew);

    return(0);
}

int
TEST_catchStdoutEnd()
{
    fflush(stdout);
    dup2(stdoutBak, fileno(stdout));
    close(stdoutBak);
    clearerr(stdout);
    fsetpos(stdout, &stdoutPos);
    return(0);
}

int stderrBak, stderrNew;
fpos_t stderrPos;

int
TEST_catchStderrStart()
{
    fflush(stderr);
    fgetpos(stdout, &stderrPos);
    stderrBak = dup(fileno(stderr));

    stderrNew = open(FILE_NAME_STDERR, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | \
                                       S_IROTH | S_IWOTH);
    if (stderrNew < 0) {
        perror(NULL);
        return(-1);
    }
    dup2(stderrNew, fileno(stderr));
    close(stderrNew);
    return(0);
}

int
TEST_catchStderrEnd()
{
    fflush(stderr);
    dup2(stderrBak, fileno(stderr));
    close(stderrBak);
    clearerr(stderr);
    fsetpos(stderr, &stderrPos);
    return(0);
}

int
TEST_stdoutEquals(const char * text)
{
    return TEST_compareFileWithString(FILE_NAME_STDOUT, text);
}

int
TEST_stderrEquals(const char * text)
{
    return TEST_compareFileWithString(FILE_NAME_STDERR, text);
}
