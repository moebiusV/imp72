/*
 * impsem_imp.c -- impsem.c's interface, over src/impsem.imp (the register machinery, NAME, DECL/DECLARE, the subroutine routines and
 * the rest, compiled by imp72stage0).  Left in C: FCON (it needs strtod), LETSYN (it drives the syntax reader), the table of the
 * names called with values (CCALL), the glue the IMP module calls (below), and the registration of the routines by name, for the
 * table of semantic routines is a table of C function pointers.  See impsem.c for the semantics of each routine.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "imp.h"

extern word impl_iinit(word *), impl_areg(word *), impl_reg2(word *), impl_areg1(word *, word *), impl_tstrn(word *),
    impl_name(word *), impl_val(word *), impl_sval(word *), impl_stackup(word *), impl_swtch(word *), impl_nsubp(word *),
    impl_hisreg(word *), impl_pairfirst(word *), impl_subreg(word *), impl_tmpreg(word *), impl_samereg(word *),
    impl_regeq(word *, word *), impl_regsim(word *, word *), impl_reg0(word *), impl_rmatch(word *, word *, word *),
    impl_subpr0(word *), impl_subprm(word *), impl_subtmpf(word *), impl_subbeg(word *), impl_subend(word *),
    impl_remot(word *), impl_callme(word *), impl_reg2s(word *), impl_decl(word *, word *), impl_declare(word *);
extern word arr, narr, regn;                    /* defined by the IMP module (src/shared.txt) */


#define ONE(f, g) word f(word a) { return g(&a); }
#define ZERO(f, g) word f(void) { word z = 0; return g(&z); }

ONE(areg, impl_areg)
ONE(reg2, impl_reg2)
word areg1(word i, word j) { return impl_areg1(&i, &j); }
ONE(tstrn, impl_tstrn)
ONE(name, impl_name)
ONE(val, impl_val)
ONE(sval, impl_sval)
ONE(stackup, impl_stackup)
ONE(switch_, impl_swtch)
ZERO(nsubp, impl_nsubp)
ONE(hisreg, impl_hisreg)
ONE(pairfirst, impl_pairfirst)
ONE(subreg, impl_subreg)
ONE(tmpreg, impl_tmpreg)
ONE(samereg, impl_samereg)
word regeq(word i, word j) { return impl_regeq(&i, &j); }
word regsim(word i, word j) { return impl_regsim(&i, &j); }
ONE(reg0, impl_reg0)
word rmatch(word i, word j, word rn) { return impl_rmatch(&i, &j, &rn); }
ONE(subpr0, impl_subpr0)
ONE(subprm, impl_subprm)
ZERO(subtmpf, impl_subtmpf)
ONE(subbeg, impl_subbeg)
ZERO(subend, impl_subend)
ONE(remot, impl_remot)
ONE(callme, impl_callme)
ONE(reg2s, impl_reg2s)
word decl(word i, word s) { return impl_decl(&i, &s); }
ONE(declare, impl_declare)

/* the declared arrays (ARR): entry i is name | (words << 18) */
word impsem_narr(void) { word z = 0; impl_iinit(&z); return narr; }
word
impsem_array(word i, word *size)
{
    word w = FREE(arr + i);

    *size = L18(w);
    return R18(w);
}
word impsem_nregs(void) { word z = 0; impl_iinit(&z); return regn; }

/* SUBTAG(N): the name of the tag for the entry of a subroutine called n: % and its first characters. */
word
subtag(word n)
{
    char buf[8];

    buf[0] = '%';
    strncpy(buf + 1, gname(n), 5);
    buf[6] = '\0';
    return dir(buf);
}

/*
 * LETSYN(S): `LET X = Y`: make the name x a synonym of the variable y by
 * entering the production <VBL> ::= x with the quoted semantics y.  s is the
 * stack entry whose second word is the array of parser output of the
 * quoted text, x first.
 */
word
letsyn(word s)
{
    word max = FREE(s + 1), j, k;

    synt(enstack(dir("VBL")), 0);
    sytrm(enstack(-FREE(max)));
    syntax(1);
    for (j = max; (k = FREE(j + 1)) != 0; j++)
        FREES(j, k);
    FREES(j, 0);
    equose(s);
    newlist(0);
    semantics();
    return enstack(0);
}

/*
 * CVAL(A): not in the original.  The name A is a C function that takes its
 * arguments by value; trans.c passes the values, not the addresses.
 */
static word cvalnames[64];
static int ncval;

word
ccall_mark(word a)
{
    word d = u36(FREE(a)) >> 18;
    int i;

    for (i = 0; i < ncval; i++)
        if (cvalnames[i] == d)
            return a;
    if (ncval < 64)
        cvalnames[ncval++] = d;
    return a;
}

int
is_cval(word d)
{
    int i;

    for (i = 0; i < ncval; i++)
        if (cvalnames[i] == d)
            return 1;
    return 0;
}

/*
 * FCON(A,B,E): the floating constant A.B"E, converted to the IEEE 754 double nearest to it (the original
 * summed the digits in 27 bit floating point, so that 0.25 came out one unit in the last place low).
 */
word
fcon(word a, word b, word e)
{
    char ai[64], bi[64], txt[160];
    const char *p;
    int ex = 0;
    double v;
    word bits;

    dname_str(L18(FREE(a)), ai, sizeof ai);
    dname_str(L18(FREE(b)), bi, sizeof bi);
    for (p = ai; *p; p++)
        if (*p < '0' || *p > '9')
            goto bad;
    for (p = bi; *p; p++)
        if (*p < '0' || *p > '9')
            goto bad;
    if (e) {
        if ((FREE(e) & 077) != 4)
            goto bad;
        ex = (int)FREE(e + 1);
        if (ex > 308 || ex < -330) {
            imp_error(1, "EXPONENT OUT OF RANGE.");
            ex = 0;
        }
    }
    snprintf(txt, sizeof txt, "%s.%se%d", ai[0] ? ai : "0", bi[0] ? bi : "0", ex);
    v = strtod(txt, NULL);
    goto done;
bad:
    imp_error(1, "ILLEGAL CHARACTER IN FLOATING CONSTANT.");
    v = 0;
done:
    memcpy(&bits, &v, sizeof bits);
    bits = real_enc(bits);
    FREES(a, 0204);
    FREES(a + 1, bits);
    return a;
}

static word w_name(const word *p) { return name(p[0]); }
static word w_areg1(const word *p) { return areg1(p[0], p[1]); }
static word w_areg(const word *p) { return areg(p[0]); }
static word w_reg2(const word *p) { return reg2(p[0]); }
static word w_val(const word *p) { return val(p[0]); }
static word w_sval(const word *p) { return sval(p[0]); }
static word w_decl(const word *p) { return decl(p[0], p[1]); }
static word w_declare(const word *p) { return declare(p[0]); }
static word w_stackup(const word *p) { return stackup(p[0]); }
static word w_switch(const word *p) { return switch_(p[0]); }
static word w_letsyn(const word *p) { return letsyn(p[0]); }
static word w_subbeg(const word *p) { return subbeg(p[0]); }
static word w_subend(const word *p) { (void)p; return subend(); }
static word w_subpr0(const word *p) { return subpr0(p[0]); }
static word w_remot(const word *p) { return remot(p[0]); }
static word w_cval(const word *p) { return ccall_mark(p[0]); }
static word w_callme(const word *p) { return callme(p[0]); }
static word w_fcon(const word *p) { return fcon(p[0], p[1], p[2]); }
static word w_reg2s(const word *p) { return reg2s(p[0]); }

void
impsem_init(void)
{
    dsem("NAME", w_name);
    dsem("AREG1", w_areg1);
    dsem("AREG", w_areg);
    dsem("REG2", w_reg2);
    dsem("VAL", w_val);
    dsem("SVAL", w_sval);
    dsem("DECL", w_decl);
    dsem("DECLARE", w_declare);
    dsem("STACKUP", w_stackup);
    dsem("SWITCH", w_switch);
    dsem("LETSYN", w_letsyn);
    dsem("SUBBEG", w_subbeg);
    dsem("SUBEND", w_subend);
    dsem("SUBPR0", w_subpr0);
    dsem("REMOT", w_remot);
    dsem("CALLME", w_callme);
    dsem("CVAL", w_cval);
    dsem("FCON", w_fcon);
    dsem("REG2S", w_reg2s);
}
