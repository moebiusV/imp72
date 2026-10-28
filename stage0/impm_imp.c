/*
 * impm_imp.c -- the C that calls the driver program (src/impm.imp): the entry point of the compiler, and ERROR and MSG for the C that is left
 * (semtab.c, ir.c, pccirsem.c, dir_imp.c).  They go with that C.
 */
#include "imp.h"

extern word impl_impk(word *, word *), impl_error(word *, word *), impl_msg(word *);

/* the driver program of the tape: 0 if all went well */
int
impk_main(int argc, char **argv)
{
    word n = argc, v = (word)(intptr_t)argv;

    return (int)impl_impk(&n, &v);
}

void
imp_error(int n, const char *e)
{
    word nn = n, zero = 0, *ep = e != NULL && *e != '\0' ? (word *)(void *)e : &zero;

    impl_error(&nn, ep);
}

void
imp_msg(const char *m)
{
    impl_msg((word *)(void *)m);
}
