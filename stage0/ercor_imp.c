/*
 * ercor_imp.c -- ercor.c's interface, over src/ercor.imp compiled by imp72stage0: ERCOR, FSUB, ISNAME
 * and AMBIG of decus/ercor.imc.  The by-value C entry points.
 */
#include <stdint.h>
#include "imp.h"

extern word impl_ercor(word *, word *, word *), impl_isname(word *),
    impl_ambig(word *, word *, word *, word *, word *);


word
ercor(word op, word np, word ss)
{
    word r;

    r = impl_ercor(&op, &np, &ss);
    return r;
}

word
isname(word s)
{
    return impl_isname(&s);
}

void
ambig(word p, word *np, word *pn, word o, word tr)
{
    impl_ambig(&p, np, pn, &o, &tr);
}
