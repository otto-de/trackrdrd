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
 * Module:       testing.c
 * Description:  Contains utilities for testing.
 */

/***** includes ***************************************************************/

#include "testing.h"


/***** functions **************************************************************/

int TEST_compareFiles(const char * fname1, const char * fname2)
{
    FILE *fp1, *fp2;
    int ch1, ch2;
    int line = 1;
    int col = 1;

    fp1 = fopen( fname1,  "r" );
    if ( fp1 == NULL )
    {
       printf("Cannot open %s for reading ", fname1 );
       return -2;
    }

    fp2 = fopen( fname2,  "r" ) ;
    if (fp2 == NULL)
    {
       printf("Cannot open %s for reading ", fname2 );
       fclose ( fp1 );
       return -3;
    }
    do
    {
        col++;
        ch1 = getc( fp1 );
        ch2 = getc( fp2 );
        if ( ch1 == '\n' )
        {
            line++;
            col = 0;
        }
    } while ((ch1 != EOF) && (ch2 != EOF) && (ch1 == ch2));
    fclose ( fp1 );
    fclose ( fp2 );
    if ( ch1 != ch2 ) {
        printf("  files differ at line: %i col: %i \n", line, col);
    }
    return ch1 == ch2;
}

