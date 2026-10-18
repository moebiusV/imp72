/*
 * cotree.c -- the semantic stack reducer.  Ports decus/cotree.imc: COTREE,
 * CFUDGE, CODENT, CKDONE, MAXWELL, MAXEND, SETPRI, SAMEAS, PSTACK.
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
#include <string.h>
#include "imp.h"

word cs, ncs;
word entries;
static word cst, nst, mark, prio = 1, ic, nc, ip, im, nm, node, snonr;
static word maxie, nmaxie;
static int demon;
static int init;

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

word
cotree(word ss)
{
    word s = ss, i, j, k, l, ll, t, immed, tr, trm;
    FILE *f;

    tr = (finam5 & 0110000L) != 0;
    trm = (finam5 & 010000L) != 0;
    if (!init) {
        init = 1;
        prio = 1;
        dsem("MAXWELL", w_maxwell);
        dsem("MAXEND", w_maxend);
        dsem("SETPRI", w_setpri);
        frelot(&cs, "COSTK", 90, NULL);
        ncs = 0;
        frelot(&cst, "COST2", 15, NULL);
    }
    f = imp_out();
    if (s == 0) {                               /* initialize */
        ncs = 0;
        return 0;
    }
    if (s == 1)                                 /* dummy production number */
        return 0;
    immed = 0;
    if (s > 0)
        immed = semed(s);
    if (tr) {
        fprintf(f, "COTREE CALLED WITH ");
        if (s < 0)
            pname(-s);
        else
            fprintf(f, "[%lo]", (long)s);
        if (immed) {
            fprintf(f, " IMMEDIATE EXECUTION");
            if ((immed & prio) == 0)
                fprintf(f, " (DEFERRED)");
        }
        fprintf(f, "; STACKED AT %ld MARK %ld\n", (long)ncs, (long)mark);
    }
    if (demon && (immed & prio) == 0) {
        fadd(&maxie, &nmaxie, s);
        return 0;
    }
    j = s < 0 ? sx36(1 | ((-s) << 18)) : sx36(s << 18);
    i = ncs + 3;
    if (flen(cs) <= i)
        faddex(&cs, i);
    k = cs + ncs;
    ncs = i;
    FREES(k, j);
    FREES(k + 1, 0);
    FREES(k + 2, 0);
    if (s < 0)
        return 0;

    if (immed & prio) {
        ic = ncs - 3;
xnow:
        if (mark == 2)
            mark = 4;
        j = snarg(s);
        nc = ic - 3 * j;
        j = semget(s);
        goto l1;
    }

doitagainsam:
    switch (mark) {
    case 0:
    case 1:
        goto m1;
    case 2:
        goto m2;
    case 3:
        goto m3;
    default:
        goto m4;
    }
m4:
    ip = im - 3;
    nc = nm;
    mark = 1;
    goto tryon;
m1:
    nc = -3;
nxst:
    if ((nc += 3) >= ncs) {
        mark = 3;
        return 0;
    }
    ip = nc;
l13:
    if (ip == ncs) {
        mark = 3;
        return 0;
    }
    j = FREE(cs + ip);
    if (j & 077) {
        ip += 3;
        goto l13;
    }
    s = L18(j);
    goto ck;
m3:
    ip = ncs - 3;
ck:
    if (trm) {
        fprintf(f, "PLIES[%lo]=%lo AT STACK %ld  NC=%ld\n", (long)s,
            (long)u36(plies(s)), (long)ip, (long)nc);
    }
    ic = ip;
    k = plies(s);
    if (k < 0)
        goto xnow;
    if (semed(s) & prio)
        goto xnow;
    if ((i = ls36(1, (ip - nc) / 3)) == 0)
        imp_error(0, "SEMANTIC CONDITION TOO COMPLEX.");
    for (;;) {
        if ((k & i) != 0)
            break;
        i = u36(i) >> 1;
        if (i == 0)
            goto nxst;
        nc += 3;
        if (nc >= ncs) {
            mark = 3;
            return 0;
        }
    }
    snonr = snoden(s);
    node = tscr(1);
    nst = 0;
    im = nm = nc;
tryon:
    if (im > ip) {
        if (im == ncs) {                        /* a partial match */
            mark = 2;
            return 0;
        }
        j = 0;
        while ((ip += 3) < ncs && ((j = FREE(cs + ip)) & 077))
            ;
        if (ip == ncs) {
            ip -= 3;
            snonr = -1;
        } else {
            if (semed(s = L18(j)) & prio) {
                ic = ip;
                mark = 2;
                goto xnow;
            }
            if ((k = plies(s)) > 0 && (k & ls36(1, (ip - nc) / 3)) == 0)
                goto poppit;
            snonr = snoden(s);
        }
    }
m2:
    j = match(&node, cs + im, snonr);
    mark = 1;
    if (trm) {
        fprintf(f, "MATCH NODE%4ld BEFORE #%ld VS STACK%3ld = ", (long)node,
            (long)snonr, (long)im);
        if (j > 0)
            fprintf(f, "%lo\n", (long)j);
        else
            fprintf(f, "%ld\n", (long)j);
    }
    if (j == 0) {
tryalt:
        if (node && (node = talt(node)) != 0)
            goto m2;
poppit:
        if (nst) {
            nst--;
            if ((077 & (j = FREE(cs + im))) == 0) {
                s = L18(j);
                snonr = snoden(s);
                ip = im;
            }
            im -= 3;
            node = FREE(cst + nst) & 0777777;
            goto tryalt;
        }
        goto nxst;
    }
    if (j < 0) {
        if ((t = tscr(node))) {
            /* mark a VAL/ node read as its original production */
            if (j == -2)
                node |= 0400000000000L;
            fadd(&cst, &nst, sx36(node));
            im += 3;
            node = t;
            goto tryon;
        }
        goto tryalt;
    }
    ic = im;
    nc = nm;

    /* a match for nc through ic: invoke [j] */
l1:
    ic += 3;
    l = stinit();
    ll = 0;
    for (i = nc; i <= ic - 3; i += 3) {
        k = cs + i;
        if ((FREE(k) & 077) == 0) {
            /* ignore productions unless they are VAL/ arguments */
            if (FREE(k + 1) == 0)
                continue;
            if (FREE(cst + (i - nc) / 3) < 0)
                continue;
        }
        FREES(l, FREE(k));
        FREES(l + 1, FREE(k + 1));
        FREES(l + 2, FREE(k + 2));
        l += 3;
        ll--;
    }
    stpop(ll);

    entries = 0;
    dosem(j);
    if (mark != 4)
        mark = 1;

    if (entries > 0) {                          /* items from quoted semantics */
        if ((i = entries - ic)) {
            j = ncs + i;
            if (flen(cs) <= j)
                faddex(&cs, j);
            memmove(freep(cs + ncs), freep(cs + ic), (size_t)i * sizeof(word));
            ncs = j;
        }
        ic = entries;
        goto l2;
    }

    j = stack(0);
    k = cs + nc;
    FREES(k, FREE(j));
    FREES(k + 1, FREE(j + 1));
    FREES(k + 2, FREE(j + 2));
    if (tr) {
        fprintf(f, "REDUCED AT %ld. TOP=%ld, RESULT: ", (long)(ic - 3),
            (long)(ncs + nc - ic));
        pstack(cs, nc);
    }
    if (entries < 0)
        goto doitagainsam;
    nc += 3;
l2:
    if (ic < ncs && nc < ic)
        memmove(freep(cs + nc), freep(cs + ic), (size_t)(ncs - ic) * sizeof(word));
    ncs += nc - ic;
    if (tr == 0 || entries <= 0)
        goto doitagainsam;
    fprintf(f, "STACK AFTER INSERTING QUOTED SEMANTICS IS:\n");
    for (i = 0; i <= ncs - 3; i += 3)
        pstack(cs, i);
    goto doitagainsam;
}
