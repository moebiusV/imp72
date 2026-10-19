/*
 * stack_imp.c -- stack.c's interface, over src/stack.imp compiled by imp72stage0.
 *
 * The IMP module takes every argument by reference and finds free storage at
 * the address FB, so each entry point here puts the current fmem in FB (the
 * block moves when it grows) and passes the addresses of its arguments.  The
 * module is built with its functions renamed impl_NAME (see the Makefile).
 * The free storage routines it calls are the C ones in free.c, through the
 * pointer-passing entry points below (the syntax layer src/impsyn64.imc
 * renames FALLOT to IMPFALLOT and so on).
 */
#include <stdint.h>
#include <string.h>
#include "imp.h"

extern word impl_stack(word *), impl_stinit(word *), impl_stpop(word *),
    impl_stpush(word *, word *, word *), impl_setstk(word *, word *, word *, word *),
    impl_pushmi(word *), impl_pullyu(word *), impl_bits(word *, word *, word *),
    impl_bitz(word *), impl_bitor(word *, word *, word *);

word fb;                                /* the base of free storage, for IMP */

static void sync(void) { fb = (word)(intptr_t)fmem; }

word stack(word n) { sync(); return impl_stack(&n); }
word stinit(void) { word nil = 0; sync(); return impl_stinit(&nil); }
word stpop(word n) { sync(); return impl_stpop(&n); }
word stpush(word a, word b, word c) { sync(); return impl_stpush(&a, &b, &c); }
word setstk(word n, word a, word b, word c) { sync(); return impl_setstk(&n, &a, &b, &c); }
void pushmi(word v) { sync(); impl_pushmi(&v); }
word pullyu(word sw) { sync(); return impl_pullyu(&sw); }
void bits(word a[3], word i, word j) { sync(); impl_bits(a, &i, &j); }
void bitz(word a[3]) { sync(); impl_bitz(a); }
void bitor_(word a[3], word i, word j) { sync(); impl_bitor(a, &i, &j); }

word
bit(const word a[3], word i, word j)
{
    word col = j / 64;

    if (i < 0 || i >= a[1] || col > a[2])
        return 0;
    return (word)((uword)FREE(a[0] + i + a[1] * col) & ((uword)1 << (j - 64 * col)));
}

void bstats(int reset) { (void)reset; }

/* ---- the free storage routines, as IMP calls them ------------------------ */

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
imperror(word *n, word *s)
{
    imp_error((int)*n, (const char *)s);
    return 0;
}
