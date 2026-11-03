/*
 * gen_code3.c -- ports src/llvm-ir/code3.imp: SUBCALL, SUBSCRIPT, BYTEP, PCODE and PCODE1 (the listing of the code, /A), and CODE3I
 * (the DSEMs of this file).  See gen_code2.c.
 */
#include <stdlib.h>
#include <string.h>
#include "gen.h"

word g_bdesc, g_nbd;                                    /* BDESC, NBD: the byte descriptors, seven words each */

word
g_subcall(word a, word b)
{
    a = g_subrcall(g_name(a));
    while (g_getlist(b) != 0)
        a = g_subrpar(a, b);
    g_dewfun(a, 2, g_regof(a));
    g_subpr0(1);
    return a;
}

/* SUBSCRIPT(S,T): the subscripted variable S[T] (S=0: [T], the word at address T) */
word
g_subscript(word s, word t)
{
    word tty, sty, te, sc, i, see, r0, k;

    tty = FREE(t) & 077;
    if (tty == 2)                                       /* (a fixed register, the value of a call: copied into a fresh one, which leaves the name of the routine behind; the tape's GO TO L46) */
        if (g_reg0(g_regof(t)) != 0) {
            g_dewop(OLOAD, g_areg1(1, 015), t);
            tty = 2;
        }
    if (tty == 020)
        if (FREE(t + 1) < 0) {
            g_dewop(OLOAD, g_areg1(1, 015), t);
            tty = 2;
        }
    if (s == 0) {
        if (tty != 2)
            if (tty != 4) {
                if (FREE(t + 1) < 0) {
                    g_dewop(OLOAD, g_areg1(1, 015), t);
                    tty = 020;
                    FREES(t, ((020 | (FREE(t) & 077))));
                } else
                    FREES(t + 1, (FREE(t + 1) | g_ls(1, 63)));
            }
        te = 020 | (FREE(t) & ~(word)077);
        if (tty == 4) {
            te = 010 | (te & 0777700);
            FREES(t + 1, HR(FREE(t + 1)));
        }
        FREES(t, te);
        return t;
    }
    if (tty != 2)
        if (tty != 4) {
            g_dewop(OLOAD, g_areg1(1, 015), t);
            tty = 2;
        }
    sty = FREE(s) & 077;
    if (sty == 020)
        if (FREE(s + 1) < 0) {
            g_dewop(OADDR, g_areg1(1, 015), s);
            sty = 020;
            FREES(s, ((020 | (FREE(s) & 0700))));
        }
    if (sty == 2) {
        imp_error(1, "SUBSCRIPTED REGISTER - IGNORED. ");
        return s;
    }
    if (sty == 4) {
        see = FREE(s) & ~(word)077;
        if (HL(see) == 0) {
            imp_error(2, "CALCULATED CONSTANT IS SUBSCRIPTED. ");
            i = stcon(FREE(s + 1));
            see = (G_L32(i) | HR(see));
        }
        FREES(s, ((see | 010)));
        FREES(s + 1, 0);
    }
    if (tty == 2) {
        if (sty != 020) {
            FREES(s, ((020 | (FREE(s) & ~(word)077))));
            FREES(s + 1, (FREE(s + 1) | G_L32(g_regof(t))));
        } else {
            r0 = g_regof(s);
            if (r0 == 0)
                FREES(s + 1, (FREE(s + 1) | G_L32(g_regof(t))));
            else {
                /* add the index to a copy of the register: it may be a parameter's or a user's */
                k = g_areg1(1, 015);
                g_addrec(s, KOP, OLOAD, k, 2, G_L32(r0), 0);
                FREES(s + 1, g_regfield(FREE(s + 1), r0, k));
                if (HL(FREE(s)) == 0)                   /* a pointer in a register: the index is in words */
                    g_addrec(s, KOP, OADDX, k, FREE(t), FREE(t + 1), 0);
                else
                    g_addrec(s, KOP, OADD, k, FREE(t), FREE(t + 1), 0);
            }
        }
    }
    if (tty == 4) {
        sc = HR(HR(FREE(s + 1)) + FREE(t + 1));
        FREES(s + 1, (sc | (FREE(s + 1) & ~G_HM)));     /* the register and the indirect bit kept */
    }
    return g_hook(s, t, s);
}

/* BYTEP(A,S,P,FLAG): the byte A<S,P> (S bits with the low bit at bit P).  The original builds a PDP-10 byte pointer as constant code; here the
 * entry is a variable named for a descriptor kept in the table BDESC (seven words: the name, the operand A, the size S and the position P, two
 * words of an entry each, a register or a constant).  The operations load byte, deposit byte and move (the pointer's value) read it in DEMIT */
word
g_bytep(word a, word s, word p, word flag)
{
    word td;

    (void)flag;
    td = newname("BYTE");
    g_name(enstack(td));
    if ((FREE(p) & 077) != 4)
        g_fetch(p);
    if ((FREE(s) & 077) != 4)
        g_fetch(s);
    if (g_bdesc == 0)
        frelot(&g_bdesc, "BDESC", 70, NULL);
    fadd(&g_bdesc, &g_nbd, td);
    fadd(&g_bdesc, &g_nbd, FREE(a)); fadd(&g_bdesc, &g_nbd, FREE(a + 1));
    fadd(&g_bdesc, &g_nbd, FREE(s)); fadd(&g_bdesc, &g_nbd, FREE(s + 1));
    fadd(&g_bdesc, &g_nbd, FREE(p)); fadd(&g_bdesc, &g_nbd, FREE(p + 1));
    g_hook(a, g_hook(p, p, s), a);
    FREES(a + 1, 0);
    FREES(a, (0110 | G_L32(td)));
    return a;
}

/* the names of the operations, one to a word (a text constant of seven characters and its NUL).  The table is the IMP's, which is not
 * the table of the numbers of irsyn.imc after 36 (CALLB is at 62 here and OCALLB is 60): a listing's names are wrong from there */
static const char *const opn[] = {
    "?      ", "LOAD   ", "NEG    ",
    "COMPL  ", "ADDR   ", "STORE  ",
    "ADD    ", "SUB    ", "MUL    ",
    "DIV    ", "MOD    ", "AND    ",
    "ANDCA  ", "ANDCM  ", "ANDCB  ",
    "IOR    ", "ORCA   ", "ORCM   ",
    "ORCB   ", "XOR    ", "EQV    ",
    "LSH    ", "ASH    ", "ROT    ",
    "?      ", "?      ", "?      ",
    "?      ", "?      ", "?      ",
    "FADD   ", "FSUB   ", "FMUL   ",
    "FDIV   ", "FLOAT  ", "FIX    ",
    "FNEG   ", "?      ", "?      ",
    "?      ", "JUMP   ", "JEQ    ",
    "JLT    ", "JLE    ", "JGE    ",
    "JGT    ", "JNE    ", "?      ",
    "?      ", "?      ", "?      ",
    "FJEQ   ", "FJLT   ", "FJLE   ",
    "FJGE   ", "FJGT   ", "FJNE   ",
    "?      ", "?      ", "?      ",
    "?      ", "?      ", "CALLB  ",
    "ARG    ", "CALLE  ", "PITEM  ",
    "?      ", "?      ", "?      ",
    "?      ", "ENTRY  ", "RET    "
};

/* PCODE(S,NS): list the code of the entry S, up to NS lines (NS=0: 15).  The original lists the PDP-10 instructions of the chain; here a
 * record of the chain is an operation of the IR: its number, the operation, the register, the two fields (octal) and the extra word */
word
g_pcode(word s, word ns)
{
    return g_pcode1(s, ns);
}

word
g_pcode1(word s, word ns)
{
    word r, h, op, kd, n, k;
    FILE *f = imp_out();

    if ((n = ns) == 0)
        n = 15;
    r = g_rs(FREE(s + 2), 32); k = 0;
    for (;;) {                                          /* L3 */
        if (r == 0)
            return 0;
        if (k >= n)
            return 0;
        h = FREE(g_co + r); op = g_rs(h, 8) & 0377; kd = h & 0377;
        fprintf(f, "%6lld  ", (long long)r);
        if (kd == 2) {
            fprintf(f, "TAG    ");
            pname(FREE(g_co + r + 4));
            fprintf(f, "\n");
        } else if (kd == 3) {
            fprintf(f, "WORD   %lld\n", (long long)FREE(g_co + r + 3));
        } else {
            if (op < 72 && opn[op][0] != '?')
                fprintf(f, "%.6s", opn[op]);
            else {
                fprintf(f, "%lld", (long long)op);
                fprintf(f, "%.*s", op < 10 ? 5 : (op < 100 ? 4 : 3), "      ");
            }
            fprintf(f, " R%lld  V %llo,%llo  X %lld\n", (long long)FREE(g_co + r + 1),
                (unsigned long long)(FREE(g_co + r + 2) & 0777777777777L),
                (unsigned long long)(FREE(g_co + r + 3) & 0777777777777L), (long long)FREE(g_co + r + 4));
        }
        r = g_rs(h, 32); k = k + 1;
    }
}

static word w_subscript(const word *p) { return g_subscript(p[0], p[1]); }
static word w_subcall(const word *p) { return g_subcall(p[0], p[1]); }
static word w_bytep(const word *p) { return g_bytep(p[0], p[1], p[2], p[3]); }

/* CODE3I(NIL): the DSEMs of this file */
void
gen_code3_dsems(void)
{
    dsem("SUBSCRIPT", w_subscript);
    dsem("SUBCALL", w_subcall);
    dsem("BYTEP", w_bytep);
}
