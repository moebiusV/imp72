/*
 * ercor_imp.c -- ercor.c's interface, over src/ercor.imp compiled by imp72stage0: ERCOR, FSUB, ISNAME
 * and AMBIG of decus/ercor.imc.  The by-value C entry points.
 */
#include <stdint.h>
#include "imp.h"

extern word fb;                         /* the base of free storage, for IMP */
extern word impl_ercor(word *, word *, word *), impl_isname(word *),
    impl_ambig(word *, word *, word *, word *, word *);

static void
sync(void)
{
    fb = (word)(intptr_t)fmem;
}

word
ercor(word op, word np, word ss)
{
    word r;

    sync();
    r = impl_ercor(&op, &np, &ss);
    sync();
    return r;
}

word
isname(word s)
{
    sync();
    return impl_isname(&s);
}

void
ambig(word p, word *np, word *pn, word o, word tr)
{
    sync();
    impl_ambig(&p, np, pn, &o, &tr);
}
