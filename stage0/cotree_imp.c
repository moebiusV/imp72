/*
 * cotree_imp.c -- cotree.c's interface, over src/cotree.imp compiled by imp72stage0: COTREE is IMP; CFUDGE, CODENT,
 * CKDONE, MAXWELL, MAXEND, SETPRI, SAMEAS and PSTACK stay here (the semantic routines are entered in the
 * table of DSEM by address).  The state they share with COTREE is in common variables of the IMP module.
 * Ports decus/cotree.imc: COTREE, CFUDGE, CODENT, CKDONE, MAXWELL, MAXEND, SETPRI, SAMEAS, PSTACK.
 *
 * COTREE(S) adds one item of parser output to the code stack CS: S is a
 * production number, or minus the directory index of a terminal symbol.  For
 * a production, a match is tried between a path through the code tree and
 * the part of the stack from each position on.  A match copies that part of
 * the stack to the semantic routines' stack and calls DOSEM; the result
 * replaces what matched, and it is repeated until nothing matches.
 * COTREE(0) clears the stack.
 *
 * A stack entry is three words (36 bit):
 *
 *   word 0   HL: DI, usually a directory index  bits 7-6: AR, 1 integer,
 *            2 real, 3 byte  bits 5-0: TY
 *   word 1   VAL
 *   word 2   HL: CB, the start of its code  HR: CE, the end
 *
 * TY: 0 production (DI = its index in SEMS), 1 raw identifier, 2 register,
 * 4 constant, 8 identifier, 16 subscripted variable.  For 2, 8 and 16, VAL is
 * bit 35 the indirect bit, bits 28-18 the register, bits 17-0 the offset.
 *
 * Quoted semantics are collected, by Maxwell's demon (MAXWELL), in a free
 * array of their own, MAXIE, one for each, and MAXEND turns the demon off.
 *
 * The stack's items are kept in 36 bit words; where code is attached to an
 * item (CB, CE) it is the PDP-10 code of the original, which is not ported:
 * the routines that copy or compare code stop with an error.
 */
#include <stdint.h>
#include <string.h>
#include "imp.h"

extern word cs, ncs, entries, prio, demon, maxie, nmaxie, nc;
extern word impl_cotree(word *);


void
cfudge(void)
{
    entries = -1;
}

static word w_maxwell(const word *p) { return maxwell(p[0]); }
static word w_maxend(const word *p) { return maxend(p[0]); }
static word w_setpri(const word *p) { return setpri(p[0]); }

/* CODENT(A,B,C,F): put a stack entry back, for quoted semantics; if f, make
 * a fresh copy of the code. */
void
codent(word a, word b, word c, word f)
{
    word csx;

    if (entries == 0)
        entries = ncs;
    if (flen(cs) <= ncs + 3)
        faddex(&cs, ncs + 3);
    csx = cs + ncs;
    ncs += 3;
    FREES(csx, a);
    FREES(csx + 1, b);
    FREES(csx + 2, c);
    if (f && c)
        copyco(csx);
}

void
ckdone(void)
{
    word i;

    if (ncs > 6) {
        imp_error(1, "CODE GENERATION STACK DID NOT REDUCE TO ONE ITEM **");
        for (i = 0; i <= ncs - 3; i += 3)
            pstack(cs, i);
    }
}

/* MAXWELL(N): turn on the demon that diverts quoted semantics into MAXIE,
 * leaving n+1 phony type 1 entries in the stack for the quoted stuff. */
word
maxwell(word n)
{
    word j;

    nmaxie = 0;
    demon = 1;
    maxie = fallot_new("DEMON", 15);
    for (j = n; j >= 0; j--) {
        fadd(&cs, &ncs, 1);
        fadd(&cs, &ncs, 0);
        fadd(&cs, &ncs, 0);
    }
    return 0;
}

word
maxend(word a)
{
    demon = 0;
    fadd(&maxie, &nmaxie, 0);
    FREES(a + 1, maxie);
    return a;
}

word
setpri(word a)
{
    prio = a;
    return 0;
}

/* SAMEAS(SN,K): is the item at sn the same as the kth item of the partial
 * match?  If they have code, it is compared. */
word
sameas(word sn, word k)
{
    word tn = cs + nc + 3 * k, r, t, s;

    r = FREE(sn);
    t = FREE(tn);
    if (r != t)
        return 0;
    s = r & 077;
    r = FREE(sn + 1);
    t = FREE(tn + 1);
    if (r != t) {
        if ((s & 032) == 0)
            return 0;
        if ((u36(r) & 0774000777777L) != (u36(t) & 0774000777777L))
            return 0;
        goto smux;
    }
    r = FREE(sn + 2);
    t = FREE(tn + 2);
    if (r != t) {
        if (r == 0 || t == 0)
            return 0;
smux:
        if (mcode(sn, tn) == 0)
            return 0;
    }
    return -1;
}

void
pstack(word s, word n)
{
    pstac1(s, n);
}


/* COTREE(SS): see src/cotree.imp */
word
cotree(word ss)
{
    word r;

    r = impl_cotree(&ss);
    return r;
}

/* the semantic routines that COTREE enters in the table of DSEM (called through impglue.c) */
void
cotree_dsems(void)
{
    dsem("MAXWELL", w_maxwell);
    dsem("MAXEND", w_maxend);
    dsem("SETPRI", w_setpri);
}
