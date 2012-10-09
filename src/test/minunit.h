/*-
 * Copyright (c) 2012 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012 Otto Gmbh & Co KG
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

#include <stdlib.h>

/*-
 * Adapted from http://www.jera.com/techinfo/jtns/jtn002.html
 * "MinUnit" - a minimal unit testing framework for C
 *
 * "You may use the code in this tech note for any purpose, with the
 *  understanding that it comes with NO WARRANTY."
 */

/* XXX: Format error message in mu_assert() */

#define mu_assert(msg, test) do { if (!(test)) return msg; } while (0)
#define mu_run_test(test) do { const char *msg = test(); tests_run++; \
                               if (msg) return msg; } while (0)
extern int tests_run;

#define TEST_RUNNER                                    \
int                                                    \
main(int argc, char **argv)                            \
{                                                      \
    (void) argc;                                       \
                                                       \
    printf("\nTEST: %s\n", argv[0]);                   \
    const char *result = all_tests();                  \
    printf("%s: %d tests run\n", argv[0], tests_run);  \
    if (result != NULL) {                              \
        printf("%s\n", result);                        \
        exit(EXIT_FAILURE);                            \
    }                                                  \
    exit(EXIT_SUCCESS);                                \
}