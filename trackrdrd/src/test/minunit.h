/*-
 * Copied from http://www.jera.com/techinfo/jtns/jtn002.html
 * "MinUnit" - a minimal unit testing framework for C
 *
 * "You may use the code in this tech note for any purpose, with the
 *  understanding that it comes with NO WARRANTY."
 */

#define mu_assert(msg, test) do { if (!(test)) return msg; } while (0)
#define mu_run_test(test) do { char *msg = test(); tests_run++; \
                               if (msg) return msg; } while (0)
extern int tests_run;
