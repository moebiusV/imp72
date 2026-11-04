/*
 * gen_code2.c -- ports src/llvm-ir/code2.imp: ADDREC (ADDCODE of the original), SETNEXT, HOOK, ANYCODE, DEWFUN, CONOP, JOP, CJUMP,
 * MOVEBLK, FREEZE, ONEWORD, COPPY, SUBRCALL, RETURN, FLOAT, FIX, DATAST, SUBRPAR, PRINPAR, PRINCAL, OJUMPOP, ADDR and CODE2I (the
 * DSEMs of this file).  The code is a chain of records in the free array CO; the code of a stack entry is the pair (first,last) in
 * its third word (left and right 32 bit halves; 0: no code).
 *
 * In IMP a relation used as a value is -1 for true and 0 for false (tried with the stage compiler); CONOP's comparisons give those.
 */
#include <stdlib.h>
#include <string.h>
#include "gen.h"

word g_co, g_nco, g_anco;

/* SETNEXT(R,N): make record N the one after record R */
word
g_setnext(word r, word n)
{
    FREES(g_co + r, HW(n, HR(FREE(g_co + r))));
    return 0;
}

/* ADDREC(S,KIND,OP,RD,V0,V1,X): add a record to the code of stack entry S.  (The IMP copies its arguments first because an argument like FREE(R)
 * is passed by reference and the arena moves when FADDEX grows an array; the C ones are values.) */
word
g_addrec(word s, word kind, word op, word rd, word v0, word v1, word x)
{
    word z, r;

    if (g_co == 0) {
        g_nco = 1;
        frelot(&g_co, "CODE", 200, NULL);
        fincset(g_co, 200);
    }
    if (g_anco == -1)
        g_anco = 0;
    else
        g_anco = 1;
    if (flen(g_co) <= (g_nco + G_RECW))
        faddex(&g_co, (g_nco + G_RECW) + 3);
    r = g_nco;
    g_nco = g_nco + G_RECW;
    FREES(g_co + r, HW(0, (kind | g_ls(op, 8))));
    FREES((g_co + r) + 1, rd);
    FREES((g_co + r) + 2, v0);
    FREES((g_co + r) + 3, v1);
    FREES((g_co + r) + 4, x);
    z = FREE(s + 2);
    if (z == 0)
        FREES(s + 2, HW(r, r));
    else {
        g_setnext(HR(z), r);
        FREES(s + 2, HW(HL(z), r));
    }
    return s;
}

/* HOOK(D,A,B): hooks code for A to code for B, puts code in D.  The value of D is set to the value of B */
word
g_hook(word d, word a, word b)
{
    word j, k;

    FREES(d, FREE(b));
    FREES(d + 1, FREE(b + 1));
    j = FREE(b + 2);
    if (j == 0) {
        FREES(d + 2, FREE(a + 2));
        return d;
    }
    k = FREE(a + 2);
    if (k == 0) {
        FREES(d + 2, j);
        return d;
    }
    g_setnext(HR(k), HL(j));
    FREES(d + 2, HW(HL(k), HR(j)));
    return d;
}

word
g_anycode(void)
{
    return g_anco;
}

/* DEWFUN(S,I,J): special function I.  2 ends a call (the value is in register J); the others (saving and restoring the PDP-10's registers,
 * declaring them) do nothing in a machine with as many registers as we like, but still count as code for ANYCODE */
word
g_dewfun(word s, word i, word j)
{
    if (g_anco == 0)
        g_anco = -1;
    if (i == 2)
        return g_addrec(s, KOP, OCALLE, j, 0, 0, 0);
    if (g_anco == -1)
        g_anco = 0;
    else
        g_anco = 1;
    return s;
}

/* an arithmetic left shift of a word by any count (the shifts of the stage compiler, ESHIFTV): the count is signed */
static word
als(word x, word n)
{
    if (n >= 64)
        return 0;
    if (n <= -64)
        return x < 0 ? -1 : 0;
    if (n >= 0)
        return (word)((uword)x << n);
    return x >> -n;
}

/* CONOP(S,T,I,V,W): an operation on one or two constants S and T, by the index I (the low 32 bits) and, if the rest of I is 1 or 2, a conditional:
 * the value of the comparison chooses the entry V (or W) for the whole result.  The arithmetic is that of 64 bit words; an operation on real
 * constants is not folded (only the negation, which flips the sign) */
word
g_conop(word s, word t, word i, word v, word w)
{
    static word co2;                    /* CO2 is a static local: a one-operand operation (T=0) leaves it as it was */
    word co1, ii, c, j;

    co1 = FREE(s + 1);
    if (t != 0)
        co2 = FREE(t + 1);
    ii = HR(i);
    if ((FREE(s) & 0300) == 0200) {
        if (ii == 0)
            co1 = co1 ^ (word)((uword)1 << 63);
        else
            imp_error(1, "OPERATION ON A REAL CONSTANT. ");
        goto cxc;
    }
    switch (ii) {                       /* GO TO (C0,...,C19) II; an index beyond the table falls through to C0 */
    default:
    case 0:
        co1 = (word)(0 - (uword)co1);
        break;
    case 1:
        co1 = (word)((uword)co1 + (uword)co2);
        break;
    case 2:
        co1 = (word)((uword)co1 - (uword)co2);
        break;
    case 3:
        co1 = (word)((uword)co1 * (uword)co2);
        break;
    case 4:                             /* division by zero traps here, and the compiler would die */
        if (co2 == 0)
            co1 = 0;
        else if (co2 == -1)
            co1 = (word)(0 - (uword)co1);
        else
            co1 = co1 / co2;
        break;
    case 5:
        co1 = g_ls(co1, co2);
        break;
    case 6:
        co1 = ~co1;
        break;
    case 7:
        co1 = co1 & co2;
        break;
    case 8:
        co1 = co1 | co2;
        break;
    case 9:
        co1 = (co1 < co2) ? -1 : 0;
        break;
    case 10:
        co1 = (co1 == co2) ? -1 : 0;
        break;
    case 11:
        co1 = (co1 <= co2) ? -1 : 0;
        break;
    case 12:                            /* this space available - call (314) 159-2656: C12 has no statement and falls into C13 */
    case 13:
        co1 = (co1 >= co2) ? -1 : 0;
        break;
    case 14:
        co1 = (co1 != co2) ? -1 : 0;
        break;
    case 15:
        co1 = (co1 > co2) ? -1 : 0;
        break;
    case 16:
        co1 = als(co1, co2);
        break;
    case 17: {                          /* LROT: a rotation by the count and 63 */
        unsigned n = (unsigned)(co2 & 63);

        co1 = n == 0 ? co1 : (word)(((uword)co1 << n) | ((uword)co1 >> (64 - n)));
        break;
    }
    case 18:
        co1 = co1 ^ co2;
        break;
    case 19:
        co1 = ~(co1 ^ co2);
        break;
    }
    c = g_rs(i, 32);                    /* CX: special dealie for conditionals */
    if (c != 0) {
        if (c == 1) {
            if (co1 == 0)
                goto cxc;
            for (j = 2; j >= 0; j--)
                FREES(s + j, FREE(v + j));
            goto cxt;
        }
        if (c == 2) {
            if (co1 == 0) {
                for (j = 2; j >= 0; j--)
                    FREES(s + j, FREE(w + j));
            } else {
                for (j = 2; j >= 0; j--)
                    FREES(s + j, FREE(v + j));
            }
            goto cxt;
        }
    }
cxc:
    FREES(s + 1, co1);
    j = FREE(s);
    FREES(s, HR(j));
    if (t != 0)
        g_hook(s, t, s);
cxt:
    return s;
}

/* JOP(C): the jump operation for the relation with code C of the relational operators (1 <, 2 =, 3 LE, 5 GE, 6 NE, 7 >); on reals the one 10 more.
 * (The numbers are decimal: OJLT 42, OJEQ 41, ...) */
word
g_jop(word c)
{
    if (c == 1)
        return 42;
    if (c == 2)
        return 41;
    if (c == 3)
        return 43;
    if (c == 5)
        return 44;
    if (c == 6)
        return 46;
    return 45;
}

/* CJUMP(OP,P,R,S): jump to the tag S if P relates to R by OP */
word
g_cjump(word op, word p, word r, word s)
{
    word o, f;

    o = op; f = 0;
    if ((FREE(p) & 0300) == 0200)
        f = 1;
    if ((FREE(r) & 0300) == 0200)
        f = 1;
    if (f != 0) {
        o = op + OFJ;
        g_float(p);
        g_float(r);
    }
    g_fetch(p);
    g_addrec(r, KOP, o, g_regof(p), FREE(r), FREE(r + 1), HW(HR(FREE(s + 1)), HL(FREE(s))));   /* the tag, and in the high half the offset of S (GO TO CND[1]) */
    return g_hook(p, p, r);
}

/* MOVE B THROUGH N TO A: N+1 words from B to A, ascending (the library routine IMP_BLK, called like any subroutine: the arguments are pointers) */
word
g_moveblk(word b, word n, word a)
{
    word t;

    t = g_name(enstack(dir("impblk")));
    g_subrcall(t);
    g_subrpar(t, a); g_subrpar(t, b); g_subrpar(t, n);
    g_dewfun(t, 2, g_regof(t));
    g_subpr0(1);
    return t;
}

/* FREEZE(S): the original flags the code so that the assembler does not delete instructions from it; the back end does its own arithmetic, so nothing */
word
g_freeze(word s)
{
    return s;
}

/* ONEWORD(S): 1 if the code of stack entry S is exactly one operation */
word
g_oneword(word s)
{
    word j;

    j = FREE(s + 2);
    if (j == 0)
        return 0;
    if (HL(j) == HR(j)) {
        if (G_RKIND(HL(j)) == KOP)
            return 1;
    }
    return 0;
}

/* COPPY(S): a copy of the entry S, with a copy of its code (the semantic routine COPY) */
word
g_coppy(word s)
{
    word w;

    w = enstack(0);
    FREES(w, FREE(s)); FREES(w + 1, FREE(s + 1));
    FREES(w + 2, FREE(s + 2));
    if (FREE(w + 2) != 0)
        g_copyco(w);
    return w;
}

/* SUBRCALL(T): begin a call of the function named at stack entry T; its value will be in a new register */
word
g_subrcall(word t)
{
    word tt, dd, j;

    g_subpr0(0);
    tt = FREE(t); dd = HL(tt);
    if (dprop(DP_COM, dd) < 2)
        dprops(DP_COM, dd, 1);
    g_addrec(t, KOP, OCALLB, 0, FREE(t), FREE(t + 1), 0);
    j = g_areg1(0140, 0);
    FREES(t, (2 | (tt & ~(word)077)));
    FREES(t + 1, G_L32(j));
    return t;
}

/* RETURN(S): return the value of S from the current subroutine */
word
g_return(word s)
{
    g_dewop(OLOAD, 0 - g_areg1(0100, 0), s);
    g_addrec(s, KOP, ORET, g_regof(s), 0, 0, 0);
    return s;
}

/* FLOAT(S): insure that S is real */
word
g_float(word s)
{
    word sf, j, b;
    double x;

    sf = FREE(s);
    if ((sf & 0300) != 0200) {
        if ((sf & 077) == 4) {                          /* a constant: the double's bits */
            x = (double)FREE(s + 1);
            memcpy(&b, &x, 8);                          /* (a real passed as an argument is fixed: the bits go through B) */
            FREES(s + 1, b);
            FREES(s, ((0204 | (sf & 0777000))));
            return s;
        }
        g_fetch(s);
        j = g_regof(s);
        g_addrec(s, KOP, OFLOAT, j, FREE(s), FREE(s + 1), 0);
        FREES(s, ((0202 | (sf & ~(word)0377))));
    }
    return s;
}

/* FIX(S): insure that S is an integer */
word
g_fix(word s)
{
    word sf, j, b, r;
    double x;

    sf = FREE(s);
    if ((sf & 0300) == 0) {                             /* no type (a parameter): an integer */
        FREES(s, (sf | 0100));
        return s;
    }
    if ((sf & 0300) != 0100) {
        if ((sf & 077) == 4) {
            b = FREE(s + 1);
            memcpy(&x, &b, 8);
            if (x != x || x >= 9223372036854775808.0 || x < -9223372036854775808.0)
                r = INT64_MIN;                          /* as the machine's conversion does */
            else
                r = (word)x;
            FREES(s + 1, r);
            FREES(s, ((0104 | (sf & 0777000))));
            return s;
        }
        g_fetch(s);
        j = g_regof(s);
        g_addrec(s, KOP, OFIX, j, FREE(s), FREE(s + 1), 0);
        FREES(s, ((0102 | (sf & ~(word)0377))));
    }
    return s;
}

/* DATAST(B): constant words for a data statement */
word
g_datast(word b)
{
    word a, i, j, k;

    a = enstack(dir("0"));
    FREES(a, ((4 | (FREE(a) & ~(word)077))));
    while (g_getlist(b) != 0) {                         /* DS1 */
        if (FREE(b + 2) != 0) {
            imp_error(1, "NON-CONSTANT EXPRESSION IN DATA STATEMENT.");
            continue;
        }
        i = FREE(b);
        if ((i & 077) == 4)
            if (HL(i) == 0) {
                g_addrec(a, KWORD, 0, 0, 0, FREE(b + 1), 0);
                continue;
            }
        i = HL(i);
        if ((j = convc(i)) != 0) {
            if (j < 0) {
                g_addrec(a, KWORD, 0, 0, 0, convc_const, 0);
                continue;
            }
            i = FREE(j);
            k = 1;
            do {
                g_addrec(a, KWORD, 0, 0, 0, FREE(j + k), 0);
                k = k + 1;
            } while (k <= i);
            continue;
        }
        /* a name: its address, a word of the data that the assembler (and the IR writer) relocates: the record has the operand of the name */
        g_unpck(b);
        g_addrec(a, KWORD, 0, 0, g_se, g_se1, 0);
    }
    return a;
}

/* SUBRPAR(S,PP): add the argument PP to the call S */
word
g_subrpar(word s, word pp)
{
    word p;

    p = g_subarg(s, pp);
    g_addrec(s, KOP, OARG, 0, FREE(p), FREE(p + 1), 0);
    return g_hook(s, p, s);
}

/* PRINPAR(N,A,B,C,D): a print list item; the entry it makes has the code that computes the item in word 2 and, in word 1, the records that enter it
 * in the print list.  N is 0 an expression, 1 OCT, 2 IGR, 3 STG, 5 / (new line), 6 FILE, 7 image mode, 8 device, 9 tab, 10 fill, 11 FLT */
word
g_prinpar(word n, word aa, word b, word c, word d)
{
    word a, t, i, j, kk, sh, sa, sv, k;
    word pps[3];

    a = aa;
    t = enstack(0);
    if (n == 5) {
        g_addrec(t, KOP, OPITEM, 0, 0, 0, n);
        goto pp9;
    }
    if (n == 11) {
        if ((FREE(a) & 077) == 4)
            if ((FREE(b) & 077) == 4) {
                FREES(a + 1, ((FREE(a + 1) & G_HM) | g_ls(FREE(b + 1), 32)));
                FREES(a, HR(FREE(a)));
                goto pp1;
            }
        i = g_regof(g_dewop(OLOAD, g_areg1(1, 013), a));
        j = g_regof(g_dewop(OLOAD, g_areg1(1, 013), b));
        g_hook(b, b, g_dewop(OLSH, j, g_stackup(32)));
        g_hook(a, a, b);
        g_dewop(OIOR, i, a);
    pp1:
        ;
    }
    a = g_subarg(t, a);
    g_addrec(t, KOP, OPITEM, 0, FREE(a), FREE(a + 1), n);
    if (n != 6)
        goto pp9;
    pps[0] = b; pps[1] = c; pps[2] = d;
    k = 0;
    do {
        if (pps[k] == 0) {
            g_addrec(t, KOP, OPITEM, 0, 0, 0, n);
            goto pp10;
        }
        sa = g_subarg(t, pps[k]);
        if ((FREE(sa) & 077) == 4) {
            if (k > 0) {                                /* project and programmer: a decimal constant read as octal */
                convc(HL(FREE(sa))); j = convc_const;
                sh = 0; sv = 0;
                while (j != 0) {                        /* PP11 */
                    kk = j / 10;
                    if ((kk = j - (10 * kk)) > 7)
                        imp_error(1, "NON-OCTAL PJ OR PN **");
                    sv = sv | g_ls(kk, sh);                    /* SV:=SV OR KK LS SH, as the original's AE_AE OR KK LS SH */
                    sh = sh + 3;
                    j = j / 10;
                }
                FREES(sa, (4 | G_L32(stcon(sv))));
                FREES(sa + 1, 0);
            }
        }
        g_addrec(t, KOP, OPITEM, 0, FREE(sa), FREE(sa + 1), n);
    pp10:
        if (pps[k] != 0)
            g_hook(a, sa, a);
        k = k + 1;
    } while (k <= 2);
pp9:
    FREES(a + 1, FREE(t + 2));
    FREES(a, 4);                                        /* so the matcher does not read word 2 */
    return a;
}

/* PRINCAL(A,B): add the print list item B to the print call A */
word
g_princal(word a, word b)
{
    word c;

    c = enstack(0);
    FREES(c + 2, FREE(b + 1));
    g_hook(b, b, g_hook(c, a, c));
    FREES(b, FREE(a));
    FREES(b + 1, FREE(a + 1));
    g_subpr0(1);
    return b;
}

word
g_ojumpop(word i)
{
    word val;

    val = 7 & (i + 4);
    if (i >= 8)
        if ((1 & i) != 0)
            val = ((2 & (i + 2)) | (5 & i));
    return val;
}

/* ADDR(S): the original makes S of type 20B, so that what was the value winds up in the address field of the instruction (an immediate
 * operand, a shift by a constant).  An operand of the IR machine can be a constant, so nothing */
word
g_addr(word s)
{
    return s;
}

static word w_dewfun(const word *p) { return g_dewfun(p[0], p[1], p[2]); }
static word w_hook(const word *p) { return g_hook(p[0], p[1], p[2]); }
static word w_freeze(const word *p) { return g_freeze(p[0]); }
static word w_subrcall(const word *p) { return g_subrcall(p[0]); }
static word w_return(const word *p) { return g_return(p[0]); }
static word w_subrpar(const word *p) { return g_subrpar(p[0], p[1]); }
static word w_conop(const word *p) { return g_conop(p[0], p[1], p[2], p[3], p[4]); }
static word w_float(const word *p) { return g_float(p[0]); }
static word w_fix(const word *p) { return g_fix(p[0]); }
static word w_princal(const word *p) { return g_princal(p[0], p[1]); }
static word w_prinpar(const word *p) { return g_prinpar(p[0], p[1], p[2], p[3], p[4]); }
static word w_ojumpop(const word *p) { return g_ojumpop(p[0]); }
static word w_datast(const word *p) { return g_datast(p[0]); }
static word w_addr(const word *p) { return g_addr(p[0]); }
static word w_copy(const word *p) { return g_coppy(p[0]); }
static word w_jop(const word *p) { return g_jop(p[0]); }
static word w_cjump(const word *p) { return g_cjump(p[0], p[1], p[2], p[3]); }
static word w_moveblk(const word *p) { return g_moveblk(p[0], p[1], p[2]); }

/* CODE2I(NIL): the DSEMs of this file */
void
gen_code2_dsems(void)
{
    dsem("DEWFUN", w_dewfun);
    dsem("HOOK", w_hook);
    dsem("FREEZE", w_freeze);
    dsem("SUBRCALL", w_subrcall);
    dsem("RETURN", w_return);
    dsem("SUBRPAR", w_subrpar);
    dsem("CONOP", w_conop);
    dsem("FLOAT", w_float);
    dsem("FIX", w_fix);
    dsem("PRINCAL", w_princal);
    dsem("PRINPAR", w_prinpar);
    dsem("OJUMPOP", w_ojumpop);
    dsem("DATAST", w_datast);
    dsem("ADDR", w_addr);
    dsem("COPY", w_copy);
    dsem("JOP", w_jop);
    dsem("CJUMP", w_cjump);
    dsem("MOVEBLK", w_moveblk);
}
