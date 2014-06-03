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
#ifdef VERBOSE
#define verbose(fmt, ...) \
    do { printf("%8s:%4d:%20s:>>> "fmt"\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); } while (0)
#else
    #define verbose(fmt, ...) do{ } while ( 0 )
#endif

#define returnIfNotNull(test) do { const char *msg = test; if (msg) return msg; } while (0)

/***** variables **************************************************************/

extern long global_line_pos;               /* actual position in input line */


/***** structures *************************************************************/


/***** functions **************************************************************/

extern int TEST_compareFiles(const char * fname1, const char * fname2);


#endif /* _TEST_UTILS_H */
