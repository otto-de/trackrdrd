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
 * Module:       test_utils.h
 * Description:  Contains utilities for testing.
 */

#ifndef _TEST_UTILS_H
#define _TEST_UTILS_H


/***** includes ***************************************************************/

#include <stdio.h>


/***** defines ****************************************************************/

/** debugging output */
#define VERBOSE 1
#undef VERBOSE
#ifdef VERBOSE
#define verbose(fmt, ...)                                               \
    do {                                                                \
        printf("%8s:%4d:%20s:>>> "fmt"\n", __FILE__, __LINE__, __FUNCTION__, \
               ##__VA_ARGS__); }                                        \
    while (0)
#else
    #define verbose(fmt, ...) do{ } while ( 0 )
#endif

#define returnIfNotNull(test) \
    do { const char *msg = test; if (msg) return msg; } while (0)


/***** constants **************************************************************/

/** file name for saving stdout */
// #define FILE_NAME_STDOUT = "stdout.txt";
extern const char * FILE_NAME_STDOUT;

/** file name for saving stderr */
//#define FILE_NAME_STDERR = "stderr.txt";
extern const char * FILE_NAME_STDERR;


/***** variables **************************************************************/


/***** structures *************************************************************/


/***** functions **************************************************************/

/**
 * Redirect stdout into new file FILE_NAME_STDOUT
 */
int TEST_catchStdoutStart(void);

/**
 * Reset redirection of stdout and close resulting file FILE_NAME_STDOUT
 */
int TEST_catchStdoutEnd(void);

/**
 * Redirect stderr into new file FILE_NAME_STDERR
 */
int TEST_catchStderrStart(void);

/**
 * Reset redirection of stdout and close resulting file FILE_NAME_STDERR
 */
int TEST_catchStderrEnd(void);

/**
 * Test if files have same content. If the comparison is not successful
 * some info is written to stderr.
 *
 * @param fname1 first file.
 * @param fname2 second file.
 * @return 0 on success, a value < 0 if we had problems reading the files and a
 *     line number (starting with 1) if there was a difference in that line.
 */
int TEST_compareFiles(const char * fname1, const char * fname2);

/**
 * Test if file contents equals given text.
 *
 * @param fname file name.
 * @param text compare file contents with this text.
 * @return 0 on success, a value < 0 if we had problems reading the file and a
 *     line number (starting with 1) if there was a difference in that line.
 */
int TEST_compareFileWithString(const char * fname, const char * text);

/**
 * Test if previously saved stdout equals given text. See
 * TEST_catchStdoutStart() and TEST_catchStdoutEnd().
 *
 * @param text compare stdout with this text.
 * @return 0 on success, a value < 0 if we had problems reading the stdout
 *     file and a line number (starting with 1) if there was a difference
 *     in that line.
 */
int TEST_stdoutEquals(const char * text);

/**
 * Test if previously saved stderr equals given text. See
 * TEST_catchStderrStart() and TEST_catchStderrEnd().
 *
 * @param text compare stderr with this text.
 * @return 0 on success, a value < 0 if we had problems reading the stderr
 *     file and a line number (starting with 1) if there was a difference
 *     in that line.
 */
int TEST_stderrEquals(const char * text);


#endif /* _TEST_UTILS_H */
