/*
 * gen_demit.c -- ports src/llvm-ir/demit.imp: it takes the place of the assembler, the register allocator and the output module of the
 * original (AMOD, RMOD, OMOD): it reads the chains of records the code generator made and writes them as IR, through the builders of
 * stage0/ir.c (the IMP calls the translation of those, src/llvm-ir/ix.imp).  LLVM does the register allocation, the instruction selection and
 * the object file.  A register index is a temporary of the IR; a program is the function impmain and each subroutine one function; the arguments
 * of a call are pointers to the values (IMP passes by reference).
 *
 * The IRSYMNAME, IRKIND, IRSTRVAL, IRLABNAME, IRPOOLEMIT, IRFNKIND, IRRUNTIME, IRTAGERR, IRJBNAME and IRVARREC of ix.imp (the names of the
 * directory entries as the assembler and the IR have them) are here too.
 *
 * The order of evaluation is that of IMP, which matters here because a register's first use takes the next temporary and a shift writes
 * statements: the arguments of a call from the right to the left (ASG(RV(RD),VEXPR(V0,V1)) runs VEXPR first).  Each place with two arguments
 * that have effects is written as the statements in that order.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gen.h"

char **g_symc;                                  /* SYMC, NSYMC: the cache of the assembler names by directory index */
word g_nsymc;

#define TLONG T_LONG
#define TPLONG T_PLONG
#define TPFTN T_PFTN
#define TPUCHAR (T_PTR | T_UCHAR)

static word dinit;                              /* DINIT */
static word rtmp;                               /* the temporary of each register index (0: none yet) */
static word tags, ntags;                        /* pairs: tag (directory index), label */
static word usedv, nused;                       /* the variables the unit uses */
static word pools, npool;                       /* pairs: pooled constant (directory index), label */
static word fnames, nfn;                        /* the functions of the unit */
static word rlist, nrl, rlist2, nrl2;           /* the records of the unit, and of its remote code */
static word xtags, nxt;                         /* the tags jumped to from outside their routine */
static word tble, ntble;                        /* the labels (index of record, label) of the ends of the tables of computed GO TOs */
static word ddl, nddl, ddw, nddw;               /* data: triples (tag, first word, count) and the words */
static word callfn, nargs, callmode, retlab, rettmp, shused, cpflag;
static struct irn *args[16];                    /* ARGS, PARGS */
static word atys[16];                           /* ATYS: the C type of each argument (the X of its record) */
static word cwused;                             /* CWUSED: the words imp_cw0 to imp_cw(CWUSED-1) the unit uses */
static struct irn *pargs[2];
static struct irn *plast;                       /* PLAST: the call of a print list item, written when the next one starts */

/* ---- the names of the directory entries (ix.imp: IRNAMEOF ... IRVARREC) --------------------------------------------------- */

static char sbuf[8 * 52 + 8];                   /* SBUF: the whole name, a C string */

/* IRNAMEOF(V): the whole name of entry V, a C string in SBUF; its length (the IMP reads the bytes of the words in memory, the first character in the
 * low byte) */
static word
irnameof(word v)
{
    word k, j, w;

    k = 0;
    do {
        w = dname(v, k);
        for (j = 0; j < 8; j++)
            sbuf[8 * k + j] = (char)g_chr(w, (int)j);
        for (j = 0; j < 8; j++)
            if (g_chr(w, (int)j) == 0)
                return (8 * k) + j;
        k = k + 1;
    } while (k < 48);
    sbuf[8 * k] = '\0';
    return 8 * k;
}

/* The assembler name of directory entry V: the name as it is written (IMP names are case sensitive, and C's are what C says), characters an
 * assembler would not take as _XX (two hexadecimal digits) */
const char *
g_irsymname(word v)
{
    word l, i, h, n;
    char *out, *p;
    int c;

    if (v >= g_nsymc) {
        n = v + 64;
        g_symc = realloc(g_symc, 8 * (size_t)n);
        memset(g_symc + g_nsymc, 0, 8 * (size_t)(n - g_nsymc));
        g_nsymc = n;
    }
    if (g_symc[v] != NULL)
        return g_symc[v];
    l = irnameof(v);
    out = malloc((3 * (size_t)l) + 4);
    p = out;
    for (i = 0; i < l; i++) {
        c = (unsigned char)sbuf[i];
        if ((c >= 0101 && c <= 0132) || (c >= 0141 && c <= 0172) || (c >= 060 && c <= 071) || c == 0137) {
            *p++ = (char)c;
            continue;
        }
        *p++ = (char)0137;
        h = c / 16; *p++ = (char)(h < 10 ? 060 + h : 0127 + h);
        h = c - (16 * (c / 16)); *p++ = (char)(h < 10 ? 060 + h : 0127 + h);
    }
    *p = '\0';
    g_symc[v] = out;
    return out;
}

/* 0 a variable or function, 1 a text constant, 2 a number */
word
g_irkind(word v)
{
    int c;

    if (v == 0)
        return 0;
    irnameof(v);
    c = (unsigned char)sbuf[0];
    if (c == 047)
        return 1;
    if (c >= 060 && c <= 071)
        if (convc(v) == 0 - 1)
            return 2;
    return 0;
}

/* The value of a text constant: its first 8 characters, the first at the first address, so that the word is the start of the string in memory */
word
g_irstrval(word v)
{
    word l, i;
    unsigned char wb[8];
    word r;

    l = irnameof(v);
    memset(wb, 0, sizeof wb);
    i = 1;
    while (i < 9 && i < l) {
        wb[i - 1] = (unsigned char)sbuf[i];
        i = i + 1;
    }
    memcpy(&r, wb, 8);
    return r;
}

/* the label .LN as a C string */
const char *
g_irlabname(word l)
{
    char *s;

    s = malloc(24);
    snprintf(s, 24, ".L%ld", (long)l);
    return s;
}

/* The body of a pooled constant: the text, or the number */
word
g_irpoolemit(word v)
{
    word l, i;
    int c;
    char obuf[8 * 200];
    char *p;

    if (g_irkind(v) != 1) {
        convc(v);
        ir_pass(" .quad %lld", (long long)convc_const);
        return 0;
    }
    l = irnameof(v);
    p = obuf;
    i = 1;
    while (i < l) {
        c = (unsigned char)sbuf[i];
        i = i + 1;
        if (c == 042 || c == 0134) {
            *p++ = (char)0134;
            *p++ = (char)c;
            continue;
        }
        if (c >= 040 && c < 0177) {
            *p++ = (char)c;
            continue;
        }
        *p++ = (char)0134;
        *p++ = (char)(060 + (c / 64));
        *p++ = (char)(060 + ((c / 8) - (8 * (c / 64))));
        *p++ = (char)(060 + (c - (8 * (c / 8))));
    }
    *p = '\0';
    ir_pass(" .string \"%s\"", obuf);
    ir_pass(" .balign 8");
    return 0;
}

/* 1 for the print routine, 2 for the read routine, else 0 */
word
g_irfnkind(word v)
{
    const char *n;

    n = g_irsymname(v);
    if (strcmp(n, "PRN_2e") == 0)
        return 1;
    if (strcmp(n, "RED_2e") == 0)
        return 2;
    return 0;
}

/* The runtime library is IMP: its routines take their arguments by reference and return a word, like any other, so no name is a routine that returns a
 * C long but the ones called with CCALL */
word
g_irruntime(word v)
{
    (void)v;
    return 0;
}

/* The error for a GO TO a tag that is not in the subroutine, with the tag's name */
word
g_irtagerr(word d)
{
    char mbuf[8 * 24 + 160];

    snprintf(mbuf, 160, "GO TO %s, WHICH IS NOT IN THE SUBROUTINE.", g_irsymname(d));
    imp_error(1, mbuf);
    return 0;
}

/* The name of the jump buffer of tag D: a GO TO from another routine is a longjmp to it (see EXPORTTAGS) */
const char *
g_irjbname(word d)
{
    const char *n;
    char *s;

    n = g_irsymname(d);
    s = malloc(strlen(n) + 8);
    snprintf(s, strlen(n) + 8, "imp_jb_%s", n);
    return s;
}

/* where a variable of one of the compiler's own modules is (IMP72MODULE) */
word
g_irvarrec(const char *sym, word sz)
{
    ir_varrec(sym, (long)sz);
    return 0;
}

/* ---- the trees --------------------------------------------------------------------------------------------------------- */

static struct irn *vexpr(word v0, word v1);
static struct irn *eaddr(word v0, word v1);

/* RV(R): the temporary of register index R */
static struct irn *
rv(word r)
{
    word t;

    if (flen(rtmp) <= r)
        faddex(&rtmp, r + 64);
    t = FREE(rtmp + r);
    if (t == 0) {
        t = ir_newtemp();
        FREES(rtmp + r, t);
    }
    return ir_temp((int)t, TLONG);
}

static struct irn *
icn(word v)
{
    return ir_icon(v, TLONG);
}

static void
asg(struct irn *l, struct irn *r)
{
    ir_stmt(ir_assign(l, r));
}

/* the offset field of an entry's word 1: 32 bits, signed */
static word
offset(word w)
{
    return (HR(w) ^ 020000000000L) - 020000000000L;
}

static word
taglabel(word d)
{
    word i, l;

    if (ntags > 0) {
        i = 0;
        do {
            if (FREE(tags + i) == d)
                return FREE((tags + i) + 1);
            i = i + 2;
        } while (i <= ntags - 2);
    }
    l = ir_newlabel();
    fadd(&tags, &ntags, d);
    fadd(&tags, &ntags, l);
    return l;
}

/* note that variable D is used by the unit */
static void
usevar(word d)
{
    word i;

    if (nused > 0) {
        i = 0;
        do {
            if (FREE(usedv + i) == d)
                return;
            i = i + 1;
        } while (i <= nused - 1);
    }
    fadd(&usedv, &nused, d);
}

/* the label of the pooled constant named D (a text or a number used as an address) */
static word
poolof(word d)
{
    word i, l;

    if (npool > 0) {
        i = 0;
        do {
            if (FREE(pools + i) == d)
                return FREE((pools + i) + 1);
            i = i + 2;
        } while (i <= npool - 2);
    }
    l = ir_newlabel();
    fadd(&pools, &npool, d);
    fadd(&pools, &npool, l);
    return l;
}

/* the address of a directory entry D, plus O words, as a pointer */
static struct irn *
symaddr(word d, word o)
{
    word k;

    k = g_irkind(d);
    if (k == 0) {
        usevar(d);
        return ir_sym(g_irsymname(d), 8 * o, TPLONG);
    }
    return ir_sym(g_irlabname(poolof(d)), 8 * o, TPLONG);
}

/* The address of the operand (V0,V1) as a pointer expression.  A variable is its byte address; the offset and the index register count words when
 * there is a variable (V[I] is at &V+8*I); a word with no variable is a pointer in the register, plus the offset in words, or an absolute address */
static struct irn *
eaddr(word v0, word v1)
{
    word d, r, o;
    struct irn *e, *x1, *x2, *x3;

    d = HL(v0);
    if ((v0 & 077) == 4) {                              /* a named constant: the address of its word */
        if (d == 0)
            imp_error(1, "THE ADDRESS OF A COMPUTED CONSTANT.");
        return symaddr(d, 0);
    }
    r = 03777 & HL(v1); o = offset(v1);
    if (d != 0) {
        e = symaddr(d, o);
        if (r != 0) {
            x1 = rv(r); x2 = ir_icon(3, T_INT); x3 = ir_bin(IR_LS, TLONG, x1, x2);
            e = ir_bin(IR_PLUS, TPLONG, e, x3);
        }
    } else {
        if (r != 0) {
            x1 = ir_icon(8 * o, TPLONG); x2 = rv(r); e = ir_bin(IR_PLUS, TPLONG, x1, x2);
        } else
            e = ir_icon(o, TPLONG);
    }
    if (v1 < 0) {                                       /* indirect: the word is a pointer */
        x1 = ir_deref(e, TLONG); e = ir_un(IR_PCONV, TPLONG, x1);
    }
    return e;
}

static word bdfind(word d);
static struct irn *bpvalue(word j);

/* The value of the operand (V0,V1) */
static struct irn *
vexpr(word v0, word v1)
{
    word k, d, r, o;

    k = v0 & 077; d = HL(v0);
    if (k == 2)
        return rv(03777 & HL(v1));
    if (k == 4) {
        if (d != 0)
            if (g_irkind(d) == 1)
                return ir_icon(g_irstrval(d), TLONG);
        return icn(v1);
    }
    if (d != 0)
        if (k == 010)
            if (bdfind(d) >= 0)
                return bpvalue(bdfind(d));
    r = 03777 & HL(v1); o = offset(v1);
    if (d != 0)
        if (r == 0)
            if (v1 >= 0)
                if (g_irkind(d) == 0) {
                    usevar(d);
                    return ir_name(g_irsymname(d), 8 * o, TLONG);
                }
    return ir_deref(eaddr(v0, v1), TLONG);
}

static struct irn *
boolop(word op, struct irn *a, struct irn *b)
{
    if (op == OAND)
        return ir_bin(IR_AND, TLONG, a, b);
    if (op == OANDCA)
        return ir_bin(IR_AND, TLONG, ir_un(IR_COMPL, TLONG, a), b);
    if (op == OANDCM)
        return ir_bin(IR_AND, TLONG, a, ir_un(IR_COMPL, TLONG, b));
    if (op == OANDCB)
        return ir_bin(IR_AND, TLONG, ir_un(IR_COMPL, TLONG, a), ir_un(IR_COMPL, TLONG, b));
    if (op == OIOR)
        return ir_bin(IR_OR, TLONG, a, b);
    if (op == OORCA)
        return ir_bin(IR_OR, TLONG, ir_un(IR_COMPL, TLONG, a), b);
    if (op == OORCM)
        return ir_bin(IR_OR, TLONG, a, ir_un(IR_COMPL, TLONG, b));
    if (op == OORCB)
        return ir_bin(IR_OR, TLONG, ir_un(IR_COMPL, TLONG, a), ir_un(IR_COMPL, TLONG, b));
    if (op == OXOR)
        return ir_bin(IR_ER, TLONG, a, b);
    return ir_un(IR_COMPL, TLONG, ir_bin(IR_ER, TLONG, a, b));
}

/* the comparison operator of the jump operation OP (41 to 46, 51 to 56) */
static int
cmpof(word op)
{
    word k;

    k = op;
    if (k > 50)
        k = k - 10;
    if (k == OJEQ)
        return IR_EQ;
    if (k == OJLT)
        return IR_LT;
    if (k == OJLE)
        return IR_LE;
    if (k == OJGE)
        return IR_GE;
    if (k == OJGT)
        return IR_GT;
    return IR_NE;
}

/* a value kept in a temporary (the IR cannot have two variable shifts in one tree, and a node is used once) */
static word
tmpv(struct irn *e)
{
    word t;

    t = ir_newtemp();
    asg(ir_temp((int)t, TLONG), e);
    return t;
}

static struct irn *
tv(word t)
{
    return ir_temp((int)t, TLONG);
}

static struct irn *
shl(struct irn *x, struct irn *c)
{
    return ir_xshift(0, x, c);
}

static struct irn *
shr(struct irn *x, struct irn *c)
{
    return ir_xshift(1, x, c);
}

/* A shift or rotation of RD by a count that is not a constant, in line (the seed's same code): the runtime library is compiled so (IMP72CHARPTR, see
 * CHARPTR), for the routines IMPLSH, IMPASH and IMPROT are made of shifts and would call themselves.  A count of 64 places or more shifts everything
 * out; a negative count shifts to the right (arithmetic or logical as the operation); a rotation is by the count and 63 */
static void
eshiftv(word op, word rd, word v0, word v1)
{
    word ct, ct2, u1, u2, kr, lpos, lrs, lls, ldone;
    struct irn *x1, *x2;

    if (op == OROT) {
        x2 = icn(63); x1 = vexpr(v0, v1);
        x1 = ir_bin(IR_AND, TLONG, x1, x2); ct = tmpv(x1);
        x1 = ir_bin(IR_MINUS, TLONG, icn(64), tv(ct)); x2 = ir_bin(IR_AND, TLONG, x1, icn(63)); ct2 = tmpv(x2);
        u1 = tmpv(rv(rd)); u2 = tmpv(rv(rd));
        x1 = shl(tv(u1), tv(ct)); asg(tv(u1), x1);
        x1 = shr(tv(u2), tv(ct2)); asg(tv(u2), x1);
        x1 = ir_bin(IR_OR, TLONG, tv(u1), tv(u2));
        asg(rv(rd), x1);
        return;
    }
    if (op == OASH)
        kr = 2;
    else
        kr = 1;
    ct = tmpv(vexpr(v0, v1));
    lpos = ir_newlabel(); lrs = ir_newlabel(); lls = ir_newlabel(); ldone = ir_newlabel();
    ir_cbranch(IR_GE, tv(ct), icn(0), (int)lpos);
    ir_cbranch(IR_GT, tv(ct), icn(0 - 64), (int)lrs);
    if (op == OASH) {
        x1 = ir_xshift(2, rv(rd), icn(63));
        asg(rv(rd), x1);
    } else
        asg(rv(rd), icn(0));
    ir_goto((int)ldone);
    ir_label((int)lrs);
    x2 = ir_un(IR_UMINUS, TLONG, tv(ct)); x1 = ir_xshift((int)kr, rv(rd), x2); asg(rv(rd), x1);
    ir_goto((int)ldone);
    ir_label((int)lpos);
    ir_cbranch(IR_LT, tv(ct), icn(64), (int)lls);
    asg(rv(rd), icn(0));
    ir_goto((int)ldone);
    ir_label((int)lls);
    x1 = shl(rv(rd), tv(ct)); asg(rv(rd), x1);
    ir_label((int)ldone);
}

static int charptr(void);

/* A shift by a count: arithmetic (OP=OASH), logical (OLSH) or a rotation.  A constant count is done in line; the runtime routines IMP_LSH, IMP_ASH and
 * IMP_ROT do the rest: they take the addresses of the words IMP_SA, IMP_SB (the operands) and IMP_SR (the result), for a call cannot carry a 64 bit word
 * on a 32 bit machine */
static void
eshift(word op, word rd, word v0, word v1)
{
    word n;
    struct irn *x1, *x2, *x3;

    if ((v0 & 077) == 4)
        if (op != OROT) {
            n = v1;
            if (n == 0)
                return;
            if (n >= 64) {
                asg(rv(rd), icn(0));
                return;
            }
            if (n <= -64) {
                if (op == OASH)
                    n = -63;
                else {
                    asg(rv(rd), icn(0));
                    return;
                }
            }
            if (n > 0) {
                x2 = ir_xshift(0, rv(rd), icn(n));
                asg(rv(rd), x2);
                return;
            }
            if (op == OASH) {
                x2 = ir_xshift(2, rv(rd), icn(0 - n));
                asg(rv(rd), x2);
                return;
            }
            x2 = ir_xshift(1, rv(rd), icn(0 - n));
            asg(rv(rd), x2);
            return;
        }
    if (charptr() != 0) {
        eshiftv(op, rd, v0, v1);
        return;
    }
    shused = 1;
    x1 = ir_name("imp_sa", 0, TLONG); x2 = rv(rd); asg(x1, x2);
    x1 = ir_name("imp_sb", 0, TLONG); x2 = vexpr(v0, v1); asg(x1, x2);
    args[0] = ir_sym("imp_sa", 0, TPLONG);
    args[1] = ir_sym("imp_sb", 0, TPLONG);
    args[2] = ir_sym("imp_sr", 0, TPLONG);
    if (op == OLSH)
        x3 = ir_sym("implsh", 0, TPFTN);
    else {
        if (op == OASH)
            x3 = ir_sym("impash", 0, TPFTN);
        else
            x3 = ir_sym("improt", 0, TPFTN);
    }
    ir_stmt(ir_xcall(x3, args, 3, 1));
    x1 = ir_name("imp_sr", 0, TLONG);
    asg(rv(rd), x1);
}

/* The letter of the C type T (1 to 15) in the signature of a typed call: CHAR c, UCHAR C, SHORT s, USHORT S, INT i, UINT I, LONG l, ULONG L, LLONG q,
 * ULLONG Q, SIZE z, PTR p, DOUBLE d, FLOAT f, VOID v.  The writers of LLVM IR read them */
static int
ctlet(word t)
{
    static const char l[] = "cCsSiIlLqQzpdfv";

    return t >= 1 && t <= 14 ? l[t - 1] : 'v';
}

/* End a call of a declared C function (EXTERNAL INT F(PTR);): the function is named in the IR by its name, an @ and its signature, the letter of the type
 * of the result, then those of the arguments, with a dot before the arguments of a variadic call that have no parameter (f@ip.d: INT F(PTR, ...) called with
 * a PTR and a DOUBLE).  Every argument is a word of the IR; a DOUBLE, a FLOAT and a LLONG are the address of a word of 64 bits (imp_cw0 to imp_cw15, stored
 * just before), which the writer of LLVM IR loads with the type, for a call cannot carry 64 bits on a 32 bit machine.  A text constant for a PTR is its
 * address.  The value of the call is the word the program sees (the writer converts it), so the call has the type long on every machine (cret 2) */
static void
ecallt(word rd, word h)
{
    static char cwnb[16][64], csig[32], cnb[400];
    struct irn *tree, *v, *x1;
    word k, t, np, q;

    np = (h >> 4) & 31;
    q = 0;
    csig[q++] = (char)ctlet(h & 15);
    k = 0;
    while (k < nargs) {
        if ((h & 512) != 0)
            if (k == np)
                csig[q++] = '.';
        t = atys[k] & 15;
        if (t == 0)
            t = 7;
        if ((atys[k] & 16) != 0)
            v = ir_un(IR_PCONV, TLONG, args[k]);
        else
            v = ir_deref(args[k], TLONG);
        if (t == 9 || t == 10 || t == 13 || t == 14) {
            snprintf(cwnb[k], 64, "imp_cw%ld", (long)k);
            x1 = ir_name(cwnb[k], 0, TLONG); asg(x1, v);
            v = ir_sym(cwnb[k], 0, TPLONG);
            if (k >= cwused)
                cwused = k + 1;
        }
        args[k] = v;
        csig[q++] = (char)ctlet(t);
        k = k + 1;
    }
    if ((h & 512) != 0)
        if (nargs <= np)
            csig[q++] = '.';
    csig[q] = '\0';
    if ((h & 2048) != 0)
        snprintf(cnb, sizeof cnb, "impcallt@%s", csig);
    else
        snprintf(cnb, sizeof cnb, "%s@%s", g_irsymname(callfn), csig);
    tree = ir_xcall(ir_sym(cnb, 0, TPFTN), args, (int)nargs, 2);
    if (rd != 0)
        asg(rv(rd), tree);
    else
        ir_stmt(tree);
}

/* The address of the C function that calls the callback D (CALLBACK(NAME); a record ADDR with X 1): the symbol NAME@SIGNATURE, the signature as in a typed call (the letter
 * of the result, then one for each parameter).  The writers of LLVM IR define the function at the first use of the symbol (it is internal to the module) and declare the
 * subr NAME if the module does not define it */
static void
ecbaddr(word rd, word v0)
{
    static char csig[32], cnb[400];
    struct irn *x1, *x2;
    word d, h, np, k, q;

    d = HL(v0);
    h = g_cphead(d);
    np = (h >> 4) & 31;
    q = 0;
    csig[q++] = (char)ctlet(h & 15);
    for (k = 0; k < np; k++)
        csig[q++] = (char)ctlet(g_cptype(d, k));
    csig[q] = '\0';
    snprintf(cnb, sizeof cnb, "%s@%s", g_irsymname(d), csig);
    x1 = ir_sym(cnb, 0, TPLONG);
    x2 = ir_un(IR_PCONV, TLONG, x1);
    asg(rv(rd), x2);
}

/* End a call (CALLE): the call of the function CALLFN with the arguments ARGS; RD is where the value goes */
static void
ecall(word rd)
{
    struct irn *tree;
    word k, cval, cret;

    if (callmode != 0) {
        if (plast != NULL) {
            if (rd != 0)
                asg(rv(rd), plast);
            else
                ir_stmt(plast);
        }
        plast = NULL;
        callmode = 0;
        return;
    }
    cval = g_iscval(callfn);
    cret = cval;
    if (cret == 0)
        cret = g_irruntime(callfn);
    if (nfn < 60)
        fadd(&fnames, &nfn, callfn);
    if (g_cphead(callfn) != 0 && (g_cphead(callfn) & 1024) == 0) {
        ecallt(rd, g_cphead(callfn));
        return;
    }
    if (cval != 0) {
        /* the IMP runs this once for K=0 even with NARGS=0 (a FOR body runs once); it dereferences a stale ARGS[0] and nothing uses the result */
        for (k = 0; k <= nargs - 1; k++)
            args[k] = ir_deref(args[k], TLONG);
        ir_pass("# cfunc %s", g_irsymname(callfn));
    }
    tree = ir_xcall(ir_sym(g_irsymname(callfn), 0, TPFTN), args, (int)nargs, (int)cret);
    if (rd != 0)
        asg(rv(rd), tree);
    else
        ir_stmt(tree);
}

/* A print list item: a call of IMP_PR (or IMP_RD) for the item, with the format code N and the address of the operand (0 for none) */
static void
epitem(word n, word v0, word v1)
{
    struct irn *tree, *x1;

    if (plast != NULL) {
        ir_stmt(plast);                                 /* the cells are the next call's */
        plast = NULL;
    }
    asg(ir_name("impca", 0, TLONG), icn(n));
    if (v0 == 0)
        x1 = icn(0);
    else
        x1 = ir_un(IR_PCONV, TLONG, eaddr(v0, v1));
    asg(ir_name("impca", 8, TLONG), x1);
    pargs[0] = ir_sym("impca", 0, TPLONG);
    pargs[1] = ir_sym("impca", 8, TPLONG);
    if (callmode == 2)
        tree = ir_xcall(ir_sym("imprd", 0, TPFTN), pargs, 2, 0);
    else
        tree = ir_xcall(ir_sym("imppr", 0, TPFTN), pargs, 2, 0);
    plast = tree;
}

/* A jump through a table: GO[B] with a register in the address (type 020), or with a constant B, which is the offset of the tag */
static int
istabj(word v0, word v1)
{
    if ((v0 & 077) == 020)
        return 1;
    if (offset(v1) != 0)
        return 1;
    return 0;
}

/* The tag and the offset of the target of a conditional jump record (the X of the record) */
static word
jtag(word x)
{
    return x & 0xFFFFFFFFL;
}

static word
joff(word x)
{
    return (HR((uword)x >> 32) ^ 020000000000L) - 020000000000L;
}

/* The index in RLIST of the tag D, or -1 */
static word
tabfind(word d)
{
    word j, i, rr;

    j = -1;
    i = 0;
    do {
        rr = FREE(rlist + i);
        if (G_RKIND(rr) == KTAG)
            if (G_RX(rr) == d)
                j = i;
        i = i + 1;
    } while (i <= nrl - 1);
    return j;
}

/* The label of the place tag+O, where the jumps after the tag D are a table (GO TO GO[B], and GO TO (L0,L1,...) B, which is GO TO GO[B]; GO: GO TO L0; GO TO L1; ...): the target of the
 * O'th jump, or, for an O out of the table, the place after it, as when the jumps are instructions (GO TO COND[1] skips the instruction after the tag).  Tags between the jumps take no space */
static word
tablabel(word d, word o)
{
    word j, i, k, rr, l;

    j = tabfind(d);
    if (j < 0) {
        imp_error(1, "COMPUTED GO TO WITHOUT ITS TABLE.");
        return taglabel(d);
    }
    k = 0; i = j + 1;
    while (i < nrl) {                                   /* TL1 */
        rr = FREE(rlist + i);
        if (G_RKIND(rr) == KTAG) {
            i = i + 1;
            continue;
        }
        if (G_RKIND(rr) == KOP && G_ROP(rr) == OJUMP && istabj(G_RV0(rr), G_RV1(rr)) == 0) {
            if (k == o)
                return taglabel(HL(G_RV0(rr)));
            k = k + 1; i = i + 1;
            continue;
        }
        break;
    }
    l = ir_newlabel();
    fadd(&tble, &ntble, i); fadd(&tble, &ntble, l);
    return l;
}

/* The label a conditional jump record jumps to (its X) */
static word
jlabel(word x)
{
    if (joff(x) != 0)
        return tablabel(jtag(x), joff(x));
    return taglabel(jtag(x));
}

/* GO TO GO[B] (a computed GO TO): the index is the register plus the offset.  A comparison for each jump of the table */
static void
etabjump(word v0, word v1)
{
    word d, r, o, i, j, k, t, rr, l;
    struct irn *e;

    d = HL(v0); r = 03777 & HL(v1); o = offset(v1);
    j = tabfind(d);
    if (j < 0) {
        imp_error(1, "COMPUTED GO TO WITHOUT ITS TABLE.");
        return;
    }
    if (r == 0) {
        ir_goto((int)tablabel(d, o));
        return;
    }
    e = rv(r);
    if (o != 0)
        e = ir_bin(IR_PLUS, TLONG, e, icn(o));
    t = ir_newtemp();
    asg(ir_temp((int)t, TLONG), e);
    k = 0; i = j + 1;
    while (i < nrl) {                                   /* TJ1 */
        rr = FREE(rlist + i);
        if (G_RKIND(rr) == KTAG) {
            i = i + 1;
            continue;
        }
        if (G_RKIND(rr) == KOP && G_ROP(rr) == OJUMP && istabj(G_RV0(rr), G_RV1(rr)) == 0) {
            int lab = (int)taglabel(HL(G_RV0(rr)));

            ir_cbranch(IR_EQ, ir_temp((int)t, TLONG), icn(k), lab);
            k = k + 1; i = i + 1;
            continue;
        }
        break;
    }
    l = ir_newlabel();                                  /* an index out of the table goes on after it */
    fadd(&tble, &ntble, i); fadd(&tble, &ntble, l);
    ir_goto((int)l);
}

/* BYTES: the descriptors (BYTEP) and the byte pointers.  A byte pointer kept in a variable or register is one word: bits 63-58 the size, 57-52 the position
 * (bits from the right, as in A<S,P>), and the low 52 bits the byte address of the word that holds the field.  Stepping it moves to the next lower field of
 * the same size, or to the next word (address+8) when there is no room. */
static word
bdfind(word d)
{
    word i;

    if (g_nbd > 0) {
        i = 0;
        do {
            if (FREE(g_bdesc + i) == d)
                return i;
            i = i + 7;
        } while (i <= g_nbd - 7);
    }
    return 0 - 1;
}

/* the mask of a field of size (V0,V1) */
static struct irn *
fmask(word v0, word v1)
{
    word m;
    struct irn *x1, *x2;

    if ((v0 & 077) == 4) {
        if (v1 >= 64)
            m = -1;
        else
            m = (g_ls(1, v1) - 1);
        return icn(m);
    }
    x2 = icn(1);
    x1 = vexpr(v0, v1);
    return ir_bin(IR_MINUS, TLONG, shl(x2, x1), icn(1));
}

/* The value of the descriptor at BDESC+J as a byte pointer (BYTEP A<S,P>).  A pointer to characters, the tape's BYTEP A<7,36> (or A<8,64>), is the byte
 * address of the byte before the first of A (a step comes before the access); any other field gives the pointer of the runtime (size, position, address of
 * the word) */
static struct irn *
bpvalue(word j)
{
    struct irn *e, *x1, *x2, *x3, *x4;
    word a0, a1, sz, ps, ch;

    a0 = FREE((g_bdesc + j) + 1); a1 = FREE((g_bdesc + j) + 2);
    x1 = eaddr(a0, a1);
    e = ir_un(IR_PCONV, TLONG, x1);
    ch = 0;
    if ((FREE((g_bdesc + j) + 3) & 077) == 4)
        if ((FREE((g_bdesc + j) + 5) & 077) == 4) {
            sz = FREE((g_bdesc + j) + 4); ps = FREE((g_bdesc + j) + 6);
            if (sz == 7)
                if (ps == 36)
                    ch = 1;
            if (sz == 8)
                if (ps == 64)
                    ch = 1;
        }
    if (ch != 0)
        return ir_bin(IR_MINUS, TLONG, e, icn(1));
    a0 = FREE((g_bdesc + j) + 3); a1 = FREE((g_bdesc + j) + 4);
    x2 = vexpr(a0, a1);
    x3 = shl(x2, icn(58));
    e = ir_bin(IR_OR, TLONG, e, x3);
    a0 = FREE((g_bdesc + j) + 5); a1 = FREE((g_bdesc + j) + 6);
    x2 = vexpr(a0, a1);
    x4 = shl(x2, icn(52));
    return ir_bin(IR_OR, TLONG, e, x4);
}

/* The pointer operand (V0,V1) into IMP_BPP, and back; the runtime routines IBP, LDB and DPB work on IMP_BPP and IMP_BPV */
static void
bpin(word v0, word v1)
{
    struct irn *x1, *x2;

    x1 = ir_name("impbpp", 0, TLONG); x2 = vexpr(v0, v1); asg(x1, x2);
}

static void
bpout(word v0, word v1)
{
    struct irn *x1, *x2;

    x1 = ir_name("impbpp", 0, TLONG); x2 = vexpr(v0, v1); asg(x2, x1);
}

/* IMP72CHARPTR in the environment (the seed's same switch): a byte pointer held in a variable is a pointer to characters, a byte address, and the stepping,
 * loading and storing are done in line instead of by IMPIBP, IMPLDB and IMPDPB.  The runtime library (lib/imp72rt.imp, imp72io.imp) is compiled so: those
 * routines are the ones that would call themselves.  CPFLAG: 0 not asked yet, 1 no, 2 yes */
static int
charptr(void)
{
    if (cpflag == 0) {
        if (getenv("IMP72CHARPTR") == NULL)
            cpflag = 1;
        else
            cpflag = 2;
    }
    return cpflag == 2;
}

/* the pointer operand (V0,V1) stepped by one byte */
static void
cpstep(word v0, word v1)
{
    struct irn *x1, *x2;

    x1 = vexpr(v0, v1); x2 = icn(1); x1 = ir_bin(IR_PLUS, TLONG, x1, x2);
    x2 = vexpr(v0, v1); asg(x2, x1);
}

/* the byte the pointer operand (V0,V1) points to */
static struct irn *
cpbyte(word v0, word v1)
{
    struct irn *x1, *x2;

    x1 = vexpr(v0, v1); x2 = ir_un(IR_PCONV, TPUCHAR, x1);
    return ir_deref(x2, T_UCHAR);
}

static void
bpcall(const char *nm)
{
    struct irn *x1;

    x1 = ir_sym(nm, 0, TPFTN);
    ir_stmt(ir_xcall(x1, args, 0, 0));
}

/* RD gets (W shifted right by P) and M, the temporaries; one step at a time, for a routine's arguments are not re-entered */
static void
efield(word rd, word w, word p, word m)
{
    struct irn *x1, *x2, *x3, *x4;

    x1 = shr(tv(w), tv(p));
    x2 = tv(m);
    x3 = ir_bin(IR_AND, TLONG, x1, x2);
    x4 = rv(rd);
    asg(x4, x3);
}

/* the word of a field: its old value with the bits of the mask (temporary M) cleared and the bits of the value (temporary V) put in */
static struct irn *
enewword(struct irn *old, word m, word v)
{
    struct irn *x1, *x2, *x3;

    x1 = ir_un(IR_COMPL, TLONG, tv(m));
    x2 = ir_bin(IR_AND, TLONG, old, x1);
    x3 = ir_bin(IR_AND, TLONG, tv(v), tv(m));
    return ir_bin(IR_OR, TLONG, x2, x3);
}

/* Load byte: RD gets the field, right justified.  The operand is a descriptor (A<S,P>) or a variable holding a byte pointer; STEP: ILDB */
static void
eldb(word rd, word v0, word v1, word step)
{
    word j, w, p, m;
    struct irn *x1, *x2;
    word a0, a1;

    j = -1;
    if ((v0 & 077) == 010)
        if (step == 0)
            j = bdfind(HL(v0));
    if (j >= 0) {
        a0 = FREE((g_bdesc + j) + 1); a1 = FREE((g_bdesc + j) + 2);
        x1 = vexpr(a0, a1); w = tmpv(x1);
        a0 = FREE((g_bdesc + j) + 5); a1 = FREE((g_bdesc + j) + 6);
        x1 = vexpr(a0, a1); p = tmpv(x1);
        a0 = FREE((g_bdesc + j) + 3); a1 = FREE((g_bdesc + j) + 4);
        x1 = fmask(a0, a1); m = tmpv(x1);
        efield(rd, w, p, m);
        return;
    }
    if (charptr() != 0) {
        if (step != 0)
            cpstep(v0, v1);
        x1 = cpbyte(v0, v1); x2 = ir_un(IR_SCONV, TLONG, x1);
        x1 = rv(rd);
        asg(x1, x2);
        return;
    }
    bpin(v0, v1);
    if (step != 0) {
        bpcall("impibp");
        bpout(v0, v1);
    }
    bpcall("impldb");
    x1 = ir_name("impbpv", 0, TLONG); x2 = rv(rd);
    asg(x2, x1);
}

/* Deposit byte: the low bits of RD into the field; STEP: IDPB */
static void
edpb(word rd, word v0, word v1, word step)
{
    word j, p, k, m, v;
    struct irn *x1, *x2, *x3;
    word a0, a1, b0, b1;

    j = -1;
    if ((v0 & 077) == 010)
        if (step == 0)
            j = bdfind(HL(v0));
    if (j >= 0) {
        a0 = FREE((g_bdesc + j) + 5); a1 = FREE((g_bdesc + j) + 6);
        x1 = vexpr(a0, a1); p = tmpv(x1);
        a0 = FREE((g_bdesc + j) + 3); a1 = FREE((g_bdesc + j) + 4);
        x1 = fmask(a0, a1); k = tmpv(x1);
        x1 = shl(tv(k), tv(p)); m = tmpv(x1);
        x1 = rv(rd); x2 = shl(x1, tv(p)); v = tmpv(x2);
        b0 = FREE((g_bdesc + j) + 1); b1 = FREE((g_bdesc + j) + 2);
        x1 = vexpr(b0, b1);
        x2 = enewword(x1, m, v);
        x3 = vexpr(b0, b1);
        asg(x3, x2);                                    /* EWRITE */
        return;
    }
    if (charptr() != 0) {
        if (step != 0)
            cpstep(v0, v1);
        x1 = cpbyte(v0, v1); x2 = rv(rd); x2 = ir_un(IR_SCONV, T_UCHAR, x2);
        asg(x1, x2);
        return;
    }
    x1 = rv(rd); x2 = ir_name("impbpv", 0, TLONG); asg(x2, x1);
    bpin(v0, v1);
    if (step != 0) {
        bpcall("impibp");
        bpout(v0, v1);
    }
    bpcall("impdpb");
}

/* HRLM, HRRM: the right half of RD into the left (POS=32) or the right (POS=0) half of the operand; the halves are 32 bits */
static void
ehalf(word rd, word v0, word v1, word pos)
{
    word m, v;
    struct irn *x1, *x2, *x3;

    x1 = icn(G_HM);
    if (pos != 0) {
        x2 = icn(pos); x3 = ir_bin(IR_LS, TLONG, x1, x2);
    } else
        x3 = x1;
    m = tmpv(x3);
    x1 = rv(rd); x2 = icn(pos); x3 = shl(x1, x2);
    v = tmpv(x3);
    x1 = vexpr(v0, v1);
    x2 = enewword(x1, m, v);
    x1 = vexpr(v0, v1);
    asg(x1, x2);                                        /* EWRITE */
}

/* CALLI: RD gets the value of IMP_CALLI(the code, the AC); the library routine takes the two words as C longs */
static void
ecalli(word rd, word v0, word v1)
{
    struct irn *x1, *x2, *x3;

    x2 = vexpr(v0, v1);
    asg(ir_name("impca", 0, TLONG), x2);
    x3 = rv(rd);
    asg(ir_name("impca", 8, TLONG), x3);
    args[0] = ir_sym("impca", 0, TPLONG);
    args[1] = ir_sym("impca", 8, TPLONG);
    x1 = ir_sym("impcalli", 0, TPFTN);
    x2 = ir_xcall(x1, args, 2, 0);
    asg(rv(rd), x2);
}

/* Execute: the word is an instruction of this machine, run by the library's IMP_XCT.  The registers of the code are temps, the accumulators of the
 * instruction are the words of IMP_AC: the registers 1 to 15 that the routine uses are stored there before and loaded after */
static void
executew(word v0, word v1)
{
    word i;
    struct irn *x1, *x2;

    x1 = vexpr(v0, v1); x2 = ir_name("impxw", 0, TLONG); asg(x2, x1);       /* the word goes in IMP_XW: a runtime routine takes a C long, which is not a word everywhere */
    i = 33;
    do {
        if (flen(rtmp) > i)
            if (FREE(rtmp + i) != 0) {
                x1 = ir_sym("impac", 8 * (i - 33), TPLONG); x2 = ir_deref(x1, TLONG);
                asg(x2, rv(i));
            }
        i = i + 1;
    } while (i <= 48);
    x1 = ir_sym("impxct", 0, TPFTN);
    ir_stmt(ir_xcall(x1, args, 0, 0));
    i = 33;
    do {
        if (flen(rtmp) > i)
            if (FREE(rtmp + i) != 0) {
                x1 = ir_sym("impac", 8 * (i - 33), TPLONG); x2 = ir_deref(x1, TLONG);
                asg(rv(i), x2);
            }
        i = i + 1;
    } while (i <= 48);
}

/* One record of the code */
static void
erec(word r)
{
    word op, rd, v0, v1, k;
    struct irn *x1, *x2, *x3, *x4, *tree;

    op = G_ROP(r); rd = G_RRD(r); v0 = G_RV0(r); v1 = G_RV1(r);
    if (G_RKIND(r) == KTAG) {
        ir_label((int)taglabel(G_RX(r)));
        return;
    }
    if (G_RKIND(r) != KOP)
        return;
    if (op == OENTRY)
        return;
    if (op == OPARAM)
        return;
    if (op == OLOAD) {
        x2 = vexpr(v0, v1); x1 = rv(rd); asg(x1, x2);
        return;
    }
    if (op == ONEG) {
        x2 = ir_un(IR_UMINUS, TLONG, vexpr(v0, v1)); x1 = rv(rd); asg(x1, x2);
        return;
    }
    if (op == OCOMPL) {
        x2 = ir_un(IR_COMPL, TLONG, vexpr(v0, v1)); x1 = rv(rd); asg(x1, x2);
        return;
    }
    if (op == OADDR) {
        if (G_RX(r) == 1) {
            ecbaddr(rd, v0);
            return;
        }
        x2 = ir_un(IR_PCONV, TLONG, eaddr(v0, v1)); x1 = rv(rd); asg(x1, x2);
        return;
    }
    if (op == OSTORE) {                                 /* ASG(VEXPR(V0,V1),RV(RD)): the right argument first */
        x2 = rv(rd); x1 = vexpr(v0, v1); asg(x1, x2);
        return;
    }
    if (op == OADD || op == OSUB || op == OMUL || op == ODIV || op == OMOD) {
        int iop = op == OADD ? IR_PLUS : op == OSUB ? IR_MINUS : op == OMUL ? IR_MUL : op == ODIV ? IR_DIV : IR_MOD;

        x3 = vexpr(v0, v1); x2 = rv(rd); x4 = ir_bin(iop, TLONG, x2, x3); x1 = rv(rd); asg(x1, x4);
        return;
    }
    if (op == OADDX) {
        x4 = icn(8); x3 = vexpr(v0, v1); x3 = ir_bin(IR_MUL, TLONG, x3, x4);
        x2 = rv(rd); x4 = ir_bin(IR_PLUS, TLONG, x2, x3); x1 = rv(rd); asg(x1, x4);
        return;
    }
    if (op >= OAND && op <= OEQV) {
        x3 = vexpr(v0, v1); x2 = rv(rd); x4 = boolop(op, x2, x3); x1 = rv(rd); asg(x1, x4);
        return;
    }
    if (op >= OLSH && op <= OROT) {
        eshift(op, rd, v0, v1);
        return;
    }
    if (op >= OFADD && op <= OFDIV) {
        if (op == OFADD)
            k = IR_PLUS;
        if (op == OFSUB)
            k = IR_MINUS;
        if (op == OFMUL)
            k = IR_MUL;
        if (op == OFDIV)
            k = IR_DIV;
        x1 = rv(rd); x2 = ir_todouble(x1, 0);
        x1 = vexpr(v0, v1); x3 = ir_todouble(x1, 1);
        tree = ir_bin((int)k, T_DOUBLE, x2, x3);
        x1 = ir_fromdouble(tree);
        asg(rv(rd), x1);
        return;
    }
    if (op == ONEGF) {
        x3 = icn(g_ls(1, 63)); x2 = vexpr(v0, v1); x2 = ir_bin(IR_ER, TLONG, x2, x3);
        x1 = rv(rd); asg(x1, x2);
        return;
    }
    if (op == OLDB) {
        eldb(rd, v0, v1, 0);
        return;
    }
    if (op == OILDB) {
        eldb(rd, v0, v1, 1);
        return;
    }
    if (op == ODPB) {
        edpb(rd, v0, v1, 0);
        return;
    }
    if (op == OIDPB) {
        edpb(rd, v0, v1, 1);
        return;
    }
    if (op == OFLOAT) {
        x1 = vexpr(v0, v1); x2 = ir_un(IR_SCONV, T_DOUBLE, x1); x1 = ir_fromdouble(x2); asg(rv(rd), x1);
        return;
    }
    if (op == OFIX) {
        x1 = vexpr(v0, v1); x2 = ir_todouble(x1, 0); x1 = ir_un(IR_SCONV, TLONG, x2); asg(rv(rd), x1);
        return;
    }
    if (op == OJUMP) {
        if (istabj(v0, v1)) {
            etabjump(v0, v1);
            return;
        }
        ir_goto((int)taglabel(HL(v0)));
        return;
    }
    if (op >= OJEQ && op <= OJNE) {                     /* IXCBRANCH(CMPOF(OP),RV(RD),VEXPR(V0,V1),TAGLABEL(RX(R))): the arguments from the right */
        int lab = (int)jlabel(G_RX(r));

        x2 = vexpr(v0, v1); x1 = rv(rd);
        ir_cbranch(cmpof(op), x1, x2, lab);
        return;
    }
    if (op >= 51 && op <= 56) {                         /* the jumps on reals */
        int lab;

        x1 = rv(rd); x2 = ir_todouble(x1, 0);
        x1 = vexpr(v0, v1); x3 = ir_todouble(x1, 1);
        lab = (int)jlabel(G_RX(r));
        ir_cbranch(cmpof(op), x2, x3, lab);
        return;
    }
    if (op == OCALLB) {
        callfn = HL(v0); nargs = 0; plast = NULL; callmode = g_irfnkind(callfn);
        return;
    }
    if (op == OARG) {
        if (nargs < 16) {
            args[nargs] = eaddr(v0, v1);
            atys[nargs] = G_RX(r);
            nargs = nargs + 1;
        }
        return;
    }
    if (op == OCALLE) {
        ecall(rd);
        return;
    }
    if (op == OCALLI) {
        ecalli(rd, v0, v1);
        return;
    }
    if (op == OHRLM) {
        ehalf(rd, v0, v1, 32);
        return;
    }
    if (op == OHRRM) {
        ehalf(rd, v0, v1, 0);
        return;
    }
    if (op == OXCT) {
        executew(v0, v1);
        return;
    }
    if (op == OPITEM) {
        epitem(G_RX(r), v0, v1);
        return;
    }
    if (op == ORET) {
        asg(ir_temp((int)rettmp, TLONG), rv(rd));
        ir_goto((int)retlab);
        return;
    }
    imp_error(1, "UNKNOWN OPERATION IN THE CODE.");
}

/* A GO TO to a tag of another routine (ERROR of the original's IMPM goes to FIN of the main program) has no jump to be: the routine that has the tag
 * takes a setjmp at its entry (EXPORTTAGS) and the jump is a longjmp to it.  XTAGS are the tags jumped to from outside.  A tag of the unit that is not in
 * the routine of the jump is found by XSCAN before any code is made */
static word
tagdef(word b, word i0, word i1, word d)
{
    word j;

    j = i0;
    do {
        if (G_RKIND(FREE(b + j)) == KTAG)
            if (G_RX(FREE(b + j)) == d)
                return 1;
        j = j + 1;
    } while (j <= i1 - 1);
    return 0;
}

static word
isxtag(word d)
{
    word i;

    if (nxt > 0) {
        i = 0;
        do {
            if (FREE(xtags + i) == d)
                return 1;
            i = i + 1;
        } while (i <= nxt - 1);
    }
    return 0;
}

/* The tag a jump record jumps to, or 0 */
static word
jumptag(word r)
{
    word op;

    if (G_RKIND(r) != KOP)
        return 0;
    op = G_ROP(r);
    if (op == OJUMP) {
        if (istabj(G_RV0(r), G_RV1(r)))
            return 0;
        return HL(G_RV0(r));
    }
    if ((op >= OJEQ && op <= OJNE) || (op >= 51 && op <= 56)) {
        if (joff(G_RX(r)) != 0)
            return 0;
        return jtag(G_RX(r));
    }
    return 0;
}

/* Only a tag of the main program (the records before the first entry, up to mainend), which is active whenever a routine runs, can be jumped to from other routines; a tag of another
 * subroutine (which may have returned) or of no routine is the error of echktags */
static word mainend;

static void
xscan(word i0, word i1)
{
    word i, t;

    i = i0;
    do {
        t = jumptag(FREE(rlist + i));
        if (t != 0)
            if (tagdef(rlist, i0, i1, t) == 0)
                if (tagdef(rlist2, 0, nrl2, t) == 0)
                    if (isxtag(t) == 0)
                        if (tagdef(rlist, 0, mainend, t) != 0)
                            fadd(&xtags, &nxt, t);
        i = i + 1;
    } while (i <= i1 - 1);
}

/* At the entry of a routine: a setjmp for each of its tags that other routines jump to; a nonzero return is the jump */
static void
exporttags(word i0, word i1)
{
    word i, t;
    struct irn *x1, *x2;

    if (nxt > 0) {
        i = 0;
        do {
            t = FREE(xtags + i);
            if (tagdef(rlist, i0, i1, t) + tagdef(rlist2, 0, nrl2, t) != 0) {
                int lab;

                args[0] = ir_sym(g_irjbname(t), 0, TPLONG);
                x1 = ir_sym("_setjmp", 0, TPFTN);
                x2 = ir_xcall(x1, args, 1, 1);
                lab = (int)taglabel(t);
                ir_cbranch(IR_NE, x2, icn(0), lab);
            }
            i = i + 1;
        } while (i <= nxt - 1);
    }
}

/* A jump to a tag that is not in the function (a GO TO across subroutines) is an error; the tag is defined at the end, for the first back end stopped at
 * a label not defined */
static void
echktags(word i0, word i1)
{
    word i, j, q;
    struct irn *x1;

    if (ntags > 0) {
        i = 0;
        do {
            q = 0;
            j = i0;
            do {
                if (G_RKIND(FREE(rlist + j)) == KTAG)
                    if (G_RX(FREE(rlist + j)) == FREE(tags + i))
                        q = 1;
                j = j + 1;
            } while (j <= i1 - 1);
            if (nrl2 > 0) {
                j = 0;
                do {
                    if (G_RKIND(FREE(rlist2 + j)) == KTAG)
                        if (G_RX(FREE(rlist2 + j)) == FREE(tags + i))
                            q = 1;
                    j = j + 1;
                } while (j <= nrl2 - 1);
            }
            if (q == 0) {
                if (isxtag(FREE(tags + i)) == 0)
                    g_irtagerr(FREE(tags + i));
                ir_label((int)FREE((tags + i) + 1));
                if (isxtag(FREE(tags + i)) != 0) {
                    args[0] = ir_sym(g_irjbname(FREE(tags + i)), 0, TPLONG); args[1] = icn(1);
                    x1 = ir_sym("_longjmp", 0, TPFTN);
                    ir_stmt(ir_xcall(x1, args, 2, 0));
                }
            }
            i = i + 2;
        } while (i <= ntags - 2);
    }
}

/* A word of a data statement is two words of DDW: the symbol whose address it is (0 for a number) and the number, or the offset in bytes from the symbol */
static void
edatw(word r)
{
    word d;

    if (G_RV0(r) == 0) {
        fadd(&ddw, &nddw, 0); fadd(&ddw, &nddw, G_RV1(r));
        return;
    }
    d = HL(G_RV0(r));
    if (d == 0 || (03777 & HL(G_RV1(r))) != 0) {
        imp_error(1, "NON-CONSTANT EXPRESSION IN DATA STATEMENT.");
        return;
    }
    if (g_irkind(d) == 0)
        usevar(d);
    else
        poolof(d);
    fadd(&ddw, &nddw, d); fadd(&ddw, &nddw, 8 * offset(G_RV1(r)));
}

/* The assembler name of the symbol D of a data word */
static const char *
datsym(word d)
{
    if (g_irkind(d) == 0)
        return g_irsymname(d);
    return g_irlabname(poolof(d));
}

/* The labels of the jumps through tables that are defined before the record I of the unit (ebody) */
static void
tblemit(word i)
{
    word p;

    if (ntble > 0) {
        p = 0;
        do {
            if (FREE(tble + p) == i) {
                ir_label((int)FREE((tble + p) + 1));
                FREES(tble + p, -1);
            }
            p = p + 2;
        } while (p <= ntble - 2);
    }
}

/* The records B[I0] to B[I1-1] of a function (B is the address of the variable that holds the array: it can move when another array grows).  F=1: the
 * remote code (REMOT), of which only the segments marked with NN, the name of the routine (0: the main program) are its */
static void
ebody(word *b, word i0, word i1, word f, word nn)
{
    word i, r, cnt, act;

    i = i0; act = 1;
    if (f != 0)
        act = 0;
    for (;;) {                                          /* EB1 */
        if (b == &rlist)
            tblemit(i);
        if (i >= i1)
            break;
        r = FREE(*b + i);
        if (f != 0)
            if (G_RKIND(r) == KOP)
                if (G_ROP(r) == OREMOTE) {
                    if (G_RX(r) == nn)
                        act = 1;
                    else
                        act = 0;
                    i = i + 1;
                    continue;
                }
        if (act == 0) {
            i = i + 1;
            continue;
        }
        /* data: a tag followed by words */
        if (G_RKIND(r) == KTAG)
            if (i + 1 < i1)
                if (G_RKIND(FREE((*b + i) + 1)) == KWORD) {
                    fadd(&ddl, &nddl, G_RX(r)); fadd(&ddl, &nddl, nddw);
                    cnt = 0; i = i + 1;
                    while (i < i1 && G_RKIND(FREE(*b + i)) == KWORD) {      /* EB2 */
                        edatw(FREE(*b + i));
                        cnt = cnt + 1; i = i + 1;
                    }
                    fadd(&ddl, &nddl, cnt);
                    continue;
                }
        erec(r);
        i = i + 1;
    }
}

/* An execute in the records B[I0] to B[I1-1]? */
static word
hasxct(word b, word i0, word i1)
{
    word i, r;

    i = i0;
    do {
        r = FREE(b + i);
        if (G_RKIND(r) == KOP)
            if (G_ROP(r) == OXCT)
                return 1;
        i = i + 1;
    } while (i <= i1 - 1);
    return 0;
}

/* The registers (0R to 15R, the temporaries 33 to 48) that the records B[I0] to B[I1-1] mention get their temporaries: an execute stores and loads those that exist when it is made, and a register
 * that the routine mentions only after the execute (in the order of the code) must be one of them */
static void
exregs(word b, word i0, word i1)
{
    word i, r, x;

    i = i0;
    do {
        r = FREE(b + i);
        if (G_RKIND(r) == KOP) {
            x = G_RRD(r);
            if (x >= 33 && x <= 48)
                rv(x);
            if ((G_RV0(r) & 077) != 4) {
                x = 03777 & HL(G_RV1(r));
                if (x >= 33 && x <= 48)
                    rv(x);
            }
        }
        i = i + 1;
    } while (i <= i1 - 1);
}

/* A function: the records RLIST[I0] to RLIST[I1-1]; the main program (ISMAIN) or a subroutine, which begins with the entry record and its parameters */
static void
efunc(word i0, word i1, word ismain)
{
    word i, r, k, j, nn;
    const char *nm;

    if (ismain != 0)
        nm = "impmain";
    else {
        nm = g_irsymname(HL(G_RV0(FREE(rlist + i0))));
        fadd(&fnames, &nfn, HL(G_RV0(FREE(rlist + i0))));
    }
    ir_func_begin(nm, 1);
    j = 0;
    do {
        FREES(rtmp + j, 0);
        j = j + 1;
    } while (j <= flen(rtmp) - 1);
    ntags = 0;
    retlab = ir_newlabel();
    rettmp = ir_newtemp();
    k = 0;
    if (ismain == 0) {
        i = i0;
        do {
            if ((r = FREE(rlist + i)) != 0)
                if (G_RKIND(r) == KOP)
                    if (G_ROP(r) == OPARAM) {
                        struct irn *x2 = ir_un(IR_PCONV, TLONG, ir_param((int)k));  /* ASG(RV(RRD(R)),IXUN(IRPCONV,TLONG,IXPARAM(K))): the right argument first */

                        asg(rv(G_RRD(r)), x2);
                        k = k + 1;
                    }
            i = i + 1;
        } while (i <= i1 - 1);
    }
    if (hasxct(rlist, i0, i1) + (nrl2 > 0 && hasxct(rlist2, 0, nrl2)) != 0) {
        exregs(rlist, i0, i1);
        if (nrl2 > 0)
            exregs(rlist2, 0, nrl2);
    }
    ir_label(ir_newlabel());            /* the first back end's optimizer wanted a label between the argument moves and the code (DELJUMPS: prolog, label, moves, label, code) */
    exporttags(i0, i1);
    ebody(&rlist, i0, i1, 0, 0);
    if (ismain != 0)
        nn = 0;
    else
        nn = HL(G_RV0(FREE(rlist + i0)));
    ebody(&rlist2, 0, nrl2, 1, nn);                     /* the remote code of this routine (REMOT) */
    echktags(i0, i1);
    ir_func_end((int)retlab, (int)rettmp);
}

/* The data of the unit: the variables used (common symbols, sized by their declarations), data statements and the pooled constants */
static void
edata(void)
{
    word i, d, j, sz, w, q;

    if (ir_usedfs != 0) {
        ir_pass(" .comm imp_fs0,8,8"); ir_pass(" .comm imp_fs1,8,8"); ir_pass(" .comm imp_fsr,8,8");
    }
    if (cwused > 0) {
        char nb[64];

        for (i = 0; i <= cwused - 1; i++) {
            snprintf(nb, sizeof nb, "imp_cw%ld", (long)i);
            ir_pass(" .comm %s,8,8", nb);
        }
    }
    if (shused != 0) {
        ir_pass(" .comm imp_sa,8,8"); ir_pass(" .comm imp_sb,8,8"); ir_pass(" .comm imp_sr,8,8");
        shused = 0;
    }
    if (nused > 0) {
        i = 0;
        do {
            d = FREE(usedv + i);
            q = 0;
            if (nddl > 0) {
                j = 0;
                do {
                    if (FREE(ddl + j) == d)
                        q = 1;
                    j = j + 3;
                } while (j <= nddl - 3);
            }
            if (nfn > 0) {
                j = 0;
                do {
                    if (FREE(fnames + j) == d)
                        q = 1;
                    j = j + 1;
                } while (j <= nfn - 1);
            }
            if (dprop(DP_COM, d) == 3)
                q = 1;                                  /* external: defined elsewhere */
            if (q == 0) {
                sz = 8;
                if (g_narr > 0) {
                    j = 0;
                    do {
                        w = FREE(g_arr + j);
                        if (HR(w) == d)
                            sz = 8 * HL(w);
                        j = j + 1;
                    } while (j <= g_narr - 1);
                }
                if (dprop(DP_COM, d) == 0)
                    ir_pass(" .local %s", g_irsymname(d));
                ir_pass(" .comm %s,%ld,8", g_irsymname(d), (long)sz);
                g_irvarrec(g_irsymname(d), sz);
            }
            i = i + 1;
        } while (i <= nused - 1);
    }
    if (nxt > 0) {
        i = 0;
        do {
            ir_pass(" .comm %s,512,16", g_irjbname(FREE(xtags + i)));
            i = i + 1;
        } while (i <= nxt - 1);
    }
    if (nddl > 0) {
        i = 0;
        do {
            ir_pass(" .data");
            ir_pass(" .balign 8");
            ir_pass("%s:", g_irsymname(FREE(ddl + i)));
            j = 0;
            do {
                word w = FREE((ddl + i) + 1) + 2 * j, q = FREE(ddw + w);

                if (q != 0)
                    ir_pass(" .quad %s+%ld", datsym(q), (long)FREE(ddw + (w + 1)));
                else
                    ir_pass(" .quad %lld", (long long)FREE(ddw + (w + 1)));
                j = j + 1;
            } while (j <= FREE((ddl + i) + 2) - 1);
            i = i + 3;
        } while (i <= nddl - 3);
    }
    if (npool > 0) {
        i = 0;
        do {
            ir_pass(" .data");
            ir_pass(" .balign 8");
            ir_pass(".L%d:", (int)FREE((pools + i) + 1));
            g_irpoolemit(FREE(pools + i));
            i = i + 2;
        } while (i <= npool - 2);
    }
}

/* DEMIT(A): the unit whose code is the chain of stack entry A */
word
g_demit(word a)
{
    word r, i, start;

    if (dinit == 0) {
        dinit = 1;
        frelot(&rtmp, "RTMP", 300, NULL); fincset(rtmp, 300);
        frelot(&tags, "TAGS", 20, NULL); frelot(&usedv, "USEDV", 20, NULL); frelot(&pools, "POOLS", 20, NULL);
        frelot(&fnames, "FNAMS", 20, NULL); frelot(&rlist, "RLIST", 100, NULL); frelot(&rlist2, "RLIS2", 20, NULL);
        frelot(&xtags, "XTAGS", 20, NULL); frelot(&ddl, "DDL", 20, NULL); frelot(&ddw, "DDW", 20, NULL); frelot(&tble, "TBLE", 20, NULL);
    }
    nrl = 0; nxt = 0; ntags = 0; nused = 0; npool = 0; nfn = 0; nddl = 0; nddw = 0; ntble = 0;
    ir_usedfs = 0; cwused = 0;
    r = HL(FREE(a + 2));
    while (r != 0) {                                    /* EM1 */
        fadd(&rlist, &nrl, r);
        r = G_RNEXT(r);
    }
    nrl2 = 0;
    if (g_remo != 0) {
        r = HL(FREE(g_remo + 2));                       /* the remote code: the original puts it in the high segment */
        while (r != 0) {                                /* EM3 */
            fadd(&rlist2, &nrl2, r);
            r = G_RNEXT(r);
        }
        FREES(g_remo + 2, 0);
    }
    mainend = nrl;
    i = 0;
    do {
        r = FREE(rlist + i);
        if (G_RKIND(r) == KOP)
            if (G_ROP(r) == OENTRY)
                if (mainend == nrl)
                    mainend = i;
        i = i + 1;
    } while (i <= nrl - 1);
    start = -1;
    i = 0;
    do {
        r = FREE(rlist + i);
        if (G_RKIND(r) == KOP)
            if (G_ROP(r) == OENTRY) {
                if (start >= 0)
                    xscan(start, i);
                else {
                    if (i > 0)
                        xscan(0, i);
                }
                start = i;
            }
        i = i + 1;
    } while (i <= nrl - 1);
    if (start >= 0)
        xscan(start, nrl);
    else {
        if (nrl > 0)
            xscan(0, nrl);
    }
    start = -1;
    i = 0;
    do {
        r = FREE(rlist + i);
        if (G_RKIND(r) == KOP)
            if (G_ROP(r) == OENTRY) {
                if (start >= 0)
                    efunc(start, i, 0);
                else {
                    if (i > 0)
                        efunc(0, i, 1);
                }
                start = i;
            }
        i = i + 1;
    } while (i <= nrl - 1);
    if (start >= 0)
        efunc(start, nrl, 0);
    else {
        if (nrl > 0)
            efunc(0, nrl, 1);
    }
    edata();
    fflush(NULL);                                       /* CCALL fflush(0) */
    return 0;
}
