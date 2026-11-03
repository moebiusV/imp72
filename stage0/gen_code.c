/*
 * gen_code.c -- ports src/llvm-ir/code.imp: UNPCK, REGOF, FETCH, ADDOP, DEWOP, TAG, STORE, COPYC1, REGFIELD, COPYCO, NEXCO, MCODE1,
 * MCODE, SUBARG, and CODEI (the DSEMs of this file; CODEI of the IMP also enters CODE2I, which gen_init does).
 *
 * The code of a stack entry is the pair (first,last) of records of the chain CO in its third word (HL, HR; 0: no code); a record is
 * five words (G_RNEXT .. G_RX in gen.h).  The routines are the original's with the instructions they make changed to the operations of
 * the IR machine: no immediate, self or both modes, no halfword operations, no register pairs, no checking whether a register is the
 * user's when an operation clobbers it, no looking in the code for where a register is used.
 */
#include <stdlib.h>
#include <string.h>
#include "gen.h"

word g_se, g_tse, g_dse, g_se1, g_rse, g_cse;           /* SE, TSE, DSE, SE1, RSE, CSE: the unpacked stack entry */

static word copys, ncop;                                /* COPYS, NCOP: the correspondence of registers of two codes */

word
g_unpck(word s)
{
    g_se = FREE(s); g_tse = g_se & 077; g_dse = HL(g_se);
    g_se1 = FREE(s + 1); g_rse = 03777 & HL(g_se1); g_cse = HR(g_se1);
    return 0;
}

word
g_regof(word s)
{
    return 03777 & HL(FREE(s + 1));
}

word
g_fetch(word s)
{
    if ((FREE(s) & 077) != 2)
        g_dewop(OLOAD, g_areg1(1, 015), s);
    return s;
}

/* ADDOP(OP,S,T) is HOOK(S,T,DEWOP(OP,REGOF(FETCH(T)),S)); the call of IMP evaluates from the right: the innermost is FETCH(T) */
word
g_addop(word op, word s, word t)
{
    word r;

    r = g_regof(g_fetch(t));
    return g_hook(s, t, g_dewop(op, r, s));
}

/* DEWOP(OPP,RR,S): add to the code of S an operation OP with accumulator RR (0: none) and the operand S.  S becomes type 2 (register RR)
 * unless RR=0.  A negative RR is its absolute value: the original uses it to mean "do not protect a user register" */
word
g_dewop(word opp, word rr, word s)
{
    word r, op;

    r = rr;
    if (r < 0)
        r = 0 - rr;
    op = opp;
    if (op >= 256)
        imp_error(1, "AN OPERATION OF THE PDP-10 THAT THE IR MACHINE DOES NOT HAVE.");
    g_unpck(s);
    g_addrec(s, KOP, op, r, g_se, g_se1, 0);
    if (r != 0) {
        FREES(s, ((2 | (g_se & 0777700))));
        FREES(s + 1, G_L32(r));
    }
    return s;
}

word
g_tag(word s)
{
    g_addrec(s, KTAG, 0, 0, 0, 0, HL(FREE(s)));
    return s;
}

/* STORE(S,T): get T into S (which is not of type 4), hooking the code for T before that of S.  The original turns an add, subtract, etc.
 * of a register just stored into the instruction's "to memory" form, and stores 0 and -1 with one instruction; the back end does that */
word
g_store(word s, word t)
{
    word s1;

    g_unpck(s);
    if (g_tse == 2) {
        s1 = g_rse;
        g_hook(s, g_dewop(OLOAD, 0 - s1, t), s);
        return s;
    }
    g_fetch(t);
    g_hook(s, t, s);
    g_dewop(OSTORE, g_regof(t), s);
    return s;
}

/* the registers of a code that is copied or compared: COPYS has pairs (old register left, new register right) */
word
g_copyc1(word j)
{
    word k, i;

    if (ncop != 0) {
        i = ncop - 1;
        do {
            k = FREE(copys + i);
            if (HL(k) == j)
                return HR(k);
            i = i - 1;
        } while (i >= 0);
    }
    i = g_samereg(j);
    fadd(&copys, &ncop, HW(j, i));
    return i;
}

/* the entry word 1 with the register field (bits 32-42, 0 for none) changed from R to RN */
word
g_regfield(word w, word r, word rn)
{
    return ((w ^ G_L32(r)) | G_L32(rn));
}

/* COPYCO(S): a fresh copy (with new registers) of the code of stack entry S */
word
g_copyco(word s)
{
    word cj, w0, w1, rd, r, ct;

    ncop = 0;
    if (copys == 0)
        copys = fallot_new("CPYCO", 20);
    cj = HL(FREE(s + 2));
    FREES(s + 2, 0);
    while (cj != 0) {                                   /* CP1 */
        rd = G_RRD(cj); w0 = G_RV0(cj); w1 = G_RV1(cj);
        if (rd != 0)
            rd = g_copyc1(rd);
        if ((w0 & 077) != 4) {
            r = 03777 & HL(w1);
            if (r != 0)
                w1 = g_regfield(w1, r, g_copyc1(r));
        }
        g_addrec(s, G_RKIND(cj), G_ROP(cj), rd, w0, w1, G_RX(cj));
        cj = G_RNEXT(cj);
    }
    w1 = FREE(s + 1);
    ct = FREE(s) & 077;
    if (ct == 2) {
        r = 03777 & HL(w1);
        if (r != 0)
            FREES(s + 1, g_regfield(w1, r, g_copyc1(r)));
    }
    if (ct == 020) {
        r = 03777 & HL(w1);
        if (r != 0)
            FREES(s + 1, g_regfield(w1, r, g_copyc1(r)));
    }
    return 0;
}

word
g_nexco(word i)
{
    (void)i;
    return 0;
}

/* MCODE1(I,J): is register I of one code the register J of the other?  (A correspondence is kept in COPYS) */
word
g_mcode1(word i, word j)
{
    word b, a, t, k;

    if (i == j)
        return 1;
    b = HW(i, j);
    if (ncop != 0) {
        k = ncop - 1;
        do {
            a = FREE(copys + k);
            if (a == b)
                return 1;
            t = HL(a);
            if ((t == i) | (t == j))
                return 0;
            t = HR(a);
            if ((t == i) | (t == j))
                return 0;
            k = k - 1;
        } while (k >= 0);
    }
    if (g_regsim(i, j) == 0)
        return 0;
    fadd(&copys, &ncop, b);
    return 1;
}

/* MCODE(S,T): 1 if the code for S and for T is the same, up to a correspondence between the registers */
word
g_mcode(word s, word t)
{
    word js, jt, rs, rt, w0s;

    if (copys == 0)
        copys = fallot_new("CPYCO", 20);
    ncop = 0;
    if (g_mcode1(03777 & HL(FREE(s + 1)), 03777 & HL(FREE(t + 1))) == 0)
        return 0;
    js = HL(FREE(s + 2)); jt = HL(FREE(t + 2));
    for (;;) {                                          /* MC1 */
        if ((js | jt) == 0)
            return 1;
        if (js == 0)
            return 0;
        if (jt == 0)
            return 0;
        if (G_RKIND(js) != G_RKIND(jt))
            return 0;
        if (G_ROP(js) != G_ROP(jt))
            return 0;
        if (G_RX(js) != G_RX(jt))
            return 0;
        if (G_RV0(js) != G_RV0(jt))
            return 0;
        rs = G_RRD(js); rt = G_RRD(jt);
        if ((rs == 0) != (rt == 0))
            return 0;
        if (rs != 0)
            if (g_mcode1(rs, rt) == 0)
                return 0;
        w0s = G_RV0(js);
        if ((w0s & 077) == 4) {
            if (G_RV1(js) != G_RV1(jt))
                return 0;
        } else {
            rs = 03777 & HL(G_RV1(js)); rt = 03777 & HL(G_RV1(jt));
            if ((rs == 0) != (rt == 0))
                return 0;
            if (rs != 0)
                if (g_mcode1(rs, rt) == 0)
                    return 0;
            if (HR(G_RV1(js)) != HR(G_RV1(jt)))
                return 0;
            if ((G_RV1(js) < 0) != (G_RV1(jt) < 0))
                return 0;
        }
        js = G_RNEXT(js); jt = G_RNEXT(jt);
    }
}

/* SUBARG(S,PP): force the argument PP into something with an address for a subroutine call, storing it if necessary.  A variable and a
 * subscripted variable have one; a register is stored in a temporary word; a constant is given a name */
word
g_subarg(word s, word pp)
{
    word p, tm, s1, s2, j;

    (void)s;
    p = pp;
    g_unpck(p);
    if (g_tse == 010)
        return p;
    if (g_tse == 2) {
        tm = g_subprm(0);
        s1 = FREE(tm); s2 = FREE(tm + 1);
        g_store(tm, p);
        FREES(tm, s1); FREES(tm + 1, s2);
        return tm;
    }
    if (g_tse == 4) {
        if (g_dse == 0) {
            j = stcon(FREE(p + 1));
            FREES(p, ((HR(g_se) | G_L32(j))));
        }
        FREES(p + 1, 0);
        return p;
    }
    return p;
}

static word w_store(const word *p) { return g_store(p[0], p[1]); }
static word w_fetch(const word *p) { return g_fetch(p[0]); }
static word w_regof(const word *p) { return g_regof(p[0]); }
static word w_dewop(const word *p) { return g_dewop(p[0], p[1], p[2]); }
static word w_addop(const word *p) { return g_addop(p[0], p[1], p[2]); }
static word w_tag(const word *p) { return g_tag(p[0]); }

/* CODEI(NIL): the DSEMs of this file */
void
gen_code_dsems(void)
{
    dsem("STORE", w_store);
    dsem("FETCH", w_fetch);
    dsem("REGOF", w_regof);
    dsem("DEWOP", w_dewop);
    dsem("ADDOP", w_addop);
    dsem("TAG", w_tag);
}
