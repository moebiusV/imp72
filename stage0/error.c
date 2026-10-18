/*
 * error.c -- the driver services the kernel calls.  Ports ERROR and MSG of
 * decus/impm.imc (and the stand-in ERROR at the end of decus/free.imc).
 *
 * ERROR(N,E): N<=1 sets the error flag, which is also the /L listing bit,
 * so the source is listed from the first error on; N=0 is fatal: the
 * compilation of this file is abandoned (`GO TO FIN` in the original).
 * The driver says where a fatal error unwinds to with imp_fatal_handler().
 */
#include <stdlib.h>
#include "imp.h"

word finam5;
FILE *listing;
word imp_err;
static void (*fatal)(void);

FILE *
imp_out(void)
{
    return listing != NULL ? listing : stdout;
}

void
imp_msg(const char *m)
{
    fprintf(stderr, "%s\n", m);
}

void
imp_fatal_handler(void (*fn)(void))
{
    fatal = fn;
}

void
imp_error(int n, const char *e)
{
    if (n <= 1)
        finam5 |= SW_ERRORS;
    if (e != NULL) {
        const char *pre = n < 2 ? "** ERROR - " : n == 2 ? "** ADVISORY - " : "";

        fputs(pre, stderr);
        fprintf(imp_out(), "%s%s\n", pre, e);
        imp_msg(e);
    }
    imp_err = -1;
    if (n == 0) {
        finam5 &= ~SW_CONTINUE;
        if (fatal != NULL)
            fatal();
        exit(1);
    }
}
