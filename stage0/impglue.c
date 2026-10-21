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
#include <stdlib.h>
#include "imp.h"

extern word fb;                         /* the base of free storage, for IMP */

static void
sync(void)
{
    static int check = -1;

    fb = (word)(intptr_t)fmem;
    if (check < 0)
        check = getenv("IMP_FCHECK") != NULL;
    if (check)
        fcheck(__func__);
}

word impbits(word *a, word *i, word *j) { bits(a, *i, *j); sync(); return 0; }
word impbitor(word *a, word *i, word *j) { bitor_(a, *i, *j); sync(); return 0; }
word impbitz(word *a) { bitz(a); return 0; }
word imppushmi(word *v) { pushmi(*v); sync(); return 0; }
word imppullyu(word *sw) { return pullyu(*sw); }
word imppout(word *q, word *v) { word r = pout(*q, *v); sync(); return r; }
/* PNAME as the original has it, a PRINT of the name, so that it goes through the same line buffer
 * as the PRINT statements around it */
extern long imp_pr(long n, int64_t *p);

word
imppname(word *i)
{
    char buf[128];
    int64_t w = 0;

    if (*i < 0)
        return 0;
    dname_str(*i, buf, sizeof buf);
    imp_pr(3, &w);
    imp_pr(0, (int64_t *)(void *)buf);
    return 0;
}
word impdname(word *i, word *j) { word r = dname(*i, *j); sync(); return r; }
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
word impgnext(word *q, word *g) { word r = gnext(*q, *g); sync(); return r; }
word impgnt2tx(word *nt, word *ni, word *gl, word *no) { *no = gnt2tx(*nt, ni, *gl); sync(); return *no; }
word impgmade(word *c, word *g) { word r = gmade(*c, *g); sync(); return r; }
word impgpout1(word *c, word *n, word *p) { gpout1(*c, *n, *p); sync(); return 0; }
word impgpout2(word *c, word *g, word *p) { gpout2(*c, *g, *p); sync(); return 0; }
word impisname(word *s) { word r = isname(*s); sync(); return r; }
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
word impgconn(word *q, word *g) { word r = gconn(*q, *g); sync(); return r; }
word impgentry(word *unused) { (void)unused; return gentry(); }

/* the IMP modules' PRINT goes where the C modules' output does: the listing */
extern FILE *(*imp_defout)(void);

__attribute__((constructor)) static void
setdefout(void)
{
    imp_defout = imp_out;
}

/* the code tree builder's calls (src/entree.imp) */
word impconvc(word *c, word *k) { word r = convc(*c); *k = convc_const; sync(); return r; }
word impsnarg(word *s) { word r = snarg(*s); sync(); return r; }
word impsnoden(word *s) { word r = snoden(*s); sync(); return r; }
word impnoteprod(word *p, word *n) { noteprod(*p, *n); sync(); return 0; }
word impenprod(word *p, word *n, word *s) { enprod(*p, *n, *s); sync(); return 0; }
word impmatchp(word *p, word *n) { word r = matchp(*p, *n); sync(); return r; }
word impsameas(word *s, word *k) { word r = sameas(*s, *k); sync(); return r; }
word imponeword(word *s) { word r = oneword(*s); sync(); return r; }

/* DIR of a name packed five characters to a word, as the IMP source builds it */
word
impdirp(word *na)
{
    char buf[64];
    int n = 0, i;

    for (; n < 60; na++) {
        for (i = 0; i < 5; i++) {
            char ch = (char)((*na >> (29 - 7 * i)) & 0x7F);

            if (ch == '\0') {
                buf[n] = '\0';
                return dir(buf);
            }
            buf[n++] = ch;
        }
    }
    buf[n] = '\0';
    return dir(buf);
}

/* the semantic stack reducer's calls (src/cotree.imp) */
extern void cotree_dsems(void);

word impdsems(word *unused) { (void)unused; cotree_dsems(); return 0; }
word impsemed(word *a) { word r = semed(*a); sync(); return r; }
word impplies(word *a) { word r = plies(*a); sync(); return r; }
word impsemget(word *a) { word r = semget(*a); sync(); return r; }
word impdosem(word *a) { word r = dosem(*a); sync(); return r; }
word impstinit(word *unused) { word r; (void)unused; r = stinit(); sync(); return r; }
word impstpop(word *n) { word r = stpop(*n); sync(); return r; }
word impstack(word *n) { word r = stack(*n); sync(); return r; }
word imppstack(word *s, word *n) { pstack(*s, *n); sync(); return 0; }
word impmatch(word *tnp, word *sn, word *snonr) { word r = match(tnp, *sn, *snonr); sync(); return r; }

/* the semantics interpreter's calls (src/dosem.imp) */
extern void dosem_dsems(void);

word impdsems2(word *unused) { (void)unused; dosem_dsems(); return 0; }
word impsetstk(word *n, word *a, word *b, word *c) { word r = setstk(*n, *a, *b, *c); sync(); return r; }
word impcodent(word *a, word *b, word *c, word *f) { codent(*a, *b, *c, *f); sync(); return 0; }
word impcfudge(word *unused) { (void)unused; cfudge(); return 0; }
word impsemcall(word *id, word *pars) { word r = semcall(*id, pars); sync(); return r; }
word impsynarg(word *i) { word r = synarg(*i); sync(); return r; }
word impnewnameg(word *x) { word r = newname(gname(*x)); sync(); return r; }
