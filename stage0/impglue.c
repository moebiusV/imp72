/*
 * impglue.c -- the C routines that IMP modules compiled by imp72stage0 call.
 *
 * An IMP module passes every argument by reference, and finds free storage at the
 * address FB (src/impsyn64.imc).  So a routine of the C side that IMP calls has an entry
 * here that takes pointers, named IMPNAME in the module's source, and that puts the
 * current free storage block in FB after the routine, which may have moved it.
 */
#include <stdint.h>
#include <stdio.h>
#include "imp.h"

extern word fb;                         /* the base of free storage, for IMP */

static void sync(void) { fb = (word)(intptr_t)fmem; }

word impbits(word *a, word *i, word *j) { bits(a, *i, *j); sync(); return 0; }
word impbitor(word *a, word *i, word *j) { bitor_(a, *i, *j); sync(); return 0; }
word impbitz(word *a) { bitz(a); return 0; }
word imppushmi(word *v) { pushmi(*v); sync(); return 0; }
word imppullyu(word *sw) { return pullyu(*sw); }
word imppout(word *q, word *v) { word r = pout(*q, *v); sync(); return r; }
word imppname(word *i) { pname(*i); return 0; }
word impdname(word *i, word *j) { return dname(*i, *j); }
word impmsec(word *unused) { (void)unused; return (word)imp_msec(); }

/* the free storage routines, as IMP calls them (FALLOT is IMPFALLOT and so on: src/impsyn64.imc) */

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
    imp_error((int)*n, s != NULL && *(const char *)s != '\0' ? (const char *)s : NULL);
    return 0;
}

/* FINCSET as IMP calls it */
word
impfincset(word *a, word *n)
{
    fincset(*a, *n);
    return 0;
}

/* the parser's calls (src/parse.imp) */
word impggoal(word *nt) { word r = ggoal(*nt); sync(); return r; }
word impgnext(word *q, word *g) { return gnext(*q, *g); }
word impgnt2tx(word *nt, word *ni, word *gl, word *no) { return *no = gnt2tx(*nt, ni, *gl); }
word impgmade(word *c, word *g) { return gmade(*c, *g); }
word impgpout1(word *c, word *n, word *p) { gpout1(*c, *n, *p); sync(); return 0; }
word impgpout2(word *c, word *g, word *p) { gpout2(*c, *g, *p); sync(); return 0; }
word impisname(word *s) { return isname(*s); }
word impercor(word *op, word *np, word *ss) { word r = ercor(*op, *np, *ss); sync(); return r; }
word impambig(word *p, word *np, word *pn, word *o, word *tr)
{
    ambig(*p, np, pn, *o, *tr);
    sync();
    return 0;
}
word implex(word *s) { word r = lex(s); sync(); return r; }
word imppline1(word *ol) { pline1(*ol); return 0; }
word impmsg(word *s) { imp_msg((const char *)s); return 0; }
word impoutstr(word *s) { fputs((const char *)s, stderr); return 0; }
word imppartr1(word *cn, word *ct, word *cv, word *p, word *ip, word *o, word *cw)
{
    partr1(*cn, *ct, *cv, *p, *ip, *o, *cw);
    return 0;
}
word impcotree(word *k) { if (parse_cotree != NULL) parse_cotree(*k); sync(); return 0; }
