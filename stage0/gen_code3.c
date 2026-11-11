/*
 * gen_code3.c -- ports src/code3.imc: SUBCALL, SUBSCRIPT, BYTEP, PCODE and PCODE1 (the listing of the code, /A), and CODE3I
 * (the DSEMs of this file).  See gen_code2.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gen.h"

word g_bdesc, g_nbd;                                    /* BDESC, NBD: the byte descriptors, seven words each */
word g_fdesc, g_nfd;                                    /* FDESC, NFD: the descriptors of the fields of structures, eight words each (FLDP) */

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

/* CSUBCALL(A,B): CCALL F(B), a call of a C function with its arguments by value (lib/ffi.imc; not in the original).  The arguments of a declared
 * function (EXTERNAL INT F(PTR);) are counted and converted to the types of the declaration (CPAR); one that is not declared is called as IMP
 * calls a subroutine, by value */
word
g_csubcall(word a, word b)
{
    word d, h, k;

    d = HL(FREE(a));
    h = g_cphead(d);
    if (h == 0)
        return g_subcall(a, b);
    if ((h & 1024) != 0) {
        g_cbnocall(d);
        return g_subcall(a, b);
    }
    a = g_subrcall(g_name(a));
    k = 0;
    while (g_getlist(b) != 0) {
        if (k < 16)
            a = g_cpar(a, b, d, k);
        k = k + 1;
    }
    g_cpcount(d, k);
    g_dewfun(a, 2, g_regof(a));
    g_subpr0(1);
    return a;
}

/* CCALL F(), with no arguments: as the syntax of a call (DEWFUN(SUBRCALL(NAME(A)),2,REGOF(A),SUBPR0(1))), and the count of a declared function checked */
word
g_c0call(word a)
{
    word d, r;

    d = HL(FREE(a));
    if ((g_cphead(d) & 1024) != 0)
        g_cbnocall(d);
    else if (g_cphead(d) != 0)
        g_cpcount(d, 0);
    a = g_subrcall(g_name(a));
    r = g_regof(a);
    g_subpr0(1);
    return g_dewfun(a, 2, r);
}

/* CCALL of a name that is declared a callback: it is an IMP subr, called without CCALL */
word
g_cbnocall(word d)
{
    char cmsg[320];

    snprintf(cmsg, 300, "%s IS A CALLBACK, A SUBR: CALL IT WITHOUT CCALL.", g_irsymname(d));
    imp_error(1, cmsg);
    return 0;
}

/* CALLBACK(NAME): the address of the C function that calls the subr NAME (lib/ffi.imc; not in the original).  It is an ADDR record whose X is 1: DEMIT names the symbol
 * NAME@SIGNATURE, which the writers of LLVM IR define as the function with the C types of the declaration (CALLBACK TYPE NAME(TYPES);) */
word
g_cbaddr(word a)
{
    word d, h, s, r;
    char cmsg[320];

    d = HL(FREE(a));
    h = g_cphead(d);
    if ((h & 1024) == 0) {
        snprintf(cmsg, 300, "%s IS NOT DECLARED A CALLBACK.", g_irsymname(d));
        imp_error(1, cmsg);
    }
    s = g_name(a);
    r = g_areg1(1, 015);
    g_unpck(s);
    g_addrec(s, KOP, OADDR, r, g_se, g_se1, 1);
    FREES(s, (2 | (g_se & 0777700)));
    FREES(s + 1, G_L32(r));
    return s;
}

/* The number K of arguments of a call of the declared function D: at least the parameters of a variadic one, else exactly */
word
g_cpcount(word d, word k)
{
    word h, np;
    char cmsg[320];

    h = g_cphead(d);
    np = (h >> 4) & 31;
    if (k > 16) {
        snprintf(cmsg, 300, "CCALL %s HAS MORE THAN 16 ARGUMENTS.", g_irsymname(d));
        imp_error(1, cmsg);
        return 0;
    }
    if ((h & 512) != 0) {
        if (k < np) {
            snprintf(cmsg, 300, "CCALL %s TAKES AT LEAST %ld ARGUMENTS, NOT %ld.", g_irsymname(d), (long)np, (long)k);
            imp_error(1, cmsg);
        }
        return 0;
    }
    if (k != np) {
        snprintf(cmsg, 300, "CCALL %s TAKES %ld ARGUMENTS, NOT %ld.", g_irsymname(d), (long)np, (long)k);
        imp_error(1, cmsg);
    }
    return 0;
}

/* The argument PP, the K'th of a call S of the declared function D, converted as C would: to the type of the parameter, or, for one of the arguments
 * of a variadic call that have no parameter, to a double for a real and to a long for an integer.  A real is floated for a double or a float and
 * fixed for any other type; a text constant for a PTR is passed by its address (+16 in the type of the record).  The X of the OARG record is the type */
word
g_cpar(word s, word pp, word d, word k)
{
    word t, sf, x, h, p;

    h = g_cphead(d);
    sf = FREE(pp);
    if (k < ((h >> 4) & 31))
        t = g_cptype(d, k);
    else {
        t = 7;
        if ((sf & 0300) == 0200)
            t = 13;
    }
    x = t;
    if (t == 13 || t == 14)
        g_float(pp);
    else if (t == 12 && (sf & 077) == 4 && HL(sf) != 0 && g_irkind(HL(sf)) == 1)
        x = t + 16;
    else if ((sf & 0300) == 0200)
        g_fix(pp);
    p = g_subarg(s, pp);
    g_addrec(s, KOP, OARG, 0, FREE(p), FREE(p + 1), x);
    return g_hook(s, p, s);
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

/* ---- Layouts of C structures (STRUCT of lib/ffi.imc; not in the original) ----------------------------------------------------------------------------------
 * STT holds the structures of the unit, five words each: the name (a directory index), the size, the alignment, the index in SFT of the first field and the
 * number of fields.  SFT holds the fields, five words each: the name, the type, the count (0 for a field that is not an array), the offset from the start of the
 * structure, 0.  A type is 1 to 14, the number of a type of C (the types of the typed declarations), or 64 plus the number of a structure (its index in STT
 * divided by 5).  The layout is C's for the machine the program is compiled for: the sizes of long and of pointers are those of the word (4 where the pointers are
 * 32 bits), and the 8 byte types of the i386 align to 4.  STT and SFT are emptied when the unit ends (ASSEMC, STRESET). */
static word stt, nstt, sft, nsft;
static word stcur = -1, stoff, stal;                    /* the structure being declared: its index in STT, the offset of the next field, the greatest alignment */

/* 1 if the program is compiled for a machine whose pointers and longs are 32 bits: the generator's own target (-T i386, arm) or the host's, the seed having no other */
int
g_stp32(void)
{
    return irtgt->wordll != 0 || sizeof(void *) == 4;
}

/* 1 for the arm, whose 8 byte types align to 8 in a structure where the i386's align to 4 */
static int
starm(void)
{
    if (irtgt->wordll != 0)
        return irtgt->pushargs;
#ifdef __arm__
    return 1;
#else
    return 0;
#endif
}

/* the size in bytes of the type T */
word
g_tysize(word t)
{
    word p32 = g_stp32();

    switch (t) {
    case 1: case 2: return 1;
    case 3: case 4: return 2;
    case 5: case 6: case 14: return 4;
    case 7: case 8: case 11: case 12: return p32 ? 4 : 8;
    case 9: case 10: case 13: return 8;
    default: break;
    }
    if (t >= 64)
        return FREE(stt + 5 * (t - 64) + 1);
    return 0;
}

/* the alignment of the type T: its size, except that the i386 aligns the 8 byte types of a structure to 4 */
word
g_tyalign(word t)
{
    word n;

    if (t >= 64)
        return FREE(stt + 5 * (t - 64) + 2);
    n = g_tysize(t);
    if (n == 8 && g_stp32() != 0 && starm() == 0)
        n = 4;
    return n;
}

/* the index in STT of the structure named D, or -1 */
static word
stfind(word d)
{
    word i;

    for (i = 0; i < nstt; i += 5)
        if (FREE(stt + i) == d)
            return i;
    return -1;
}

static void
sterr(const char *f, const char *a, const char *b)
{
    char cmsg[320];

    snprintf(cmsg, 300, f, a, b);
    imp_error(1, cmsg);
}

/* the end of the unit */
word
g_streset(word z)
{
    (void)z;
    nstt = 0;
    nsft = 0;
    stcur = -1;
    return 0;
}

/* STRUCT N ( : begins the structure N */
word
g_stbeg(word n)
{
    word d = HL(FREE(n));

    if (stcur >= 0)
        g_stend(0);
    if (stfind(d) >= 0 || g_ctypeof(d) != 0) {
        sterr("%s IS DECLARED TWICE AS A STRUCTURE OR IS A C TYPE.", g_irsymname(d), "");
        return enstack(0);
    }
    if (stt == 0)
        frelot(&stt, "STT", 25, NULL);
    if (sft == 0)
        frelot(&sft, "SFT", 25, NULL);
    stcur = nstt;
    fadd(&stt, &nstt, d); fadd(&stt, &nstt, 0); fadd(&stt, &nstt, 1); fadd(&stt, &nstt, nsft); fadd(&stt, &nstt, 0);
    stoff = 0;
    stal = 1;
    return enstack(0);
}

/* T F or T F [ N ]: a field of the structure being declared, of the type T (a type of C or a structure declared before), an array of N of them if N is not 0 */
word
g_stfld(word t, word f, word n)
{
    word td, fd, ty, cnt, k, i, sz, al;

    if (stcur < 0)
        return enstack(0);
    td = HL(FREE(t));
    fd = HL(FREE(f));
    ty = g_ctypeof(td);
    if (ty == 15) {
        sterr("%s IS NOT A TYPE OF A FIELD.", g_irsymname(td), "");
        ty = 5;
    } else if (ty == 0) {
        k = stfind(td);
        if (k >= 0 && k != stcur)
            ty = 64 + k / 5;
        else {
            sterr("%s IS NOT A C TYPE OR A STRUCTURE DECLARED BEFORE.", g_irsymname(td), "");
            ty = 5;
        }
    }
    cnt = 0;
    if (n != 0) {
        if ((FREE(n) & 077) == 4 && FREE(n + 1) > 0 && FREE(n + 1) < 16777216)
            cnt = FREE(n + 1);
        else {
            sterr("THE LENGTH OF THE ARRAY FIELD %s IS NOT A POSITIVE CONSTANT.", g_irsymname(fd), "");
            cnt = 1;
        }
    }
    for (i = FREE(stt + stcur + 3); i < nsft; i += 5)
        if (FREE(sft + i) == fd)
            sterr("THE FIELD %s IS DECLARED TWICE.", g_irsymname(fd), "");
    sz = g_tysize(ty);
    al = g_tyalign(ty);
    if (al < 1)
        al = 1;
    stoff = (stoff + al - 1) / al * al;
    fadd(&sft, &nsft, fd); fadd(&sft, &nsft, ty); fadd(&sft, &nsft, cnt); fadd(&sft, &nsft, stoff); fadd(&sft, &nsft, 0);
    FREES(stt + stcur + 4, FREE(stt + stcur + 4) + 1);
    stoff = stoff + sz * (cnt != 0 ? cnt : 1);
    if (al > stal)
        stal = al;
    return enstack(0);
}

/* ) : ends the structure: its size is the end of the last field rounded up to the greatest alignment */
word
g_stend(word h)
{
    (void)h;
    if (stcur < 0)
        return enstack(0);
    if (FREE(stt + stcur + 4) == 0)
        sterr("THE STRUCTURE %s HAS NO FIELD.", g_irsymname(FREE(stt + stcur)), "");
    FREES(stt + stcur + 1, (stoff + stal - 1) / stal * stal);
    FREES(stt + stcur + 2, stal);
    stcur = -1;
    return enstack(0);
}

/* the type of the name D, a type of C or a structure (for SIZEOF), or 0 */
static word
sttype(word d)
{
    word ty, k;

    ty = g_ctypeof(d);
    if (ty == 15)
        return 0;
    if (ty != 0)
        return ty;
    k = stfind(d);
    if (k >= 0 && k != stcur)
        return 64 + k / 5;
    return 0;
}

/* a constant of the semantics stack */
static word
stconst(word v)
{
    word e = enstack(0);

    FREES(e, 0104);
    FREES(e + 1, v);
    return e;
}

/* SIZEOF(N): the size in bytes of the type of C or the structure N, a constant */
word
g_stsize(word s)
{
    word d = HL(FREE(s)), ty;

    ty = sttype(d);
    if (ty == 0) {
        sterr("%s IS NOT A C TYPE OR A STRUCTURE.", g_irsymname(d), "");
        return stconst(0);
    }
    return stconst(g_tysize(ty));
}

/* A path S.F or S,F: the entry holds, in the word 0, 1 and the type (from bit 8, 16 bits) and the count of the array (from bit 24) of the field the path ends at, and in
 * the word 1 its offset.  Type 0 is a path that failed (the error is said once) */
#define FPTY(p)  ((word)(((uword)FREE(p) >> 8) & 0xFFFF))
#define FPCNT(p) ((word)((uword)FREE(p) >> 24))

word
g_fpbeg(word s, word f)
{
    word d = HL(FREE(s)), k, e;

    e = enstack(0);
    k = stfind(d);
    if (k < 0 || k == stcur) {
        sterr("%s IS NOT A STRUCTURE.", g_irsymname(d), "");
        FREES(e, 1);
        FREES(e + 1, 0);
        return e;
    }
    FREES(e, 1 | ((word)(64 + k / 5) << 8));
    FREES(e + 1, 0);
    return g_fpfld(e, f);
}

word
g_fpfld(word p, word f)
{
    word fd = HL(FREE(f)), ty = FPTY(p), k, i, n;

    if (ty == 0)
        return p;
    if (ty < 64 || FPCNT(p) != 0) {
        sterr("%s IS NOT A FIELD OF A STRUCTURE TYPE (FOR AN ARRAY OF STRUCTURES ADD THE ELEMENT'S OFFSET TO THE ADDRESS).", g_irsymname(fd), "");
        FREES(p, 1);
        return p;
    }
    k = 5 * (ty - 64);
    n = FREE(stt + k + 4);
    for (i = FREE(stt + k + 3); i < FREE(stt + k + 3) + 5 * n; i += 5)
        if (FREE(sft + i) == fd) {
            FREES(p, 1 | (FREE(sft + i + 1) << 8) | (FREE(sft + i + 2) << 24));
            FREES(p + 1, FREE(p + 1) + FREE(sft + i + 3));
            return p;
        }
    sterr("THE STRUCTURE %s HAS NO FIELD %s.", g_irsymname(FREE(stt + k)), g_irsymname(fd));
    FREES(p, 1);
    return p;
}

/* OFFSETOF(S,F): the offset of the field in bytes, a constant */
word
g_stoff(word p)
{
    return stconst(FREE(p + 1));
}

/* FIELD(P,S.F) and FIELD(P,S.F,I): the field F of the structure S at the address P (the Ith element if the field is an array).  The entry is a variable named for a
 * descriptor in FDESC (eight words: the name, the address P as an operand of two words, the index I as an operand of two words or 0, the offset of the field plus the
 * offset of the element if I is a constant, the type, the size of an element).  The operations LDB and DPB on it load and store the field with its type (DEMIT) */
word
g_fldp(word p, word fp, word idx)
{
    word td, e, ty, cnt, off, esz, i0, i1, ci;

    ty = FPTY(fp);
    cnt = FPCNT(fp);
    off = FREE(fp + 1);
    if (ty == 0)
        ty = 5;
    else if (ty >= 64) {
        imp_error(1, "THE FIELD IS OF A STRUCTURE TYPE: NAME A FIELD OF IT (FIELD(P, S.F.G)).");
        ty = 5;
    }
    ci = 0;
    if (cnt != 0 && idx == 0) {
        imp_error(1, "THE FIELD IS AN ARRAY: GIVE THE ELEMENT (FIELD(P, S.F, I)).");
        cnt = 0;
    } else if (cnt == 0 && idx != 0) {
        imp_error(1, "THE FIELD IS NOT AN ARRAY, SO IT TAKES NO ELEMENT.");
        idx = 0;
    }
    esz = g_tysize(ty);
    i0 = 0; i1 = 0;
    if (idx != 0) {
        if ((FREE(idx) & 077) == 4) {
            off = off + FREE(idx + 1) * esz;
            ci = 1;
        } else {
            g_fetch(idx);
            i0 = FREE(idx); i1 = FREE(idx + 1);
        }
    }
    td = newname("FLD");
    e = g_name(enstack(td));
    if ((FREE(p) & 077) != 4)
        g_fetch(p);
    if (g_fdesc == 0)
        frelot(&g_fdesc, "FDESC", 80, NULL);
    fadd(&g_fdesc, &g_nfd, td);
    fadd(&g_fdesc, &g_nfd, FREE(p)); fadd(&g_fdesc, &g_nfd, FREE(p + 1));
    fadd(&g_fdesc, &g_nfd, i0); fadd(&g_fdesc, &g_nfd, i1);
    fadd(&g_fdesc, &g_nfd, off);
    fadd(&g_fdesc, &g_nfd, ty);
    fadd(&g_fdesc, &g_nfd, esz);
    if (idx != 0 && ci == 0)
        g_hook(p, p, idx);
    g_hook(e, p, e);
    FREES(e + 1, 0);
    FREES(e, ((ty == 13 || ty == 14 ? 0210 : 0110) | G_L32(td)));
    return e;
}

/* FIELD(...) := B: the value is converted to the type of the field (a real for DOUBLE and FLOAT, an integer otherwise) and deposited */
word
g_fldst(word a, word b)
{
    if ((FREE(a) & 0300) == 0200)
        g_float(b);
    else
        g_fix(b);
    return g_addop(ODPB, a, b);
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
    "CALLB  ", "ARG    ", "CALLE  ",
    "PITEM  ", "?      ", "?      ",
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
static word w_csubcall(const word *p) { return g_csubcall(p[0], p[1]); }
static word w_c0call(const word *p) { return g_c0call(p[0]); }
static word w_cbaddr(const word *p) { return g_cbaddr(p[0]); }
static word w_bytep(const word *p) { return g_bytep(p[0], p[1], p[2], p[3]); }
static word w_stbeg(const word *p) { return g_stbeg(p[0]); }
static word w_stfld(const word *p) { return g_stfld(p[0], p[1], p[2]); }
static word w_stend(const word *p) { return g_stend(p[0]); }
static word w_stsize(const word *p) { return g_stsize(p[0]); }
static word w_fpbeg(const word *p) { return g_fpbeg(p[0], p[1]); }
static word w_fpfld(const word *p) { return g_fpfld(p[0], p[1]); }
static word w_stoff(const word *p) { return g_stoff(p[0]); }
static word w_fldp(const word *p) { return g_fldp(p[0], p[1], p[2]); }
static word w_fldst(const word *p) { return g_fldst(p[0], p[1]); }

/* CODE3I(NIL): the DSEMs of this file */
void
gen_code3_dsems(void)
{
    dsem("SUBSCRIPT", w_subscript);
    dsem("SUBCALL", w_subcall);
    dsem("CSUBCALL", w_csubcall);
    dsem("CCALL0", w_c0call);
    dsem("CBADDR", w_cbaddr);
    dsem("BYTEP", w_bytep);
    dsem("STBEG", w_stbeg);
    dsem("STFLD", w_stfld);
    dsem("STEND", w_stend);
    dsem("STSIZE", w_stsize);
    dsem("FPBEG", w_fpbeg);
    dsem("FPFLD", w_fpfld);
    dsem("STOFF", w_stoff);
    dsem("FLDP", w_fldp);
    dsem("FLDST", w_fldst);
}
