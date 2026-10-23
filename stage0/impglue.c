/*
 * impglue.c -- the free storage routines as IMP calls them: FALLOT, FRELOT, FADDEX and FINCSET of src/impsyn64.imc are calls of these, which
 * take the arguments by reference and put the arena in FB (the C side's fmem) after the routine, which may have moved it.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "imp.h"

static void
sync(void)
{
    static int check = -1;

    if (check < 0)
        check = getenv("IMP_FCHECK") != NULL;
    if (check)
        fcheck(__func__);
}

word
impfallot(word *a, word *n)
{
    word r;

    if (*a >= (1L << 18))                       /* a name: a new array */
        r = fallot_new((const char *)a, *n);
    else
        r = fallot(*a, *n);
    sync();
    return r;
}

word
impfrelot(word *a, word *aa, word *n, word *s)
{
    word r = frelot(a, (const char *)aa, *n, *s ? (void (*)(void))*s : NULL);

    sync();
    return r;
}

word
impfaddex(word *a, word *n)
{
    faddex(a, *n);
    sync();
    return *a;
}

word
impfincset(word *a, word *n)
{
    fincset(*a, *n);
    return 0;
}

/* the table of semantic routines (semtab.c) is a table of addresses; SEMCALL of the tape is this */
word
impsemcall(word *id, word *pars)
{
    word r = semcall(*id, pars);

    sync();
    return r;
}
