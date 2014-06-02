/*-
 * Copyright (c) 2012-2014 UPLEX Nils Goroll Systemoptimierung
 * Copyright (c) 2012-2014 Otto Gmbh & Co KG
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

#ifndef _MINUNIT_INCLUDED
#define _MINUNIT_INCLUDED

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

/*-
 * Adapted from http://www.jera.com/techinfo/jtns/jtn002.html
 * "MinUnit" - a minimal unit testing framework for C
 *
 * "You may use the code in this tech note for any purpose, with the
 *  understanding that it comes with NO WARRANTY."
 */

#define mu_assert(msg, test) do { if (!(test)) return msg; } while (0)
#define mu_run_test(test) do { const char *msg = test(); tests_run++; \
                               if (msg) return msg; } while (0)

char _mu_errmsg[BUFSIZ];

#define _massert(test, msg, ...)                        \
    do {                                                \
        if (!(test)) {                                  \
            sprintf((_mu_errmsg), (msg), __VA_ARGS__);  \
            return (_mu_errmsg);                        \
        }                                               \
    } while(0)

#define _massert0(test, msg)                    \
    do {                                        \
        if (!(test)) {                          \
            sprintf((_mu_errmsg), (msg));       \
            return (_mu_errmsg);                \
        }                                       \
    } while(0)

#define VMASSERT(test, msg, ...) _massert((test),(msg),__VA_ARGS__)
#define MASSERT0(test, msg)      _massert0((test),(msg))

#define MASSERT(c)                                              \
    VMASSERT((c), "%s failed in %s at %s:%d (errno %d: %s)",    \
             #c, __func__, __FILE__, __LINE__, errno, strerror(errno))

/* short for MU Assert Zero / Non-Zero */
#define MAZ(c) MASSERT((c) == 0)
#define MAN(c) MASSERT((c) != 0)

#define MCHECK_OBJ(ptr, type_magic) MASSERT((ptr)->magic == type_magic)
#define MCHECK_OBJ_NOTNULL(ptr, type_magic)     \
    do {                                        \
        MAN(ptr);                               \
        MCHECK_OBJ(ptr, type_magic);            \
    } while(0)

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

#endif

