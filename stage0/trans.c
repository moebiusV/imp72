/*
 * trans.c -- from PDP-10 code to pcc IR.
 *
 * Replaces the assembler, register allocator and output module of the
 * original compiler (amod.imc, rmod.imc, omod.imc), which assemble the code
 * chains the semantic routines of codegen.c make into PDP-10 object code.
 * Here each chain is read as a PDP-10 program over virtual registers and
 * written as pcc IR (ir.h), which pcc compiles for the machine at hand.  The
 * meaning of an instruction is the PDP-10's, with these changes for a 64 bit
 * machine with byte addresses:
 *
 *  - a word is 64 bits; a halfword (HRRZ, HLL, ...) is 32 bits;
 *  - an address that names a variable is its byte address; the constant and
 *    the index register of an instruction addressing a variable count words
 *    (so V[I] is at &V + 8*I); with no variable, they count bytes ([P+3] is
 *    P+3 bytes);
 *  - the argument words after a JSA are pointers (IMP, like FORTRAN, passes
 *    its arguments by reference), and a subroutine is a C function taking
 *    them as pointers: `@k(R15)` in the body is parameter k;
 *  - a subroutine returns the value in the register moved just before its JRA.
 *
 * Not translated yet: byte pointer instructions, BLT and XCT, double length
 * arithmetic, floating point, UUOs, and code outside subroutines.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "imp.h"
#include "ir.h"

static FILE *irfile;
static int unsupported;

void
trans_open(FILE *f)
{
    irfile = f;
    ir_open(f);
}

static void
tfail(const char *fmt, ...)
{
    char buf[200];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    unsupported = 1;
    imp_error(1, buf);
}

/* ---- the records of a chain ---------------------------------------------- */

enum { K_INSN, K_TAG, K_SPEC, K_CONST };

struct ti {
    int kind;
    word op, ac, ix, v, c, fn;
    int ind, ra;
};

static struct ti *tis;
static long nti, cti;

static struct ti *
newti(void)
{
    if (nti == cti) {
        struct ti *t = realloc(tis, (size_t)(cti = cti ? cti * 2 : 256) * sizeof *t);

        if (t == NULL) {
            imp_error(0, "OUT OF MEMORY IN TRANS. ");
            exit(1);
        }
        tis = t;
    }
    memset(&tis[nti], 0, sizeof tis[nti]);
    return &tis[nti++];
}

static void
collect(word iw)
{
    word w, w2;
    struct ti *t;

    nti = 0;
    for (;;) {
        w = FREE(co + iw);
        switch (u36(w) >> 34) {
        case 2:
            if ((iw = w & MASK18) == 0)
                return;
            continue;
        case 1:
            t = newti();
            t->fn = 0177 & (u36(w) >> 27);
            t->v = w & MASK18;
            t->kind = t->fn == 0 ? K_TAG : K_SPEC;
            iw++;
            continue;
        case 3:
            t = newti();
            t->kind = K_CONST;
            t->v = w & MASK18;
            t->c = FREE(co + iw + 1);
            iw += 2;
            continue;
        default:
            break;
        }
        w2 = FREE(co + iw + 1);
        t = newti();
        t->kind = K_INSN;
        t->op = 0777 & (u36(w) >> 24);
        t->ac = 03777 & (u36(w) >> 12);
        t->ix = 03777 & u36(w);
        t->ind = (u36(w) >> 33) & 1;
        t->ra = (u36(w) >> 11) & 1;
        t->v = w2 & MASK18;
        t->c = L18(w2);
        iw += 2;
    }
}

/* ---- names, variables, constants ----------------------------------------- */

static char **symcache;
static word nsymcache;

/* The assembler name of directory entry v: lower case, characters an
 * assembler would not take as _xx. */
static const char *
symname(word v)
{
    char buf[128], out[300];
    size_t i, k = 0;

    if (v >= nsymcache) {
        word n = v + 64;

        symcache = realloc(symcache, (size_t)n * sizeof *symcache);
        memset(symcache + nsymcache, 0, (size_t)(n - nsymcache) * sizeof *symcache);
        nsymcache = n;
    }
    if (symcache[v] != NULL)
        return symcache[v];
    dname_str(v, buf, sizeof buf);
    for (i = 0; buf[i] && k + 4 < sizeof out; i++) {
        unsigned char ch = (unsigned char)buf[i];

        if (ch >= 'A' && ch <= 'Z')
            out[k++] = (char)(ch + 'a' - 'A');
        else if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_')
            out[k++] = (char)ch;
        else
            k += (size_t)sprintf(out + k, "_%02x", ch);
    }
    out[k] = '\0';
    symcache[v] = malloc(k + 1);
    memcpy(symcache[v], out, k + 1);
    return symcache[v];
}

/* Variables used by the unit, and what the unit defines. */
struct var {
    word v;
    long words;                         /* 0: not defined here (scalar) */
    struct var *next;
};
static struct var *vars;

/* DATA statements: a tag followed by constant words in the code */
struct datadef {
    word v;
    long n;
    long long *w;
    struct datadef *next;
};
static struct datadef *datadefs;

/* the subroutines defined in this unit: not variables */
static word fnames[512];
static int nfnames;

static void
usevar(word v)
{
    struct var *p;

    for (p = vars; p != NULL; p = p->next)
        if (p->v == v)
            return;
    p = calloc(1, sizeof *p);
    p->v = v;
    p->next = vars;
    vars = p;
}

/* Constants used as addresses: a pool of words and of strings. */
struct pool {
    word v;
    int label;
    struct pool *next;
};
static struct pool *pools;

static int
poolof(word v)
{
    struct pool *p;

    for (p = pools; p != NULL; p = p->next)
        if (p->v == v)
            return p->label;
    p = calloc(1, sizeof *p);
    p->v = v;
    p->label = ir_newlabel();
    p->next = pools;
    pools = p;
    return p->label;
}

static int
isnumconst(word v, word *val)
{
    char buf[40];

    if (v == 0)
        return 0;
    dname_str(v, buf, sizeof buf);
    if (buf[0] < '0' || buf[0] > '9')
        return 0;
    if (convc(v) != -1)
        return 0;
    *val = convc_const;
    return 1;
}

static int
isstring(word v)
{
    char buf[4];

    dname_str(v, buf, sizeof buf);
    return buf[0] == '\'';
}

/* The value of a text constant: its first 8 characters, the first in the low
 * byte, so that the word is the start of the string in memory. */
static long long
strvalue(word v)
{
    char buf[40];
    unsigned long long x = 0;
    int k;

    dname_str(v, buf, sizeof buf);
    for (k = 1; k < 9 && buf[k]; k++)
        x |= (unsigned long long)(unsigned char)buf[k] << (8 * (k - 1));
    return (long long)x;
}

/* ---- per subroutine state ------------------------------------------------ */

/* BLT's AC holds source,,destination; with 32-bit halves that cannot hold a
 * pointer, so HRLI/HRRI also leave the whole address in a shadow temp */
static int bltsrc[2048], bltdst[2048];
static int *regtmp;
static word nregtmp;
static int paramtmp[16];
#define MAXPARAMS 16
static int nparams;
static int retlab, rettmp;
static struct { word tag; int label, defined; } *tags;
static long ntags, ctags;

static int
regtemp(word r)
{
    if (r >= nregtmp) {
        word n = r + 64;

        regtmp = realloc(regtmp, (size_t)n * sizeof *regtmp);
        memset(regtmp + nregtmp, 0, (size_t)(n - nregtmp) * sizeof *regtmp);
        nregtmp = n;
    }
    if (regtmp[r] == 0)
        regtmp[r] = ir_newtemp();
    return regtmp[r];
}

static struct irn *
rv(word r)
{
    return ir_temp(regtemp(r), T_LONG);
}

static int
taglabel(word tag)
{
    long i;

    for (i = 0; i < ntags; i++)
        if (tags[i].tag == tag)
            return tags[i].label;
    if (ntags == ctags)
        tags = realloc(tags, (size_t)(ctags = ctags ? ctags * 2 : 32) * sizeof *tags);
    tags[ntags].tag = tag;
    tags[ntags].label = ir_newlabel();
    tags[ntags].defined = 0;
    return tags[ntags++].label;
}

#define LINKREG 15

static struct irn *
icon(long long v)
{
    return ir_icon(v, T_LONG);
}

/*
 * The effective address of an instruction, as a byte address: variable v
 * (if any) plus c words, plus the index register (words if there is a
 * variable, else bytes); an indirect address through the link register
 * is a parameter.
 */
static struct irn *
effaddr(const struct ti *t)
{
    struct irn *e;
    word cc = t->c;

    if (t->ra) {
        tfail("TRANS: ADDRESS OF A REGISTER");
        return icon(0);
    }
    if (t->ind) {
        if (t->v == 0 && t->ix == LINKREG && cc < nparams)
            return ir_temp(paramtmp[cc], T_PLONG);
        {
            /* the word at the address holds a (byte) address */
            struct ti u = *t;

            u.ind = 0;
            return ir_un(IR_PCONV, T_PLONG, ir_deref(effaddr(&u), T_LONG));
        }
    }
    if (t->v) {
        word cv;

        if (isstring(t->v) || isnumconst(t->v, &cv)) {
            int l = poolof(t->v);
            char *nm = malloc(24);

            sprintf(nm, ".L%d", l);
            e = ir_sym(nm, 8 * cc, T_PLONG);
        } else {
            usevar(t->v);
            e = ir_sym(symname(t->v), 8 * cc, T_PLONG);
        }
        if (t->ix)
            e = ir_bin(IR_PLUS, T_PLONG, e,
                ir_bin(IR_LS, T_LONG, rv(t->ix), ir_icon(3, T_INT)));
        return e;
    }
    if (cc & 0400000)
        cc -= 01000000;
    if (t->ix) {                                /* c words from the pointer in the register */
        e = ir_bin(IR_PLUS, T_PLONG, ir_icon(8 * (long long)cc, T_PLONG), rv(t->ix));
        return e;
    }
    return ir_icon(cc, T_PLONG);
}

static struct bytedesc *bytedescof(word v);
static struct irn *bytepvalue(const struct bytedesc *d);

/* the value of the operand of an instruction (the word at the address) */
static struct irn *
opvalue(const struct ti *t)
{
    word cv;

    if (t->ra)
        return rv(t->v);
    if (t->v && !t->ind && !t->ix && bytedescof(t->v))
        return bytepvalue(bytedescof(t->v));
    if (t->v && !t->ind && !t->ix && isstring(t->v))
        return icon(strvalue(t->v));
    if (t->v && !t->ind && !t->ix && isnumconst(t->v, &cv))
        return icon(cv);
    if (t->v && !t->ind && !t->ix && !isstring(t->v)) {
        usevar(t->v);
        return ir_name(symname(t->v), 8 * t->c, T_LONG);
    }
    return ir_deref(effaddr(t), T_LONG);
}

/* the operand as a place to store into */
static struct irn *
oplvalue(const struct ti *t)
{
    return opvalue(t);
}

/* the immediate form: the effective address itself as a number */
static struct irn *
immvalue(const struct ti *t)
{
    struct irn *e;

    if (t->v == 0 && !t->ix && !t->ind && !t->ra)
        return icon(t->c);
    if (!t->v && !t->ind && !t->ra) {           /* pure arithmetic: keep it in words */
        word cc = t->c;

        if (cc & 0400000)
            cc -= 01000000;
        return ir_bin(IR_PLUS, T_LONG, icon(8 * (long long)cc), rv(t->ix));
    }
    e = effaddr(t);
    return ir_un(IR_PCONV, T_LONG, e);
}


/* ---- byte fields --------------------------------------------------------- */

static void asg(struct irn *l, struct irn *r);

/*
 * Shifts for the field code.  pcc handles a shift of a temp by an int count
 * assigned back to the same temp, so every shift goes through one: the result
 * is a fresh temp node.  (A constant on the left, or a shift whose operand
 * is not the destination, is not code pcc can generate.)
 */
static struct irn *
shiftop(int right, struct irn *l, struct irn *r)
{
    int c = ir_newtemp();
    struct irn *cnt;

    if (r->op == IR_ICON)
        cnt = ir_icon(r->lval, T_INT);
    else {
        int k = ir_newtemp();

        asg(ir_temp(k, T_LONG), r);
        cnt = ir_un(IR_SCONV, T_UCHAR, ir_temp(k, T_LONG));  /* pcc: counts are chars */
    }
    asg(ir_temp(c, T_LONG), l);
    if (right)
        asg(ir_temp(c, T_LONG), ir_un(IR_SCONV, T_LONG, ir_bin(IR_RS, T_ULONG,
            ir_un(IR_SCONV, T_ULONG, ir_temp(c, T_LONG)), cnt)));
    else
        asg(ir_temp(c, T_LONG), ir_bin(IR_LS, T_LONG, ir_temp(c, T_LONG), cnt));
    return ir_temp(c, T_LONG);
}
#define SHL(l, r) shiftop(0, (l), (r))
#define SHR(l, r) shiftop(1, (l), (r))

/* The instruction a BYTEP descriptor was made from, decoded like collect(). */
static int
bytefield(const struct ti *t, struct bytedesc **dp, struct ti *u)
{
    struct bytedesc *d;
    word w;

    if (t->ra || t->ind || t->ix || (d = bytedescof(t->v)) == NULL)
        return 0;
    w = d->a0;
    memset(u, 0, sizeof *u);
    u->kind = K_INSN;
    u->op = 0777 & (u36(w) >> 24);
    u->ac = 03777 & (u36(w) >> 12);
    u->ix = 03777 & u36(w);
    u->ind = (u36(w) >> 33) & 1;
    u->ra = (u36(w) >> 11) & 1;
    u->v = d->a1 & MASK18;
    u->c = L18(d->a1);
    *dp = d;
    return 1;
}

static struct irn *
fieldpos(const struct bytedesc *d)
{
    return d->pconst ? icon(d->p) : rv(d->p);
}

static struct irn *
fieldmask(const struct bytedesc *d)
{
    if (d->sconst)
        return icon(d->s >= 63 ? -1LL : (1LL << d->s) - 1);
    return ir_bin(IR_MINUS, T_LONG,
        SHL(icon(1), rv(d->s)), icon(1));
}

/*
 * A byte pointer held in a variable or register is one word: bits 63..58 the
 * size, 57..52 the position (bits from the right, as in A<S,P>), and the low
 * 52 bits the byte address of the word that holds the field.  Incrementing
 * moves to the next lower field of the same size, or to the next word
 * (address + 8) when there is no room, as <+X> does on the PDP-10 with 64
 * bits in the word in place of 36.
 */
#define BPADDR 0xFFFFFFFFFFFFFLL

static struct bytedesc *
bytedescof(word v)
{
    int i;

    for (i = 0; i < nbytedescs; i++)
        if (bytedescs[i].name == v)
            return &bytedescs[i];
    return NULL;
}

static void
decodeop(const struct bytedesc *d, struct ti *u)
{
    word w = d->a0;

    memset(u, 0, sizeof *u);
    u->kind = K_INSN;
    u->op = 0777 & (u36(w) >> 24);
    u->ac = 03777 & (u36(w) >> 12);
    u->ix = 03777 & u36(w);
    u->ind = (u36(w) >> 33) & 1;
    u->ra = (u36(w) >> 11) & 1;
    u->v = d->a1 & MASK18;
    u->c = L18(d->a1);
}

static struct irn *
bytepvalue(const struct bytedesc *d)
{
    struct ti u;
    struct irn *a;

    decodeop(d, &u);
    a = ir_un(IR_PCONV, T_LONG, effaddr(&u));
    a = ir_bin(IR_OR, T_LONG, a, SHL(d->sconst ? icon(d->s) : rv(d->s), icon(58)));
    return ir_bin(IR_OR, T_LONG, a, SHL(d->pconst ? icon(d->p) : rv(d->p), icon(52)));
}

/* a value kept in a temp: pcc cannot have two variable shifts in one tree */
static int
tmpof(struct irn *e)
{
    int c = ir_newtemp();

    asg(ir_temp(c, T_LONG), e);
    return c;
}

/* IR nodes are freed with their statement, so each use needs its own */
#define TMP(c) ir_temp((c), T_LONG)

static struct irn *
bp_size(int pt)
{
    return ir_bin(IR_AND, T_LONG, SHR(ir_temp(pt, T_LONG), icon(58)), icon(63));
}

static struct irn *
bp_pos(int pt)
{
    return ir_bin(IR_AND, T_LONG, SHR(ir_temp(pt, T_LONG), icon(52)), icon(63));
}

static struct irn *
bp_word(int pt)
{
    return ir_deref(ir_un(IR_PCONV, T_PLONG,
        ir_bin(IR_AND, T_LONG, ir_temp(pt, T_LONG), icon(BPADDR))), T_LONG);
}

static struct irn *
bp_mask(int pt)
{
    return ir_bin(IR_MINUS, T_LONG, SHL(icon(1), bp_size(pt)), icon(1));
}

/* IBP: step the pointer in the operand to the next field */
static int
bp_step(const struct ti *t)
{
    int pt = ir_newtemp(), np = ir_newtemp(), lok = ir_newlabel();

    asg(ir_temp(pt, T_LONG), opvalue(t));
    asg(ir_temp(np, T_LONG), ir_bin(IR_MINUS, T_LONG, bp_pos(pt), bp_size(pt)));
    ir_cbranch(IR_GE, ir_temp(np, T_LONG), icon(0), lok);
    asg(ir_temp(np, T_LONG), ir_bin(IR_MINUS, T_LONG, icon(64), bp_size(pt)));
    asg(ir_temp(pt, T_LONG), ir_bin(IR_PLUS, T_LONG, ir_temp(pt, T_LONG), icon(8)));
    ir_label(lok);
    asg(ir_temp(pt, T_LONG), ir_bin(IR_OR, T_LONG,
        ir_bin(IR_AND, T_LONG, ir_temp(pt, T_LONG), icon(~(63LL << 52))),
        SHL(ir_temp(np, T_LONG), icon(52))));
    asg(oplvalue(t), ir_temp(pt, T_LONG));
    return pt;
}

/* LDB/ILDB through a pointer in the operand */
static void
ldb_dyn(const struct ti *t, int step)
{
    int pt;

    if (step)
        pt = bp_step(t);
    else {
        pt = ir_newtemp();
        asg(ir_temp(pt, T_LONG), opvalue(t));
    }
    {
        int w = tmpof(bp_word(pt)), p = tmpof(bp_pos(pt)), m = tmpof(bp_mask(pt));

        asg(rv(t->ac), ir_bin(IR_AND, T_LONG, SHR(TMP(w), TMP(p)), TMP(m)));
    }
}

/* DPB/IDPB through a pointer in the operand */
static void
dpb_dyn(const struct ti *t, int step)
{
    int pt;

    if (step)
        pt = bp_step(t);
    else {
        pt = ir_newtemp();
        asg(ir_temp(pt, T_LONG), opvalue(t));
    }
    {
        int p = tmpof(bp_pos(pt)), k = tmpof(bp_mask(pt)), m, v;

        m = tmpof(SHL(TMP(k), TMP(p)));
        v = tmpof(SHL(rv(t->ac), TMP(p)));
        asg(bp_word(pt), ir_bin(IR_OR, T_LONG,
            ir_bin(IR_AND, T_LONG, bp_word(pt), ir_un(IR_COMPL, T_LONG, TMP(m))),
            ir_bin(IR_AND, T_LONG, TMP(v), TMP(m))));
    }
}

/* LDB ac,field: the field, right justified */
static void
ldb(const struct ti *t)
{
    struct bytedesc *d;
    struct ti u;

    if (!bytefield(t, &d, &u)) {
        ldb_dyn(t, 0);
        return;
    }
    {
        int w = tmpof(opvalue(&u)), p = tmpof(fieldpos(d)), m = tmpof(fieldmask(d));

        asg(rv(t->ac), ir_bin(IR_AND, T_LONG, SHR(TMP(w), TMP(p)), TMP(m)));
    }
}

/* DPB ac,field: store the low bits of ac into the field */
static void
dpb(const struct ti *t)
{
    struct bytedesc *d;
    struct ti u;
    struct irn *old;

    if (!bytefield(t, &d, &u)) {
        dpb_dyn(t, 0);
        return;
    }
    {
        int p = tmpof(fieldpos(d)), k = tmpof(fieldmask(d)), m, v;

        m = tmpof(SHL(TMP(k), TMP(p)));
        v = tmpof(SHL(rv(t->ac), TMP(p)));
        old = ir_bin(IR_AND, T_LONG, opvalue(&u), ir_un(IR_COMPL, T_LONG, TMP(m)));
        asg(oplvalue(&u), ir_bin(IR_OR, T_LONG, old,
            ir_bin(IR_AND, T_LONG, TMP(v), TMP(m))));
    }
}

/* ---- operations ---------------------------------------------------------- */

#define HMASK 0xFFFFFFFFLL

static void
asg(struct irn *l, struct irn *r)
{
    ir_stmt(ir_assign(l, r));
}

static int
cmpop(int c)
{
    switch (c & 7) {
    case 1: return IR_LT;
    case 2: return IR_EQ;
    case 3: return IR_LE;
    case 5: return IR_GE;
    case 6: return IR_NE;
    case 7: return IR_GT;
    default: return 0;
    }
}

/* jump to label if (l cond r), cond as the low 3 bits of the opcode */
static void
jcond(int c, struct irn *l, struct irn *r, int label)
{
    if ((c & 7) == 0)                           /* never */
        return;
    if ((c & 7) == 4) {                         /* always */
        ir_goto(label);
        return;
    }
    ir_cbranch(cmpop(c), l, r, label);
}

static struct irn *
boolfn(int fn, struct irn *a, struct irn *b)
{
    switch (fn) {
    case 0: return icon(0);
    case 1: return ir_bin(IR_AND, T_LONG, a, b);
    case 2: return ir_bin(IR_AND, T_LONG, ir_un(IR_COMPL, T_LONG, a), b);
    case 3: return b;
    case 4: return ir_bin(IR_AND, T_LONG, a, ir_un(IR_COMPL, T_LONG, b));
    case 5: return a;
    case 6: return ir_bin(IR_ER, T_LONG, a, b);
    case 7: return ir_bin(IR_OR, T_LONG, a, b);
    case 8: return ir_un(IR_COMPL, T_LONG, ir_bin(IR_OR, T_LONG, a, b));
    case 9: return ir_un(IR_COMPL, T_LONG, ir_bin(IR_ER, T_LONG, a, b));
    case 10: return ir_un(IR_COMPL, T_LONG, a);
    case 11: return ir_bin(IR_OR, T_LONG, ir_un(IR_COMPL, T_LONG, a), b);
    case 12: return ir_un(IR_COMPL, T_LONG, b);
    case 13: return ir_bin(IR_OR, T_LONG, a, ir_un(IR_COMPL, T_LONG, b));
    case 14: return ir_un(IR_COMPL, T_LONG, ir_bin(IR_AND, T_LONG, a, b));
    default: return icon(-1);
    }
}

/* 0 basic (R <- R op m), 1 immediate (R <- R op Y), 2 to memory, 3 both */
static void
twoop(const struct ti *t, struct irn *(*f)(struct irn *, struct irn *), int mode)
{
    struct irn *res;

    switch (mode) {
    case 0:
        if (t->ac)
            asg(rv(t->ac), f(rv(t->ac), opvalue(t)));
        break;
    case 1:
        if (t->ac)
            asg(rv(t->ac), f(rv(t->ac), immvalue(t)));
        break;
    case 2:
        asg(oplvalue(t), f(t->ac ? rv(t->ac) : icon(0), opvalue(t)));
        break;
    default:
        res = f(t->ac ? rv(t->ac) : icon(0), opvalue(t));
        if (t->ac) {
            asg(rv(t->ac), res);
            asg(oplvalue(t), rv(t->ac));
        } else
            asg(oplvalue(t), res);
        break;
    }
}

static struct irn *fadd_(struct irn *a, struct irn *b) { return ir_bin(IR_PLUS, T_LONG, a, b); }
static struct irn *fsub_(struct irn *a, struct irn *b) { return ir_bin(IR_MINUS, T_LONG, a, b); }
static struct irn *fmul_(struct irn *a, struct irn *b) { return ir_bin(IR_MUL, T_LONG, a, b); }
static int bfn_;
static struct irn *fbool_(struct irn *a, struct irn *b) { return boolfn(bfn_, a, b); }

/* Shift r by the signed count in y: left if positive, else right. */
static void
shift(const struct ti *t, int kind)
{
    struct irn *y;
    int ty = kind == 1 ? T_LONG : T_ULONG;

    if (t->ac == 0)
        return;
    if (kind == 2)
        goto rot;
    if (t->v == 0 && !t->ix && !t->ind && !t->ra) {
        word y0 = t->c & 0400000 ? t->c - 01000000 : t->c;

        if (y0 >= 0)
            asg(rv(t->ac), ir_bin(IR_LS, T_LONG, rv(t->ac), ir_icon(y0, T_INT)));
        else
            asg(rv(t->ac), ir_un(IR_SCONV, T_LONG,
                ir_bin(IR_RS, ty, ir_un(IR_SCONV, ty, rv(t->ac)),
                ir_icon(-y0, T_INT))));
        return;
    }
    {
        int lpos = ir_newlabel(), ldone = ir_newlabel(), cnt = ir_newtemp();

        y = immvalue(t);
        asg(ir_temp(cnt, T_LONG), y);
        ir_cbranch(IR_GE, ir_temp(cnt, T_LONG), icon(0), lpos);
        asg(rv(t->ac), ir_un(IR_SCONV, T_LONG,
            ir_bin(IR_RS, ty, ir_un(IR_SCONV, ty, rv(t->ac)),
            ir_un(IR_SCONV, T_UCHAR, ir_un(IR_UMINUS, T_LONG, ir_temp(cnt, T_LONG))))));
        ir_goto(ldone);
        ir_label(lpos);
        asg(rv(t->ac), ir_bin(IR_LS, T_LONG, rv(t->ac),
            ir_un(IR_SCONV, T_UCHAR, ir_temp(cnt, T_LONG))));
        ir_label(ldone);
    }
    return;
rot:
    {
        int cnt = ir_newtemp(), cnt2 = ir_newtemp(), u1 = ir_newtemp(), u2 = ir_newtemp();

        asg(ir_temp(cnt, T_LONG), ir_bin(IR_AND, T_LONG, immvalue(t), icon(63)));
        asg(ir_temp(cnt2, T_LONG), ir_bin(IR_AND, T_LONG,
            ir_bin(IR_MINUS, T_LONG, icon(64), ir_temp(cnt, T_LONG)), icon(63)));
        asg(ir_temp(u1, T_ULONG), ir_un(IR_SCONV, T_ULONG, rv(t->ac)));
        asg(ir_temp(u2, T_ULONG), ir_temp(u1, T_ULONG));
        asg(ir_temp(u1, T_ULONG), ir_bin(IR_LS, T_ULONG, ir_temp(u1, T_ULONG),
            ir_un(IR_SCONV, T_UCHAR, ir_temp(cnt, T_LONG))));
        asg(ir_temp(u2, T_ULONG), ir_bin(IR_RS, T_ULONG, ir_temp(u2, T_ULONG),
            ir_un(IR_SCONV, T_UCHAR, ir_temp(cnt2, T_LONG))));
        asg(rv(t->ac), ir_un(IR_SCONV, T_LONG, ir_bin(IR_OR, T_ULONG,
            ir_temp(u1, T_ULONG), ir_temp(u2, T_ULONG))));
    }
}

/* %CALn: a word, tagged in the code, that receives a computed address. */
static int
iscal(word v)
{
    char buf[16];

    if (v == 0)
        return 0;
    dname_str(v, buf, sizeof buf);
    return strncmp(buf, "%CAL", 4) == 0;
}

/* the left / right halfword of x, as a number */
static struct irn *
lhalf(struct irn *x)
{
    return ir_bin(IR_AND, T_LONG,
        ir_bin(IR_RS, T_LONG, x, ir_icon(32, T_INT)), icon(HMASK));
}

static struct irn *
rhalf(struct irn *x)
{
    return ir_bin(IR_AND, T_LONG, x, icon(HMASK));
}

/*
 * The halfword group 500-577: dest.half <- src.half, the other half of the
 * destination unchanged (none), zeros (Z), ones (O) or the sign of the moved
 * half (E).  mode 0 basic (R <- from memory), 1 immediate (the source is 0,,Y),
 * 2 to memory (memory <- from R), 3 self.
 */
static void
halfword(const struct ti *t)
{
    int n = (int)((t->op - 0500) >> 2), mode = (int)(t->op & 3);

    /* HRRM into a %CAL word stores the whole address */
    if (t->op == 0542 && t->ac && iscal(t->v) && !t->ix && !t->ind && !t->ra) {
        usevar(t->v);
        asg(ir_name(symname(t->v), 0, T_LONG), rv(t->ac));
        return;
    }
    if ((t->op == 0505 || t->op == 0541) && t->ac && t->ac < 2048) {
        int *sh = t->op == 0505 ? bltsrc : bltdst;

        if (!sh[t->ac])
            sh[t->ac] = ir_newtemp();
        asg(ir_temp(sh[t->ac], T_LONG), immvalue(t));
    }
    int destleft = n < 8, srcleft;
    int ext = (n >> 1) & 3;
    struct irn *src, *dst, *moved, *keep;
    int cur = ir_newtemp();

    srcleft = (n & 1) ? !destleft : destleft;
    if (mode == 2) {                            /* memory <- R */
        src = t->ac ? rv(t->ac) : icon(0);
        dst = oplvalue(t);
    } else {
        src = mode == 1 ? immvalue(t) : opvalue(t);
        if (mode == 3 && t->ac == 0) {
            tfail("TRANS: SELF-MODE HALFWORD OPERATION WITHOUT AN ACCUMULATOR");
            return;
        }
        dst = t->ac ? rv(t->ac) : NULL;
        if (dst == NULL)
            return;
    }
    moved = srcleft ? lhalf(src) : rhalf(src);
    asg(ir_temp(cur, T_LONG), moved);
    if (ext == 0) {                             /* the other half stays */
        struct irn *old = mode == 2 ? oplvalue(t) : rv(t->ac);

        keep = destleft ? rhalf(old) : ir_bin(IR_AND, T_LONG, old,
            icon(~HMASK));
        moved = destleft ? ir_bin(IR_LS, T_LONG, ir_temp(cur, T_LONG),
            ir_icon(32, T_INT)) : ir_temp(cur, T_LONG);
        moved = ir_bin(IR_OR, T_LONG, moved, keep);
    } else {
        struct irn *other;

        if (ext == 1)
            other = icon(0);
        else if (ext == 2)
            other = icon(destleft ? HMASK : ~HMASK);
        else                                    /* E: the sign of the half */
            other = ir_bin(IR_AND, T_LONG,
                ir_bin(IR_RS, T_LONG,
                ir_bin(IR_LS, T_LONG, ir_temp(cur, T_LONG),
                    ir_icon(destleft ? 0 : 32, T_INT)),
                ir_icon(63, T_INT)),
                icon(destleft ? HMASK : ~HMASK));
        if (destleft)
            moved = ir_bin(IR_OR, T_LONG,
                ir_bin(IR_LS, T_LONG, ir_temp(cur, T_LONG), ir_icon(32, T_INT)),
                other);
        else
            moved = ir_bin(IR_OR, T_LONG, ir_temp(cur, T_LONG), other);
    }
    asg(dst, moved);
    if (mode == 3)
        asg(oplvalue(t), rv(t->ac));
}


/* ---- floating point --------------------------------------------------------
 * A REAL is the 64 bits of an IEEE double kept in a word.  Registers are long
 * temps, so an operation goes through the scratch word imp_fs: store the
 * long, read it as a double, compute, store the double, read it as a long.
 */
static int usedfs;

static int
tod(struct irn *x)
{
    int c = ir_newtemp(), te = ir_newtemp(), ts = ir_newtemp();

    usedfs = 1;
    /* the word is the sign and magnitude of the double with the magnitude
     * negated for a negative number (as on the PDP-10): integer order and
     * integer negation are the real ones.  Back to the double's bits: */
    asg(ir_temp(te, T_LONG), x);
    asg(ir_temp(ts, T_LONG), ir_bin(IR_RS, T_LONG, ir_temp(te, T_LONG), ir_icon(63, T_INT)));
    asg(ir_name("imp_fs", 0, T_LONG), ir_bin(IR_OR, T_LONG,
        ir_bin(IR_MINUS, T_LONG, ir_bin(IR_ER, T_LONG, ir_temp(te, T_LONG), ir_temp(ts, T_LONG)),
            ir_temp(ts, T_LONG)),
        ir_bin(IR_AND, T_LONG, ir_temp(ts, T_LONG), icon((long long)0x8000000000000000ULL))));
    asg(ir_temp(c, T_DOUBLE), ir_name("imp_fs", 0, T_DOUBLE));
    return c;
}

/* the word for the double d: a statement leaves it in temps; the value is the
 * expression that combines them */
static struct irn *
fromd(struct irn *d)
{
    int tb = ir_newtemp(), ts = ir_newtemp();

    usedfs = 1;
    asg(ir_name("imp_fs", 0, T_DOUBLE), d);
    asg(ir_temp(tb, T_LONG), ir_name("imp_fs", 0, T_LONG));
    asg(ir_temp(ts, T_LONG), ir_bin(IR_RS, T_LONG, ir_temp(tb, T_LONG), ir_icon(63, T_INT)));
    /* The original's reals have a 27 bit mantissa: round the magnitude of the double to it
     * (to nearest, a tie away from zero, as the rounded floating point instructions do: add
     * half a unit and clear the 26 low bits of the 52 bit fraction). */
    {
        int tm = ir_newtemp();

        asg(ir_temp(tm, T_LONG), ir_bin(IR_AND, T_LONG, ir_temp(tb, T_LONG), icon(0x7FFFFFFFFFFFFFFFLL)));
        asg(ir_temp(tm, T_LONG), ir_bin(IR_AND, T_LONG,
            ir_bin(IR_PLUS, T_LONG, ir_temp(tm, T_LONG), icon(0x2000000LL)),
            icon(-67108864LL)));
        return ir_bin(IR_MINUS, T_LONG,
            ir_bin(IR_ER, T_LONG, ir_temp(tm, T_LONG), ir_temp(ts, T_LONG)),
            ir_temp(ts, T_LONG));
    }
}

/* FADR/FSBR/FMPR/FDVR (144, 154, 164, 174 + mode) */
static void
fpop(const struct ti *t)
{
    int mode = (int)(t->op & 3), a, b;
    int kind = (int)((t->op >> 3) & 7) - 4, ops[4] = { IR_PLUS, IR_MINUS, IR_MUL, IR_DIV };
    struct irn *r;

    if (mode == 1) {
        tfail("TRANS: IMMEDIATE FLOATING OPERAND IS NOT TRANSLATED");
        return;
    }
    a = tod(rv(t->ac));
    b = tod(opvalue(t));
    r = ir_bin(ops[kind], T_DOUBLE, ir_temp(a, T_DOUBLE), ir_temp(b, T_DOUBLE));
    if (mode == 0 || mode == 3)
        asg(rv(t->ac), fromd(r));
    else
        asg(oplvalue(t), fromd(r));
    if (mode == 3)
        asg(oplvalue(t), rv(t->ac));
}

/* FSC ac,233: the integer in ac made a floating number */
static void
fsc(const struct ti *t)
{
    if (t->c != 0233 || t->v || t->ix) {
        tfail("TRANS: FSC OTHER THAN THE INTEGER TO FLOATING IDIOM");
        return;
    }
    asg(rv(t->ac), fromd(ir_un(IR_SCONV, T_DOUBLE, rv(t->ac))));
}

/* ---- calls ---------------------------------------------------------------- */

#define argreg(k) (irtgt->arg[k])
#define NARGREG (irtgt->narg)


/* the routines of the runtime library, which return a C long, not a word */
static int
isruntime(const char *nm)
{
    return strcmp(nm, "ifix") == 0 || strcmp(nm, "fini") == 0 || strncmp(nm, "imp_", 4) == 0;
}

/*
 * A call of fn with the arguments a[0..n-1]: in registers and in the
 * outgoing stack area as the machine's procedure call standard wants, or
 * as FUNARG nodes that pass 2 pushes (i386).  A function of the runtime or
 * of C returns a C `long`: on i386 an int, which is widened to the word.
 */
static struct irn *
mkcall(struct irn *fn, struct irn **a, int n, int cret)
{
    struct irn *cm = NULL, *tree;
    int k, rt = T_LONG, qual = 0;

    if (irtgt->wordll)                                  /* a word argument: the C long */
        for (k = 0; k < n; k++)
            if (!(a[k]->type & T_PTR) && a[k]->type != T_INT)
                a[k] = ir_un(IR_SCONV, T_INT, a[k]);
    if (irtgt->fnargs) {
        for (k = 0; k < n; k++) {
            struct irn *v = a[k], *f;

            f = ir_un(IR_FUNARG, v->type, v);
            cm = cm == NULL ? f : ir_bin(IR_CM, T_INT, cm, f);
        }
        if (cret)
            rt = T_INT;
    } else {
        /* the arguments beyond the registers go in the outgoing stack
         * area, which pcc reserves for any assignment to memory in a call's
         * list; they come first, the register ones last */
        for (k = irtgt->narg; k < n && irtgt->pushargs; k++) {  /* pass 2 pushes from the right: the last first */
            struct irn *f = ir_un(IR_FUNARG, a[k]->type, a[k]);

            cm = cm == NULL ? f : ir_bin(IR_CM, T_INT, cm, f);
            qual += irtgt->argslot;
        }
        for (k = irtgt->narg; k < n && !irtgt->pushargs; k++) {
            struct irn *m = ir_un(IR_UMUL, a[k]->type, ir_bin(IR_PLUS, T_PLONG,
                ir_reg(R_RSP, T_PLONG), ir_icon(irtgt->argslot * (k - irtgt->narg), T_LONG)));
            struct irn *as = ir_assign(m, a[k]);

            cm = cm == NULL ? as : ir_bin(IR_CM, T_INT, cm, as);
        }
        for (k = 0; k < n && k < irtgt->narg; k++) {
            struct irn *as = ir_assign(ir_reg(argreg(k), a[k]->type), a[k]);

            cm = cm == NULL ? as : ir_bin(IR_CM, T_INT, cm, as);
        }
        if (cret && irtgt->wordll)
            rt = T_INT;
        if (cret && n > 0 && strcmp(irtgt->name, "amd64") == 0)   /* varargs: %al = vector registers used */
            cm = ir_bin(IR_CM, T_INT, cm, ir_assign(ir_reg(R_RAX, T_LONG), icon(0)));
    }
    tree = n > 0 ? ir_bin(IR_CALL, rt, fn, cm) : ir_un(IR_UCALL, rt, fn);
    tree->qual = qual;
    if (rt != T_LONG)
        tree = ir_un(IR_SCONV, T_LONG, tree);
    return tree;
}

/* the pointer an argument word stands for */
static struct irn *
argptr(const struct ti *t)
{
    struct ti u = *t;

    u.ix = 0;
    u.ac = 0;
    u.ra = 0;
    if (t->ind) {
        /* the word at tag v was set by an HRRM just before the call: a
         * pointer temp named for the tag */
        u.ind = 0;
        usevar(t->v);
        return ir_name(symname(t->v), 0, T_PLONG);
    }
    return effaddr(&u);
}

/*
 * A call of the print routine, PRN.: the words after it are a print list, each
 * N << 24 | offset plus the address of the variable named in it (of the %CAL
 * word that has been given an address).  It becomes a call of imp_pr(n, p)
 * for each item, a routine of the runtime library that keeps the format.
 */
static long
printcall(long i, int rd)
{
    long j = i + 1;
    int res = -1;
    struct irn *last = NULL;
    struct ti *tag = NULL;

    while (j < nti && (tis[j].kind == K_CONST || tis[j].kind == K_TAG)) {
        struct ti *a = &tis[j];
        word n, off;
        struct irn *p, *tree;

        j++;
        if (a->kind == K_TAG) {
            tag = a;                    /* a %CAL word follows: an HRRM has filled in its address */
            continue;
        }
        n = 077 & (u36(a->c) >> 24);
        off = a->c & 0777777;
        if (tag != NULL && a->v == 0 && iscal(tag->v)) {
            usevar(tag->v);
            p = ir_name(symname(tag->v), 0, T_PLONG);
        } else if (a->v == 0)
            p = icon(0);
        else if ((a->c & 020000000) || iscal(a->v)) {   /* the word holds the address */
            usevar(a->v);
            p = ir_name(symname(a->v), 0, T_PLONG);
        } else {
            struct ti u;

            memset(&u, 0, sizeof u);
            u.v = a->v;
            u.c = off;
            p = effaddr(&u);
        }
        tag = NULL;
        {
            struct irn *av[2];

            av[0] = icon(n);
            av[1] = p;
            tree = mkcall(ir_sym(rd ? "imp_rd" : "imp_pr", 0, T_PFTN), av, 2, 1);
        }
        if (last != NULL)
            ir_stmt(last);
        last = tree;
    }
    if (j < nti && tis[j].kind == K_SPEC && tis[j].fn == 2) {
        if (tis[j].v)
            res = regtemp(tis[j].v);
        j++;
    }
    if (last != NULL) {
        if (res >= 0)
            asg(ir_temp(res, T_LONG), last);
        else
            ir_stmt(last);
    }
    return j - 1;
}

/*
 * A call: JSA at index i, then the argument words (JUMP, op 320) and the
 * restore special function that names the register with the result.
 */
static long
call(long i)
{
    struct ti *t = &tis[i];
    const char *nm;
    int nargs = 0, res = -1, k, byval;
    long j = i + 1;
    struct irn *args[MAXPARAMS];
    struct irn *tree;

    if (t->v == 0 || t->ind || t->ix) {
        tfail("TRANS: CALL OF A COMPUTED SUBROUTINE");
        return i;
    }
    nm = symname(t->v);
    if (strcmp(nm, "prn_2e") == 0)
        return printcall(i, 0);
    if (strcmp(nm, "red_2e") == 0)
        return printcall(i, 1);
    while (j < nti) {
        struct ti *a = &tis[j];

        if (a->kind == K_TAG && iscal(a->v) && j + 1 < nti) {
            /* the argument word a HRRM has just filled in with an address */
            if (nargs >= MAXPARAMS) {
                tfail("TRANS: MORE THAN %d ARGUMENTS", MAXPARAMS);
                break;
            }
            usevar(a->v);
            args[nargs++] = ir_name(symname(a->v), 0, T_PLONG);
            j += 2;
            continue;
        }
        if (a->kind == K_INSN && a->op == 0320 && a->ac == 0) {
            if (nargs >= MAXPARAMS) {
                tfail("TRANS: MORE THAN %d ARGUMENTS", MAXPARAMS);
                break;
            }
            args[nargs++] = argptr(a);
            j++;
            continue;
        }
        break;
    }
    if (j < nti && tis[j].kind == K_SPEC && tis[j].fn == 2) {
        if (tis[j].v)
            res = regtemp(tis[j].v);
        j++;
    }
    tree = ir_sym(nm, 0, T_PFTN);
    byval = is_cval(t->v);
    {
        int q;

        for (q = 0; q < nfnames; q++)
            if (fnames[q] == t->v)
                break;
        if (q == nfnames && nfnames < 512)
            fnames[nfnames++] = t->v;       /* called: a function, not data */
    }
    if (byval)                                  /* C: the values, not the addresses */
        for (k = 0; k < nargs; k++)
            args[k] = ir_deref(args[k], T_LONG);
    {
        struct irn *av[MAXPARAMS];

        for (k = 0; k < nargs; k++)
            av[k] = args[k];
        if (byval)                      /* tell the C back end the callee is C's, with C's longs */
            ir_pass("\t# cfunc %s", nm);
        tree = mkcall(tree, av, nargs, byval || isruntime(nm));
    }
    if (res >= 0)
        asg(ir_temp(res, T_LONG), tree);
    else
        ir_stmt(tree);
    return j - 1;
}

/* ---- the translation ------------------------------------------------------ */

static int pendskip;                            /* label after the skipped insn */
static word lastmove;                           /* AC of the MOVE just before */

static void
skipnext(struct irn *cond_l, struct irn *cond_r, int c)
{
    int l = ir_newlabel();

    jcond(c, cond_l, cond_r, l);
    pendskip = l;
}

/* Translate instruction i; returns the index of the last record used. */
static long
insn(long i)
{
    struct ti *t = &tis[i];
    word op = t->op;
    int mode = (int)(op & 3), l;
    struct irn *x;

    if (op == 0) return i;                      /* the entry word */
    if (op >= 0200 && op <= 0217) {             /* MOVE, MOVS, MOVN, MOVM */
        int f = (int)((op - 0200) >> 2);
        struct irn *v;
        int tmp = ir_newtemp();

#define MV(src) do { \
        x = (src); \
        if (f == 0) v = x; \
        else if (f == 1) { /* swap the halves */ \
            asg(ir_temp(tmp, T_LONG), x); \
            v = ir_bin(IR_OR, T_LONG, \
                ir_bin(IR_LS, T_LONG, rhalf(ir_temp(tmp, T_LONG)), ir_icon(32, T_INT)), \
                lhalf(ir_temp(tmp, T_LONG))); } \
        else if (f == 2) v = ir_un(IR_UMINUS, T_LONG, x); \
        else { /* absolute value */ \
            asg(ir_temp(tmp, T_LONG), x); \
            l = ir_newlabel(); \
            v = ir_temp(tmp, T_LONG); \
            ir_cbranch(IR_GE, ir_temp(tmp, T_LONG), icon(0), l); \
            asg(ir_temp(tmp, T_LONG), ir_un(IR_UMINUS, T_LONG, ir_temp(tmp, T_LONG))); \
            ir_label(l); } } while (0)

        if (mode == 0) {
            if (t->ac) {
                MV(opvalue(t));
                asg(rv(t->ac), v);
            }
        } else if (mode == 1) {
            if (t->ac) {
                MV(immvalue(t));
                asg(rv(t->ac), v);
            }
        } else if (mode == 2) {
            MV(t->ac ? rv(t->ac) : icon(0));
            asg(oplvalue(t), v);
        } else {
            int res = ir_newtemp();

            MV(opvalue(t));
            asg(ir_temp(res, T_LONG), v);
            asg(oplvalue(t), ir_temp(res, T_LONG));
            if (t->ac)
                asg(rv(t->ac), ir_temp(res, T_LONG));
        }
        return i;
#undef MV
    }
    if (op >= 0220 && op <= 0223) {             /* IMUL */
        twoop(t, fmul_, mode);
        return i;
    }
    if (op >= 0230 && op <= 0233) {             /* IDIV: quotient, remainder */
        int d = ir_newtemp(), q = ir_newtemp(), r = ir_newtemp();

        if (t->ac == 0 && mode != 2) {
            tfail("TRANS: IDIV WITHOUT AN ACCUMULATOR");
            return i;
        }
        asg(ir_temp(d, T_LONG), mode == 1 ? immvalue(t) : opvalue(t));
        asg(ir_temp(q, T_LONG), ir_bin(IR_DIV, T_LONG, t->ac ? rv(t->ac) : icon(0),
            ir_temp(d, T_LONG)));
        asg(ir_temp(r, T_LONG), ir_bin(IR_MOD, T_LONG, t->ac ? rv(t->ac) : icon(0),
            ir_temp(d, T_LONG)));
        if (mode == 0 || mode == 1 || mode == 3) {
            /* AC+1 gets the remainder, but IDIV does not clobber it unless the
             * compiler reserved the pair; reusing it as another register is
             * what the original's code does */
            if (pairfirst(t->ac))
                asg(rv(t->ac + 1), ir_temp(r, T_LONG));
            asg(rv(t->ac), ir_temp(q, T_LONG));
        }
        if (mode == 2 || mode == 3)
            asg(oplvalue(t), ir_temp(q, T_LONG));
        return i;
    }
    if (op >= 0270 && op <= 0273) {
        twoop(t, fadd_, mode);
        return i;
    }
    if (op >= 0274 && op <= 0277) {
        twoop(t, fsub_, mode);
        return i;
    }
    if (op >= 0400 && op <= 0477) {
        bfn_ = (int)((op - 0400) >> 2);
        twoop(t, fbool_, mode);
        return i;
    }
    if (op >= 0500 && op <= 0577) {
        halfword(t);
        return i;
    }
    if (op == 0240 || op == 0242 || op == 0241) {
        shift(t, op == 0240 ? 1 : op == 0242 ? 0 : 2);
        return i;
    }
    if (op == 0132) {                           /* FSC */
        fsc(t);
        return i;
    }
    if (op >= 0144 && op <= 0177 && (op & 4)) {   /* FADR, FSBR, FMPR, FDVR */
        fpop(t);
        return i;
    }
    if (op == 0133) {                           /* IBP */
        bp_step(t);
        return i;
    }
    if (op == 0134) {                           /* ILDB */
        ldb_dyn(t, 1);
        return i;
    }
    if (op == 0136) {                           /* IDPB */
        dpb_dyn(t, 1);
        return i;
    }
    if (op == 0135) {                           /* LDB */
        ldb(t);
        return i;
    }
    if (op == 0137) {                           /* DPB */
        dpb(t);
        return i;
    }
    if (op == 0251) {                           /* BLT: imp_blt(dst, src, bytes) */
        struct irn *tree;

        if (!t->ac || t->ac >= 2048 || !bltsrc[t->ac] || !bltdst[t->ac]) {
            tfail("TRANS: BLT WITHOUT HRLI/HRRI SETUP");
            return i;
        }
        {
            struct irn *av[3];

            av[0] = ir_temp(bltdst[t->ac], T_LONG);
            av[1] = ir_temp(bltsrc[t->ac], T_LONG);
            av[2] = ir_bin(IR_PLUS, T_LONG,
                ir_bin(IR_MINUS, T_LONG, immvalue(t), ir_temp(bltdst[t->ac], T_LONG)),
                icon(8));
            tree = mkcall(ir_sym("imp_blt", 0, T_PFTN), av, 3, 1);
        }
        ir_stmt(tree);
        return i;
    }
    if (op == 0250) {                           /* EXCH */
        int tmp = ir_newtemp();

        asg(ir_temp(tmp, T_LONG), opvalue(t));
        asg(oplvalue(t), rv(t->ac));
        asg(rv(t->ac), ir_temp(tmp, T_LONG));
        return i;
    }
    if (op == 0254) {                           /* JRST */
        if (t->v && !t->ind && t->ix) {         /* GO TO (L0,L1,...) N: JRST table(N) */
            int j, k;

            for (j = 0; j < nti; j++)
                if (tis[j].kind == K_TAG && tis[j].v == t->v)
                    break;
            if (j == nti) {
                tfail("TRANS: JUMP TABLE NOT FOUND");
                return i;
            }
            for (k = 0, j++; j < nti && tis[j].kind == K_INSN && tis[j].op == 0254 &&
                tis[j].v && !tis[j].ind && !tis[j].ix; j++, k++)
                ir_cbranch(IR_EQ, rv(t->ix), icon(k), taglabel(tis[j].v));
            return i;                           /* out of range: falls into the table's jumps */
        }
        if (t->v == 0 || t->ind || t->ix) {
            tfail("TRANS: JUMP TO A COMPUTED ADDRESS");
            return i;
        }
        ir_goto(taglabel(t->v));
        return i;
    }
    if (op == 0255)                             /* JFCL */
        return i;
    if (op == 0047) {                           /* CALLI: a monitor call */
        struct irn *r = t->ac ? rv(t->ac) : icon(0);
        struct irn *av[2], *tree;

        av[0] = immvalue(t);
        av[1] = r;
        tree = mkcall(ir_sym("imp_calli", 0, T_PFTN), av, 2, 1);

        if (t->ac)
            asg(rv(t->ac), tree);
        else
            ir_stmt(tree);
        return i;
    }
    if (op == 0266)
        return call(i);
    if (op == 0267) {                           /* JRA: return */
        if (lastmove > 0)
            asg(ir_temp(rettmp, T_LONG), rv(lastmove));
        ir_goto(retlab);
        return i;
    }
    if (op >= 0300 && op <= 0317) {             /* CAI, CAM: compare, skip */
        skipnext(rv(t->ac), (op & 010) ? opvalue(t) : immvalue(t), (int)(op & 7));
        return i;
    }
    if (op >= 0320 && op <= 0327) {             /* JUMP */
        if (t->v == 0) {
            tfail("TRANS: JUMP TO A COMPUTED ADDRESS");
            return i;
        }
        jcond((int)(op & 7), rv(t->ac), icon(0), taglabel(t->v));
        return i;
    }
    if (op >= 0330 && op <= 0337) {             /* SKIP */
        int tmp = ir_newtemp();

        asg(ir_temp(tmp, T_LONG), opvalue(t));
        if (t->ac)
            asg(rv(t->ac), ir_temp(tmp, T_LONG));
        skipnext(ir_temp(tmp, T_LONG), icon(0), (int)(op & 7));
        return i;
    }
    if (op >= 0340 && op <= 0377) {             /* AOJ AOS SOJ SOS */
        int dec = op >= 0360, inmem = (op & 010) != 0, tmp = ir_newtemp();
        struct irn *cur = inmem ? opvalue(t) : rv(t->ac);

        asg(ir_temp(tmp, T_LONG), ir_bin(dec ? IR_MINUS : IR_PLUS, T_LONG, cur, icon(1)));
        if (inmem) {
            asg(oplvalue(t), ir_temp(tmp, T_LONG));
            if (t->ac)
                asg(rv(t->ac), ir_temp(tmp, T_LONG));
            skipnext(ir_temp(tmp, T_LONG), icon(0), (int)(op & 7));
        } else {
            asg(rv(t->ac), ir_temp(tmp, T_LONG));
            if (t->v == 0) {
                tfail("TRANS: JUMP TO A COMPUTED ADDRESS");
                return i;
            }
            jcond((int)(op & 7), ir_temp(tmp, T_LONG), icon(0), taglabel(t->v));
        }
        return i;
    }
    tfail("TRANS: INSTRUCTION %03lo IS NOT YET TRANSLATED", (long)op);
    return i;
}

/* A subroutine: records [i0, i1). */
static void
subr(long i0, long i1, int ismain)
{
    long i;
    word name = ismain ? 0 : tis[i0].v;
    int k;
    word maxp = 0;

    /* how many parameters: the JRA offsets and the @k(R15) references */
    nparams = 0;
    for (i = i0; i < i1; i++) {
        if (tis[i].kind != K_INSN)
            continue;
        if (tis[i].ind && tis[i].v == 0 && tis[i].ix == LINKREG && tis[i].c + 1 > maxp)
            maxp = tis[i].c + 1;
        if (tis[i].op == 0267 && tis[i].c > maxp)
            maxp = tis[i].c;
    }
    nparams = ismain ? 0 : (int)maxp;
    if (nparams > MAXPARAMS) {
        tfail("TRANS: SUBROUTINE %s HAS MORE THAN %d PARAMETERS", symname(name), MAXPARAMS);
        return;
    }
    memset(regtmp, 0, (size_t)nregtmp * sizeof *regtmp);
    memset(bltsrc, 0, sizeof bltsrc);
    memset(bltdst, 0, sizeof bltdst);
    ntags = 0;
    pendskip = 0;
    if (!ismain && nfnames < 512)
        fnames[nfnames++] = name;
    ir_func_begin(ismain ? "imp_main" : symname(name), 1);
    retlab = ir_newlabel();
    for (k = 0; k < nparams; k++) {
        paramtmp[k] = ir_newtemp();
        if (k < NARGREG)
            asg(ir_temp(paramtmp[k], T_PLONG), ir_reg(argreg(k), T_PLONG));
        else                                    /* above the return address and saved rbp */
            asg(ir_temp(paramtmp[k], T_PLONG), ir_un(IR_UMUL, T_PLONG,
                ir_bin(IR_PLUS, T_PLONG, ir_reg(R_RBP, T_PLONG), ir_icon(irtgt->stackarg0 + irtgt->argslot * (k - NARGREG), T_LONG))));
    }
    rettmp = ir_newtemp();
    lastmove = 0;
    for (i = i0; i < i1; i++) {
        struct ti *t = &tis[i];

        switch (t->kind) {
        case K_TAG:
            if (i + 1 < i1 && tis[i + 1].kind == K_CONST) {   /* DATA: not code */
                struct datadef *d = calloc(1, sizeof *d);

                d->v = t->v;
                for (i++; i < i1 && tis[i].kind == K_CONST; i++) {
                    if (tis[i].v)
                        tfail("TRANS: ADDRESS CONSTANT IN DATA");
                    d->w = realloc(d->w, (size_t)(d->n + 1) * sizeof *d->w);
                    d->w[d->n++] = (long long)tis[i].c;
                }
                i--;
                d->next = datadefs;
                datadefs = d;
                break;
            }
            if (ismain || t->v != name) {
                int lb = taglabel(t->v), q;

                for (q = 0; q < ntags; q++)
                    if (tags[q].label == lb)
                        tags[q].defined = 1;
                ir_label(lb);
            }
            break;
        case K_SPEC:
            break;
        case K_CONST:
            tfail("TRANS: DATA IN THE CODE");
            break;
        default: {
            int skipped = pendskip;
            word op = t->op;

            pendskip = 0;
            i = insn(i);
            if (skipped)                        /* the skipped instruction is done */
                ir_label(skipped);
            lastmove = (op == 0200 || op == 0201 || op == 0202) ? t->ac : 0;
            break;
            }
        }
    }
    if (pendskip)
        ir_label(pendskip);
    {
        long q;

        for (q = 0; q < ntags; q++)
            if (!tags[q].defined)
                tfail("TRANS: %s: JUMP TO %s, WHICH IS NOT IN THE SUBROUTINE",
                    ismain ? "main" : symname(name), symname(tags[q].tag));
    }
    ir_func_end(retlab, rettmp);
}

/* ---- a unit --------------------------------------------------------------- */

static void
emit_data(void)
{
    struct var *v;
    struct pool *p;
    word i, n, nm, cv;
    long sz;
    char buf[400];

    if (usedfs)
        ir_pass("\t.comm imp_fs,8,8");
    for (v = vars; v != NULL; v = v->next) {
        struct datadef *d;

        for (d = datadefs; d != NULL; d = d->next)
            if (d->v == v->v)
                break;
        if (d != NULL)
            continue;
        {
            int q;

            for (q = 0; q < nfnames; q++)
                if (fnames[q] == v->v)
                    break;
            if (q < nfnames)
                continue;
        }
        sz = 8;
        n = impsem_narr();
        for (i = 0; i < n; i++) {
            word w;

            nm = impsem_array(i, &w);
            if (nm == v->v)
                sz = 8 * (long)w;
        }
        ir_pass("\t.comm %s,%ld,8", symname(v->v), sz);
    }
    {
        struct datadef *d;
        long k;

        for (d = datadefs; d != NULL; d = d->next) {
            ir_pass("\t.data");
            ir_pass("\t.balign 8");
            ir_pass("%s:", symname(d->v));
            for (k = 0; k < d->n; k++)
                ir_pass("\t.quad %lld", d->w[k]);
        }
    }
    for (p = pools; p != NULL; p = p->next) {
        ir_pass("\t.data");
        ir_pass("\t.balign 8");
        ir_pass(".L%d:", p->label);
        if (isstring(p->v)) {
            char *s;
            size_t k, m = 0;

            dname_str(p->v, buf, sizeof buf);
            s = malloc(strlen(buf) * 4 + 8);
            for (k = 1; buf[k]; k++) {
                unsigned char ch = (unsigned char)buf[k];

                if (ch == '"' || ch == '\\')
                    s[m++] = '\\', s[m++] = (char)ch;
                else if (ch >= 32 && ch < 127)
                    s[m++] = (char)ch;
                else
                    m += (size_t)sprintf(s + m, "\\%03o", ch);
            }
            s[m] = '\0';
            ir_pass("\t.string \"%s\"", s);
            free(s);
            ir_pass("\t.balign 8");
        } else if (isnumconst(p->v, &cv))
            ir_pass("\t.quad %lld", (long long)cv);
    }
}

/* Translate the program whose code is the chain of stack entry a. */
void
trans_unit(word a)
{
    long i, start = -1;

    if (irfile == NULL)
        return;
    collect(u36(FREE(a + 2)) >> 18);
    vars = NULL;
    nfnames = 0;
    usedfs = 0;
    datadefs = NULL;
    pools = NULL;
    unsupported = 0;
    for (i = 0; i < nti; i++) {
        int entry = tis[i].kind == K_TAG && i + 1 < nti
            && tis[i + 1].kind == K_INSN && tis[i + 1].op == 0
            && tis[i + 1].v == tis[i].v;

        if (entry) {
            if (start >= 0)
                subr(start, i, 0);
            else if (i > 0)
                subr(0, i, 1);                  /* the main program */
            start = i;
        }
    }
    if (start >= 0)
        subr(start, nti, 0);
    else if (nti > 0)
        subr(0, nti, 1);
    emit_data();
    fflush(irfile);
}
