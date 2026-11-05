/*
 * ir2ll -- the LLVM back end: the IR text that imp72s0 writes (-c), read on standard input, becomes a module of LLVM IR on standard output, for
 * clang or llc to turn into an object.  It reads what ir2c reads (the IR as lowered for amd64: the arguments of a call in the registers rdi, rsi, ...
 * and above the sixth in the outgoing area, the result in rax) and undoes the lowering the same way, so that any host that has LLVM can be a host of
 * the compiler, whatever its byte order or word size.
 *
 * A word is i64, a real a double (an IMP real is the bits of one, moved through the scratch words imp_fs0, imp_fs1 and imp_fsr), an address an i64
 * that is made a pointer where it is dereferenced.  The arguments and the parameters of a subroutine are pointer sized integers, as the C that ir2c
 * writes has them (intptr_t): -p32 says the host's pointers are 32 bits, else they are 64.  No target triple or data layout is written: the
 * tool that reads the module supplies the host's.
 *
 * Input (see stage0/ir.c): "* text" assembler lines, which here are only data directives; "! type flags prolab tmpbase lblbase name",
 * "^ n" (label), '" op type qual ...' (one tree node per line, preorder) and "% autos retlab tmpend lblend name".
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    NAME = 2, ICON = 4, REG = 6, TEMP = 8, PLUS = 10, MINUS = 11, DIV = 12,
    MOD = 13, MUL = 14, AND = 15, OR = 16, ER = 17, LS = 18, RS = 19,
    COMPL = 20, UMUL = 23, UMINUS = 24, EQ = 25, NE = 26, LE = 27, LT = 28,
    GE = 29, GT = 30, ULE = 31, ULT = 32, UGE = 33, UGT = 34, CBRANCH = 35,
    SCONV = 37, PCONV = 38, CALL = 41, UCALL = 42, CM = 48, ASSIGN = 49,
    GOTO = 54
};
enum { T_CHAR = 2, T_UCHAR = 3, T_SHORT = 4, T_USHORT = 5, T_INT = 6, T_UINT = 7, T_LONG = 8, T_ULONG = 9, T_FLOAT = 12, T_DOUBLE = 13, T_VOID = 19 };
enum { R_RAX = 0, R_RBP = 6, R_RSP = 7 };

struct node {
    int op, type;
    long long lval;
    int rval;
    char *name;
    struct node *l, *r;
};

#ifdef __APPLE__
#define IVSEG "__DATA,"                  /* a Mach-O section has a segment */
#else
#define IVSEG ""
#endif

static FILE *out;                       /* the function being written */
static FILE *datafile;                  /* the global variables */
static FILE *bodyfile;                  /* the functions */
static int ptr32;                       /* the host's pointers are 32 bits */
static int tbe;                         /* the target is big endian: the address in a data word of a 32 bit pointer is its second half */
static FILE *inp, *outp;                /* the IR in, the module out */

static void
die(const char *msg, const char *line)
{
    fprintf(stderr, "ir2ll: %s: %s\n", msg, line);
    exit(1);
}

static char *
xstrdup(const char *s)
{
    size_t k = strlen(s) + 1;
    char *r = malloc(k);

    memcpy(r, s, k);
    return r;
}

static char *
fmt(const char *f, ...)
{
    char *buf = malloc(8192);
    va_list ap;

    va_start(ap, f);
    vsnprintf(buf, 8192, f, ap);
    va_end(ap);
    return buf;
}

#define IPT (ptr32 ? "i32" : "i64")

/* an assembler symbol as an LLVM global name */
static char *
gname(const char *s)
{
    return fmt("@\"%s\"", s);
}

/* ---- name tables ------------------------------------------------------------- */

struct set {
    char *v[4096];
    int n;
};

static int
inset(struct set *s, const char *x)
{
    int i;

    for (i = 0; i < s->n; i++)
        if (strcmp(s->v[i], x) == 0)
            return 1;
    return 0;
}

static void
add(struct set *s, const char *x)
{
    if (!inset(s, x) && s->n < 4096)
        s->v[s->n++] = xstrdup(x);
}

static struct set called, datarefs, datadefs, funcs, cfuncs, locals;
static struct { char *sym; long size; char *name; } varrecs[4096];       /* the compiler's own variables, for the image (ir_varrec) */
static int nvarrec;
static struct { char *name; long size; int local; } comms[4096];
static int ncomm;
static int usesjmp;                     /* the function being written calls _setjmp */

/* the C functions (CCALL's, marked `# cfunc`): they return a C long, not a word; what the IR defines, and the runtime library (written in IMP), returns a word */
static int
iscfunc(const char *nm)
{
    if (inset(&funcs, nm))                      /* defined in IMP here (impmain, say): it returns a word */
        return 0;
    if (strcmp(nm, "_setjmp") == 0 || strcmp(nm, "setjmp") == 0 || strcmp(nm, "longjmp") == 0 || strcmp(nm, "_longjmp") == 0)
        return 1;                               /* the C library's, which the nonlocal GO TO calls: an int in eax, not a word in edx:eax on a 32 bit machine */
    return inset(&cfuncs, nm);
}

/* ---- trees ---------------------------------------------------------------- */

static struct node *
readnode(FILE *f)
{
    char line[600];
    struct node *n;
    int op, type, q, k;
    long long lv = 0;
    char name[500] = "";

    if (fgets(line, sizeof line, f) == NULL || line[0] != '"')
        die("tree expected", line);
    n = calloc(1, sizeof *n);
    if (sscanf(line + 1, "%d %d %d", &op, &type, &q) != 3)
        die("bad node", line);
    n->op = op;
    n->type = type;
    switch (op) {
    case NAME:
    case ICON:
        k = sscanf(line + 1, "%*d %*d %*d %lld %499s", &lv, name);
        if (k < 1)
            die("bad leaf", line);
        n->lval = lv;
        n->name = k == 2 ? xstrdup(name) : NULL;
        return n;
    case REG:
    case TEMP:
        if (sscanf(line + 1, "%*d %*d %*d %*d %d", &n->rval) != 1)
            die("bad register", line);
        return n;
    default:
        break;
    }
    if (sscanf(line + 1, "%*d %*d %*d %d", &n->rval) == 1) {    /* unary */
        n->l = readnode(f);
        return n;
    }
    n->l = readnode(f);
    n->r = readnode(f);
    return n;
}

/* ---- LLVM types and values ------------------------------------------------ */

static const char *
ltype(int t)
{
    switch (t) {
    case T_CHAR: case T_UCHAR: return "i8";
    case T_SHORT: case T_USHORT: return "i16";
    case T_INT: return "i32";
    case T_FLOAT: return "float";
    case T_DOUBLE: return "double";
    default: return "i64";              /* longs and every pointer */
    }
}

static int
bits(int t)
{
    switch (t) {
    case T_CHAR: case T_UCHAR: return 8;
    case T_SHORT: case T_USHORT: return 16;
    case T_INT: case T_FLOAT: return 32;
    default: return 64;
    }
}

static int
isunsigned(int t)
{
    return t == T_ULONG || t == T_UCHAR || t == T_USHORT;
}

static const char *
lalign(int t)
{
    switch (t) {
    case T_CHAR: case T_UCHAR: return "1";
    case T_SHORT: case T_USHORT: return "2";
    case T_INT: case T_FLOAT: return "4";
    default: return ptr32 ? "4" : "8";
    }
}

static int nval;                        /* the SSA values of the function */
static int term;                        /* the block ends in a terminator */
static int ndead;

static char *
newval(void)
{
    return fmt("%%v%d", nval++);
}

/* an instruction: a block that was ended is followed by one that nothing reaches */
static void
ins(const char *f, ...)
{
    va_list ap;

    if (term) {
        fprintf(out, "dead%d:\n", ndead++);
        term = 0;
    }
    fputs("  ", out);
    va_start(ap, f);
    vfprintf(out, f, ap);
    va_end(ap);
    fputc('\n', out);
}

static void
terminate(const char *f, ...)
{
    va_list ap;

    if (term) {
        fprintf(out, "dead%d:\n", ndead++);
        term = 0;
    }
    fputs("  ", out);
    va_start(ap, f);
    vfprintf(out, f, ap);
    va_end(ap);
    fputc('\n', out);
    term = 1;
}

/* v of type from, as the type to */
static char *
conv(char *v, int from, int to)
{
    const char *lf = ltype(from), *lt = ltype(to);
    char *r;

    if (strcmp(lf, lt) == 0)
        return v;
    r = newval();
    if (from == T_FLOAT && to == T_DOUBLE) {
        ins("%s = fpext float %s to double", r, v);
    } else if (from == T_DOUBLE && to == T_FLOAT) {
        ins("%s = fptrunc double %s to float", r, v);
    } else if (from == T_DOUBLE) {
        ins("%s = %s %s %s to %s", r, isunsigned(to) ? "fptoui" : "fptosi", lf, v, lt);
    } else if (to == T_DOUBLE) {
        ins("%s = %s %s %s to double", r, isunsigned(from) ? "uitofp" : "sitofp", lf, v);
    } else if (bits(to) > bits(from)) {
        ins("%s = %s %s %s to %s", r, isunsigned(from) ? "zext" : "sext", lf, v, lt);
    } else {
        ins("%s = trunc %s %s to %s", r, lf, v, lt);
    }
    return r;
}

/* an address constant: symbol + offset, as an i64 */
static char *
symaddr(const char *name, long long off)
{
    char *p = newval(), *q = newval();

    ins("%s = getelementptr i8, ptr %s, i64 %lld", p, gname(name), off);
    if (ptr32) {
        char *z = newval();

        ins("%s = ptrtoint ptr %s to i32", q, p);
        ins("%s = zext i32 %s to i64", z, q);
        return z;
    }
    ins("%s = ptrtoint ptr %s to i64", q, p);
    return q;
}

static int temptype[8192];              /* per temp number - 100 */
static int ntemps, maxarg;

static void
note_temp(struct node *n)
{
    if (n == NULL)
        return;
    if (n->op == TEMP) {
        int i = n->rval - 100;

        if (i < 0 || i >= 8192)
            die("temp out of range", "");
        if (temptype[i] == 0)
            temptype[i] = n->type;
        if (i + 1 > ntemps)
            ntemps = i + 1;
    }
    note_temp(n->l);
    note_temp(n->r);
}

static int
argslot(int reg)
{
    switch (reg) {
    case 5: return 0;                   /* rdi */
    case 4: return 1;                   /* rsi */
    case 1: return 2;                   /* rdx */
    case 2: return 3;                   /* rcx */
    case 8: return 4;
    case 9: return 5;
    default: return -1;
    }
}

/* an argument slot above the sixth: *(rbp + 16 + 8k) */
static int
stackparam(struct node *n)
{
    struct node *p;

    if (n->op != UMUL)
        return -1;
    p = n->l;
    if (p->op == PLUS && p->l->op == REG && p->l->rval == R_RBP && p->r->op == ICON)
        return (int)((p->r->lval - 16) / 8) + 6;
    return -1;
}

static char *expr(struct node *n);

/* the argument k of the function, as an i64 (an address: zero extended) */
static char *
param(int k)
{
    char *r;

    if (k > maxarg)
        maxarg = k;
    if (!ptr32)
        return fmt("%%a%d", k);
    r = newval();
    ins("%s = zext i32 %%a%d to i64", r, k);
    return r;
}

/* a pointer for the address (an i64) */
static char *
topointer(char *a)
{
    char *p = newval();

    ins("%s = inttoptr i64 %s to ptr", p, a);
    return p;
}

/* the pointer to the place a NAME, TEMP or UMUL node names */
static char *
place(struct node *n)
{
    char *p;

    switch (n->op) {
    case NAME:
        add(&datarefs, n->name);
        p = newval();
        ins("%s = getelementptr i8, ptr %s, i64 %lld", p, gname(n->name), n->lval);
        return p;
    case TEMP:
        return fmt("%%t%d", n->rval);
    case UMUL:
        return topointer(expr(n->l));
    default:
        die("not an lvalue", "");
    }
    return NULL;
}

static char *
load(struct node *n)
{
    char *p = place(n), *r = newval();
    int t = n->type;

    if (n->op == TEMP) {                /* the temp has the type of its first appearance */
        int tt = temptype[n->rval - 100];

        ins("%s = load%s %s, ptr %s, align %s", r, usesjmp ? " volatile" : "", ltype(tt), p, lalign(tt));
        return conv(r, tt, t);
    }
    ins("%s = load %s, ptr %s, align %s", r, ltype(t), p, lalign(t));
    return r;
}

static void
store(struct node *d, char *v, int vt)
{
    char *p = place(d);

    if (d->op == TEMP) {
        int tt = temptype[d->rval - 100];

        v = conv(conv(v, vt, d->type), d->type, tt);
        ins("store%s %s %s, ptr %s, align %s", usesjmp ? " volatile" : "", ltype(tt), v, p, lalign(tt));
        return;
    }
    v = conv(v, vt, d->type);
    ins("store %s %s, ptr %s, align %s", ltype(d->type), v, p, lalign(d->type));
}

/* The number of fixed parameters of the C library's variadic functions that programs call, else 0. */
static int
variadic_fixed(const char *nm)
{
    static const struct { const char *name; int fixed; } v[] = {
        {"printf", 1}, {"fprintf", 2}, {"sprintf", 2}, {"snprintf", 3}, {"dprintf", 2},
        {"scanf", 1}, {"sscanf", 2}, {"fscanf", 2}, {"syslog", 2}, {"open", 2}, {"fcntl", 2},
        {"ioctl", 2}, {"execl", 2}, {"execlp", 2},
    };
    size_t k;

    for (k = 0; k < sizeof v / sizeof v[0]; k++)
        if (strcmp(nm, v[k].name) == 0)
            return v[k].fixed;
    return 0;
}

/* ---- the calls of declared C functions ------------------------------------------------------------------------------------------
 * The code generator names such a function NAME@SIG in the call: SIG is the letter of the type of the result (c C s S i I l L q Q z p d f v: CHAR UCHAR
 * SHORT USHORT INT UINT LONG ULONG LLONG ULLONG SIZE PTR DOUBLE FLOAT VOID), then the letters of the arguments, with a dot before those of a variadic call
 * that have no parameter.  The arguments are words (pointer sized); a double, a float and a long long are the address of a word of 64 bits. */

static struct { char *name; char *decl; } tprotos[256];
static int ntprotos;

static const char *
tytext(int c)
{
    switch (c) {
    case 'c': case 'C': return "i8";
    case 's': case 'S': return "i16";
    case 'i': case 'I': return "i32";
    case 'l': case 'L': case 'z': return IPT;
    case 'q': case 'Q': return "i64";
    case 'p': return "ptr";
    case 'd': return "double";
    case 'f': return "float";
    case 'v': return "void";
    default: die("unknown C type in a call", "");
    }
    return NULL;
}

static const char *
tyattr(int c)
{
    switch (c) {
    case 'c': case 's': case 'i': return " signext";
    case 'C': case 'S': case 'I': return " zeroext";
    default: return "";
    }
}

/* the value of the argument with the C type c, from v: a pointer sized integer */
static char *
targ(int c, char *v)
{
    char *t, *p;

    switch (c) {
    case 'c': case 'C':
        t = newval();
        ins("%s = trunc %s %s to i8", t, IPT, v);
        return t;
    case 's': case 'S':
        t = newval();
        ins("%s = trunc %s %s to i16", t, IPT, v);
        return t;
    case 'i': case 'I':
        if (!ptr32) {
            t = newval();
            ins("%s = trunc i64 %s to i32", t, v);
            return t;
        }
        return v;
    case 'p':
        t = newval();
        ins("%s = inttoptr %s %s to ptr", t, IPT, v);
        return t;
    case 'q': case 'Q': case 'd': case 'f':
        p = newval();
        ins("%s = inttoptr %s %s to ptr", p, IPT, v);
        t = newval();
        ins("%s = load %s, ptr %s, align 8", t, c == 'd' || c == 'f' ? "double" : "i64", p);
        if (c == 'f') {
            char *u = newval();

            ins("%s = fptrunc double %s to float", u, t);
            return u;
        }
        return t;
    default:
        return v;
    }
}

static char *
typedcall(const char *nm, const char *sig, char **vals, int nargs)
{
    int res = sig[0], k = 0, nfix = -1, dots = 0;
    const char *s;
    char *o = fmt(""), *ft = fmt(""), *r, *t;
    char *fixed = fmt("");
    char *callee = gname(nm);
    int indirect = strcmp(nm, "impcallt") == 0;

    if (indirect) {                                     /* a call through a pointer (FNPTR): the signature has a p, the function, after the letter of the result */
        if (nargs < 1 || sig[1] != 'p')
            die("call through a pointer without the pointer", nm);
        callee = targ('p', vals[0]);
        sig = fmt("%c%s", sig[0], sig + 2);
        vals++;
        nargs--;
    }

    for (s = sig + 1; *s; s++) {
        if (*s == '.') {
            nfix = k;
            continue;
        }
        if (k >= nargs)
            die("call with fewer arguments than its signature", nm);
        if (nfix < 0 || k < nfix)
            fixed = fmt("%s%s%s%s", fixed, k ? ", " : "", tytext(*s), tyattr(*s));
        k++;
    }
    if (k != nargs)
        die("call with more arguments than its signature", nm);
    dots = nfix >= 0;
    if (dots) {
        char *fp = fmt("");
        int j;

        s = sig + 1;
        for (j = 0; j < nfix; j++)
            fp = fmt("%s%s%s", fp, j ? ", " : "", tytext(s[j]));
        ft = fmt("(%s%s...) ", fp, nfix ? ", " : "");
    }
    k = 0;
    for (s = sig + 1; *s; s++) {
        if (*s == '.')
            continue;
        t = targ(*s, vals[k]);
        o = fmt("%s%s%s%s %s", o, k ? ", " : "", tytext(*s), nfix >= 0 && k >= nfix ? "" : tyattr(*s), t);
        k++;
    }
    if (!indirect && ntprotos < 256) {
        int j;

        for (j = 0; j < ntprotos; j++)
            if (strcmp(tprotos[j].name, nm) == 0)
                break;
        if (j == ntprotos) {
            tprotos[j].name = xstrdup(nm);
            tprotos[j].decl = xstrdup(fmt("declare %s %s(%s%s)", tytext(res), gname(nm), fixed, dots ? (nfix ? ", ..." : "...") : ""));
            ntprotos++;
        }
    }
    if (res == 'v') {
        ins("call void %s%s(%s)", ft, callee, o);
        return fmt("0");
    }
    r = newval();
    ins("%s = call %s %s%s(%s)", r, tytext(res), ft, callee, o);
    switch (res) {
    case 'c': case 's': case 'i':
        t = newval();
        ins("%s = sext %s %s to i64", t, tytext(res), r);
        return t;
    case 'C': case 'S': case 'I':
        t = newval();
        ins("%s = zext %s %s to i64", t, tytext(res), r);
        return t;
    case 'l':
        if (ptr32) {
            t = newval();
            ins("%s = sext i32 %s to i64", t, r);
            return t;
        }
        return r;
    case 'L': case 'z':
        if (ptr32) {
            t = newval();
            ins("%s = zext i32 %s to i64", t, r);
            return t;
        }
        return r;
    case 'p': {
        char *u = newval();

        ins("%s = ptrtoint ptr %s to %s", u, r, IPT);
        if (ptr32) {
            t = newval();
            ins("%s = zext i32 %s to i64", t, u);
            return t;
        }
        return u;
    }
    case 'd':
        t = newval();
        ins("%s = bitcast double %s to i64", t, r);
        return t;
    case 'f': {
        char *u = newval();

        ins("%s = fpext float %s to double", u, r);
        t = newval();
        ins("%s = bitcast double %s to i64", t, u);
        return t;
    }
    default:
        return r;
    }
}

/* ---- the C entries of the callbacks ---------------------------------------------------------------------------------------------
 * CALLBACK(NAME) is the address of the symbol NAME@SIG, SIG written as for a typed call (the letter of the result, then one for each parameter).  The module defines
 * it, at the first use, as an internal function with the C types of SIG: each parameter is converted to a word, stored in a word of its own, and the IMP routine NAME
 * is called with the addresses of the words (the IMP convention: references, a word as the result); the word it returns is converted to the C type of the result. */

static struct set thunks;

static void
thunkdef(const char *full)
{
    char *base = xstrdup(full), *sig = strchr(base, '@'), *v, *x;
    int res, np, k;
    FILE *f = bodyfile;

    *sig++ = '\0';
    if (inset(&thunks, full)) {
        free(base);
        return;
    }
    add(&thunks, full);
    add(&called, base);
    res = sig[0];
    np = (int)strlen(sig + 1);
    fprintf(f, "define internal%s %s %s(", tyattr(res), tytext(res), gname(full));
    for (k = 0; k < np; k++)
        fprintf(f, "%s%s%s %%p%d", k ? ", " : "", tytext(sig[k + 1]), tyattr(sig[k + 1]), k);
    fprintf(f, ") {\nentry:\n");
    for (k = 0; k < np; k++) {
        int c = sig[k + 1];

        fprintf(f, "  %%w%d = alloca i64, align 8\n", k);
        switch (c) {
        case 'c': case 's': case 'i':
            fprintf(f, "  %%c%d = sext %s %%p%d to i64\n", k, tytext(c), k);
            break;
        case 'C': case 'S': case 'I':
            fprintf(f, "  %%c%d = zext %s %%p%d to i64\n", k, tytext(c), k);
            break;
        case 'l':
            if (ptr32)
                fprintf(f, "  %%c%d = sext i32 %%p%d to i64\n", k, k);
            else
                fprintf(f, "  %%c%d = add i64 %%p%d, 0\n", k, k);
            break;
        case 'L': case 'z':
            if (ptr32)
                fprintf(f, "  %%c%d = zext i32 %%p%d to i64\n", k, k);
            else
                fprintf(f, "  %%c%d = add i64 %%p%d, 0\n", k, k);
            break;
        case 'p':
            fprintf(f, "  %%u%d = ptrtoint ptr %%p%d to %s\n", k, k, IPT);
            if (ptr32)
                fprintf(f, "  %%c%d = zext i32 %%u%d to i64\n", k, k);
            else
                fprintf(f, "  %%c%d = add i64 %%u%d, 0\n", k, k);
            break;
        case 'd':
            fprintf(f, "  %%c%d = bitcast double %%p%d to i64\n", k, k);
            break;
        case 'f':
            fprintf(f, "  %%u%d = fpext float %%p%d to double\n", k, k);
            fprintf(f, "  %%c%d = bitcast double %%u%d to i64\n", k, k);
            break;
        default:
            fprintf(f, "  %%c%d = add i64 %%p%d, 0\n", k, k);
            break;
        }
        fprintf(f, "  store i64 %%c%d, ptr %%w%d, align 8\n", k, k);
        fprintf(f, "  %%a%d = ptrtoint ptr %%w%d to %s\n", k, k, IPT);
    }
    v = xstrdup("(");
    x = xstrdup("");
    for (k = 0; k < np; k++) {
        v = fmt("%s%s%s", v, k ? ", " : "", IPT);
        x = fmt("%s%s%s %%a%d", x, k ? ", " : "", IPT, k);
    }
    fprintf(f, "  %%r = call i64 %s) %s(%s)\n", v, gname(base), x);
    switch (res) {
    case 'v':
        fprintf(f, "  ret void\n");
        break;
    case 'c': case 's': case 'i': case 'C': case 'S': case 'I':
        fprintf(f, "  %%x = trunc i64 %%r to %s\n  ret %s %%x\n", tytext(res), tytext(res));
        break;
    case 'l': case 'L': case 'z':
        if (ptr32)
            fprintf(f, "  %%x = trunc i64 %%r to i32\n  ret i32 %%x\n");
        else
            fprintf(f, "  ret i64 %%r\n");
        break;
    case 'p':
        if (ptr32) {
            fprintf(f, "  %%t = trunc i64 %%r to i32\n  %%x = inttoptr i32 %%t to ptr\n");
        } else
            fprintf(f, "  %%x = inttoptr i64 %%r to ptr\n");
        fprintf(f, "  ret ptr %%x\n");
        break;
    case 'd':
        fprintf(f, "  %%x = bitcast i64 %%r to double\n  ret double %%x\n");
        break;
    case 'f':
        fprintf(f, "  %%t = bitcast i64 %%r to double\n  %%x = fptrunc double %%t to float\n  ret float %%x\n");
        break;
    default:
        fprintf(f, "  ret i64 %%r\n");
        break;
    }
    fprintf(f, "}\n\n");
    free(base);
}

/* a call: the value is an i64 */
static char *
callexpr(struct node *n)
{
    struct node *fn = n->l, *args[64] = { NULL }, *list[64], *c;
    char *vals[64], *r, *o;
    int nargs, k, top = -1, nl = 0, nfix, cf, ind;
    const char *sig;

    if (fn->op != ICON || fn->name == NULL)
        die("call of a computed function", "");
    if ((sig = strchr(fn->name, '@')) != NULL) {         /* a declared C function: NAME@SIGNATURE */
        *(char *)sig = '\0';
        sig++;
    }
    /* a call through an address: CCALL IMPCALLP(F,A,...) (the result a C long) or IMPCALLW(F,A,...) (the result an IMP word, 64 bits) calls the function at F with the arguments A, ... by value */
    ind = strcmp(fn->name, "impcallp") == 0 || strcmp(fn->name, "impcallw") == 0 || strcmp(fn->name, "impcallt") == 0;
    if (!ind)
        add(&called, fn->name);
    if (strcmp(fn->name, "_setjmp") == 0 || strcmp(fn->name, "setjmp") == 0)
        usesjmp = 1;
    if (n->op == CALL) {
        for (c = n->r; c->op == CM; c = c->l)       /* the list is left nested */
            list[nl++] = c->r;
        list[nl++] = c;
        for (k = nl - 1; k >= 0; k--) {
            struct node *a = list[k], *dst = a->l;
            int slot;

            if (a->op != ASSIGN)
                die("argument is not an assignment", fn->name);
            if (dst->op == REG) {
                if (dst->rval == R_RAX)
                    continue;                       /* the vector register count */
                slot = argslot(dst->rval);
                if (slot < 0)
                    die("argument register", fn->name);
            } else if (dst->op == UMUL && dst->l->op == PLUS && dst->l->l->op == REG &&
                dst->l->l->rval == R_RSP)
                slot = 6 + (int)(dst->l->r->lval / 8);
            else
                die("argument destination", fn->name);
            args[slot] = a->r;
            if (slot > top)
                top = slot;
        }
    }
    nargs = top + 1;
    for (k = 0; k < nargs; k++) {                   /* the arguments, evaluated in order, as pointer sized integers */
        char *v;

        if (args[k] == NULL)
            die("missing argument", fn->name);
        v = expr(args[k]);
        v = conv(v, args[k]->type, T_LONG);
        if (ptr32) {
            char *t = newval();

            ins("%s = trunc i64 %s to i32", t, v);
            v = t;
        }
        vals[k] = v;
    }
    if (sig != NULL)
        return typedcall(fn->name, sig, vals, nargs);
    if (ind) {
        char *f = newval(), *ft = fmt("(");
        int j;

        if (nargs < 1)
            die("call through an address without the address", fn->name);
        ins("%s = inttoptr %s %s to ptr", f, IPT, vals[0]);
        o = fmt("");
        for (j = 1; j < nargs; j++) {
            ft = fmt("%s%s%s", ft, j > 1 ? ", " : "", IPT);
            o = fmt("%s%s%s %s", o, j > 1 ? ", " : "", IPT, vals[j]);
        }
        ft = fmt("%s)", ft);
        r = newval();
        if (fn->name[7] == 'w') {
            ins("%s = call i64 %s %s(%s)", r, ft, f, o);
            return r;
        }
        ins("%s = call %s %s %s(%s)", r, IPT, ft, f, o);
        if (ptr32) {
            char *z = newval();

            ins("%s = sext i32 %s to i64", z, r);
            return z;
        }
        return r;
    }
    cf = iscfunc(fn->name);
    nfix = variadic_fixed(fn->name);
    o = fmt("");
    for (k = 0; k < nargs; k++)
        o = fmt("%s%s%s %s", o, k ? ", " : "", IPT, vals[k]);
    r = newval();
    if (nfix > 0) {                                 /* a variadic function of the C library */
        char *ft = fmt("(");
        int j;

        for (j = 0; j < nfix; j++)
            ft = fmt("%s%s%s", ft, j ? ", " : "", IPT);
        ft = fmt("%s, ...)", ft);
        ins("%s = call i32 %s %s(%s)", r, ft, gname(fn->name), o);
        {
            char *z = newval();

            ins("%s = sext i32 %s to i64", z, r);
            return z;
        }
    }
    if (cf) {                                       /* a C long: the pointer's size */
        char *z = newval();
        char *ft = fmt("(");

        for (k = 0; k < nargs; k++)
            ft = fmt("%s%s%s", ft, k ? ", " : "", IPT);
        ft = fmt("%s)", ft);
        ins("%s = call %s %s %s(%s)%s", r, IPT, ft, gname(fn->name), o,
            strcmp(fn->name, "_setjmp") == 0 || strcmp(fn->name, "setjmp") == 0 ? " returns_twice" : "");
        if (ptr32) {
            ins("%s = sext i32 %s to i64", z, r);
            return z;
        }
        return r;
    } else {
        char *ft = fmt("(");

        for (k = 0; k < nargs; k++)
            ft = fmt("%s%s%s", ft, k ? ", " : "", IPT);
        ft = fmt("%s)", ft);
        ins("%s = call i64 %s %s(%s)", r, ft, gname(fn->name), o);
    }
    return r;
}

/* a comparison: an i1 */
static char *
compare(struct node *n)
{
    static const char *sg[] = { "eq", "ne", "sle", "slt", "sge", "sgt" };
    static const char *ug[] = { "eq", "ne", "ule", "ult", "uge", "ugt" };
    static const char *fg[] = { "oeq", "une", "ole", "olt", "oge", "ogt" };
    int lt = n->l->type, ct;
    char *l, *r, *c;

    if (n->op >= ULE) {                             /* the unsigned compares */
        static const char *uo[] = { "ule", "ult", "uge", "ugt" };

        l = conv(expr(n->l), lt, T_ULONG);
        r = conv(expr(n->r), n->r->type, T_ULONG);
        c = newval();
        ins("%s = icmp %s i64 %s, %s", c, uo[n->op - ULE], l, r);
        return c;
    }
    ct = lt == T_DOUBLE ? T_DOUBLE : isunsigned(lt) ? T_ULONG : lt == T_INT ? T_INT : T_LONG;
    l = conv(expr(n->l), lt, ct);
    r = conv(expr(n->r), n->r->type, ct);
    c = newval();
    if (ct == T_DOUBLE)
        ins("%s = fcmp %s double %s, %s", c, fg[n->op - EQ], l, r);
    else
        ins("%s = icmp %s %s %s, %s", c, (isunsigned(ct) ? ug : sg)[n->op - EQ], ltype(ct), l, r);
    return c;
}

static char *
expr(struct node *n)
{
    int t = n->type;
    char *l, *r, *v;
    const char *lt_;

    switch (n->op) {
    case ICON:
        if (n->name != NULL) {
            if (strchr(n->name, '@') != NULL)               /* CALLBACK(NAME): NAME@SIGNATURE */
                thunkdef(n->name);
            else if (!inset(&called, n->name))
                add(&datarefs, n->name);
            return conv(symaddr(n->name, n->lval), T_LONG, t);
        }
        if (t == T_DOUBLE)
            return fmt("%lld.0", n->lval);
        if (t == T_INT)
            return fmt("%d", (int)n->lval);
        if (t == T_UCHAR)
            return fmt("%d", (int)(n->lval & 255));
        return fmt("%lld", n->lval);
    case NAME:
        return load(n);
    case TEMP:
        return load(n);
    case REG: {
        int s = argslot(n->rval);

        if (s < 0)
            die("register not translated", "");
        return conv(param(s), T_LONG, t);
    }
    case UMUL: {
        int sp = stackparam(n);

        if (sp >= 0)
            return conv(param(sp), T_LONG, t);
        return load(n);
    }
    case CALL:
    case UCALL:
        return conv(callexpr(n), T_LONG, t);
    case PCONV:
    case SCONV:
        return conv(expr(n->l), n->l->type, t);
    case UMINUS:
        l = conv(expr(n->l), n->l->type, t);
        v = newval();
        if (t == T_DOUBLE)
            ins("%s = fneg double %s", v, l);
        else
            ins("%s = sub %s 0, %s", v, ltype(t), l);
        return v;
    case COMPL:
        l = conv(expr(n->l), n->l->type, t);
        v = newval();
        ins("%s = xor %s %s, -1", v, ltype(t), l);
        return v;
    default:
        break;
    }
    if (n->op >= EQ && n->op <= UGT) {              /* a comparison as a value: 0 or 1 */
        char *c = compare(n);

        v = newval();
        ins("%s = zext i1 %s to %s", v, c, ltype(t));
        return v;
    }
    l = conv(expr(n->l), n->l->type, t);
    r = conv(expr(n->r), n->r->type, t);
    lt_ = ltype(t);
    v = newval();
    switch (n->op) {
    case PLUS:
        ins("%s = %s %s %s, %s", v, t == T_DOUBLE ? "fadd" : "add", lt_, l, r);
        return v;
    case MINUS:
        ins("%s = %s %s %s, %s", v, t == T_DOUBLE ? "fsub" : "sub", lt_, l, r);
        return v;
    case MUL:
        ins("%s = %s %s %s, %s", v, t == T_DOUBLE ? "fmul" : "mul", lt_, l, r);
        return v;
    case DIV:
        ins("%s = %s %s %s, %s", v, t == T_DOUBLE ? "fdiv" : isunsigned(t) ? "udiv" : "sdiv", lt_, l, r);
        return v;
    case MOD:
        if (t == T_DOUBLE)
            die("integer operator on a real", "");
        ins("%s = %s %s %s, %s", v, isunsigned(t) ? "urem" : "srem", lt_, l, r);
        return v;
    case AND: case OR: case ER:
        if (t == T_DOUBLE)
            die("integer operator on a real", "");
        ins("%s = %s %s %s, %s", v, n->op == AND ? "and" : n->op == OR ? "or" : "xor", lt_, l, r);
        return v;
    case LS: case RS: {
        char *m = newval();

        ins("%s = and %s %s, %d", m, lt_, r, bits(t) - 1);
        ins("%s = %s %s %s, %s", v, n->op == LS ? "shl" : isunsigned(t) ? "lshr" : "ashr", lt_, l, m);
        return v;
    }
    default:
        break;
    }
    fprintf(stderr, "ir2ll: operator %d\n", n->op);
    exit(1);
}

/* ---- statements ------------------------------------------------------------ */

static void
statement(struct node *n)
{
    switch (n->op) {
    case ASSIGN: {
        struct node *d = n->l;

        if (d->op == REG && d->rval == R_RAX) {
            char *v = conv(expr(n->r), n->r->type, T_LONG);

            terminate("ret i64 %s", v);
            return;
        }
        {
            char *v = expr(n->r);

            store(d, v, n->r->type);
        }
        return;
    }
    case CBRANCH: {
        char *c, *next = fmt("%%n%d", nval++);

        if (n->l->op >= EQ && n->l->op <= UGT)
            c = compare(n->l);
        else {
            char *v = conv(expr(n->l), n->l->type, T_LONG);

            c = newval();
            ins("%s = icmp ne i64 %s, 0", c, v);
        }
        terminate("br i1 %s, label %%L%lld, label %s", c, n->r->lval, next);
        fprintf(out, "%s:\n", next + 1);
        term = 0;
        return;
    }
    case GOTO:
        terminate("br label %%L%lld", n->l->lval);
        return;
    case CALL:
    case UCALL:
        (void)callexpr(n);
        return;
    default:
        fprintf(stderr, "ir2ll: statement %d\n", n->op);
        exit(1);
    }
}

/* ---- data ------------------------------------------------------------------ */

static char *dlabel, *dstring;
static long long dquads[4096];
static char *dqsyms[4096];                      /* a data word that is the address of a symbol (plus dquads[i] bytes), or NULL */
static int ndq;

/* the bytes of an assembler string (escapes: \n \t \\ \" and octal) as an LLVM constant */
static char *
llstring(const char *s, int *len)
{
    char *r = malloc(strlen(s) * 3 + 8), *p = r;
    int n = 0;

    while (*s) {
        unsigned c = (unsigned char)*s++;

        if (c == '\\' && *s) {
            c = (unsigned char)*s++;
            switch (c) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'r': c = '\r'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case 'a': c = 7; break;
            case 'v': c = 11; break;
            case 'x': {
                unsigned x = 0;
                int k = 0;

                while (k < 2 && ((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f') || (*s >= 'A' && *s <= 'F'))) {
                    x = x * 16 + (unsigned)((*s <= '9') ? *s - '0' : (*s | 32) - 'a' + 10);
                    s++;
                    k++;
                }
                c = x;
                break;
            }
            default:
                if (c >= '0' && c <= '7') {
                    unsigned x = c - '0';
                    int k = 1;

                    while (k < 3 && *s >= '0' && *s <= '7') {
                        x = x * 8 + (unsigned)(*s++ - '0');
                        k++;
                    }
                    c = x & 255;
                }
                break;
            }
        }
        p += snprintf(p, 4, "\\%02X", c & 255);
        n++;
    }
    p += snprintf(p, 4, "\\00");
    n++;
    *len = n;
    return r;
}

static void
flushdata(void)
{
    int i;

    if (dlabel != NULL) {
        const char *link = strncmp(dlabel, ".L", 2) == 0 ? "internal" : "dso_local";

        if (dstring != NULL) {
            int len;
            char *s = llstring(dstring, &len);

            fprintf(datafile, "%s = %s global [%d x i8] c\"%s\", align 8\n", gname(dlabel), link, len, s);
            add(&datadefs, dlabel);
        } else if (ndq > 0) {
            int mixed = 0;

            if (ptr32)
                for (i = 0; i < ndq; i++)
                    if (dqsyms[i] != NULL)
                        mixed = 1;
            if (mixed) {
                fprintf(datafile, "%s = %s global <{ ", gname(dlabel), link);
                for (i = 0; i < ndq; i++)
                    fprintf(datafile, "%s%s", i ? ", " : "", dqsyms[i] == NULL ? "i64" : "{ i32, i32 }");
                fprintf(datafile, " }> <{ ");
            } else
                fprintf(datafile, "%s = %s global [%d x i64] [", gname(dlabel), link, ndq);
            for (i = 0; i < ndq; i++) {
                if (dqsyms[i] == NULL)
                    fprintf(datafile, "%si64 %lld", i ? ", " : "", dquads[i]);
                else if (mixed && dquads[i] == 0)
                    fprintf(datafile, tbe ? "%s{ i32, i32 } { i32 0, i32 ptrtoint (ptr %s to i32) }" : "%s{ i32, i32 } { i32 ptrtoint (ptr %s to i32), i32 0 }", i ? ", " : "", gname(dqsyms[i]));
                else if (mixed)
                    fprintf(datafile, tbe ? "%s{ i32, i32 } { i32 0, i32 add (i32 ptrtoint (ptr %s to i32), i32 %lld) }" : "%s{ i32, i32 } { i32 add (i32 ptrtoint (ptr %s to i32), i32 %lld), i32 0 }", i ? ", " : "", gname(dqsyms[i]), dquads[i]);
                else if (dquads[i] == 0)
                    fprintf(datafile, "%si64 ptrtoint (ptr %s to i64)", i ? ", " : "", gname(dqsyms[i]));
                else
                    fprintf(datafile, "%si64 add (i64 ptrtoint (ptr %s to i64), i64 %lld)", i ? ", " : "", gname(dqsyms[i]), dquads[i]);
                dqsyms[i] = NULL;
            }
            fprintf(datafile, mixed ? " }>, align 8\n" : "], align 8\n");
            add(&datadefs, dlabel);
        }
    }
    dlabel = dstring = NULL;
    ndq = 0;
}

static void
passthrough(char *line)
{
    char name[256];
    long sz;
    char *c;

    while (*line == ' ' || *line == '\t')
        line++;
    if (strncmp(line, ".local ", 7) == 0) {
        add(&locals, line + 7);
        return;
    }
    if (sscanf(line, ".comm %255[^,],%ld", name, &sz) == 2) {
        flushdata();
        if (ncomm < 4096) {
            comms[ncomm].name = xstrdup(name);
            comms[ncomm].size = sz;
            comms[ncomm++].local = inset(&locals, name);
        }
        add(&datadefs, name);
        return;
    }
    if (strncmp(line, ".quad ", 6) == 0) {
        if (ndq < 4096) {
            const char *q = line + 6;

            if ((*q >= '0' && *q <= '9') || *q == '-') {
                dqsyms[ndq] = NULL;
                dquads[ndq++] = atoll(q);
            } else {                                    /* .quad SYMBOL+OFFSET: the address of a symbol */
                const char *pl = strrchr(q, '+');
                size_t nl = pl != NULL ? (size_t)(pl - q) : strlen(q);
                char *nm = malloc(nl + 1);

                memcpy(nm, q, nl);
                nm[nl] = '\0';
                add(&datarefs, nm);
                dqsyms[ndq] = nm;
                dquads[ndq++] = pl != NULL ? atoll(pl + 1) : 0;
            }
        }
        return;
    }
    if (strncmp(line, ".string \"", 9) == 0) {
        c = strrchr(line, '"');
        *c = '\0';
        dstring = xstrdup(line + 9);
        return;
    }
    if (strncmp(line, "# varrec ", 9) == 0) {
        char sym[256], nm[300];
        long vsz;

        if (sscanf(line + 9, "%255s %ld %299s", sym, &vsz, nm) == 3 && nvarrec < 4096) {
            varrecs[nvarrec].sym = xstrdup(sym);
            varrecs[nvarrec].size = vsz;
            varrecs[nvarrec++].name = xstrdup(nm);
            add(&datarefs, sym);
        }
        return;
    }
    if (strncmp(line, "# cfunc ", 8) == 0) {
        add(&cfuncs, line + 8);
        return;
    }
    c = strchr(line, ':');
    if ((line[0] != '.' || strncmp(line, ".L", 2) == 0) && c != NULL && c[1] == '\0') {
        *c = '\0';
        flushdata();
        dlabel = xstrdup(line);         /* a function entry label too: dropped at "!" */
        return;
    }
    /* .text .data .align .balign .globl .type .section: nothing to say in LLVM IR (the records of the image's variables, ir_varrec, are dropped too) */
}

/* ---- the functions ---------------------------------------------------------- */

static void
function(const char *header)
{
    char fname[256], line[600];
    int type, flags, prolab, tmpbase, lblbase, i;
    static struct node *stmts[40000];
    static int labels[40000];
    int nstm = 0;
    char *buf = NULL;
    size_t len = 0;
    FILE *mem;

    if (sscanf(header, "! %d %d %d %d %d %255s", &type, &flags, &prolab, &tmpbase, &lblbase, fname) != 6)
        die("bad function header", header);
    memset(temptype, 0, sizeof temptype);
    ntemps = 0;
    maxarg = -1;
    dlabel = NULL;                      /* the entry label just before the header */
    for (;;) {
        int c = getc(inp);

        if (c == EOF)
            die("function without an end", fname);
        if (c == '%') {
            if (fgets(line, sizeof line, inp) == NULL)
                break;
            break;
        }
        if (c == '^') {
            int lab;

            if (fscanf(inp, "%d", &lab) != 1)
                die("bad label", fname);
            while ((c = getc(inp)) != '\n' && c != EOF)
                ;
            labels[nstm] = lab;
            stmts[nstm++] = NULL;
            continue;
        }
        if (c == '*') {                 /* a pass-through line inside the body */
            if (fgets(line, sizeof line, inp) == NULL)
                break;
            line[strcspn(line, "\n")] = '\0';
            passthrough(line + 1);
            continue;
        }
        ungetc(c, inp);
        stmts[nstm] = readnode(inp);
        note_temp(stmts[nstm]);
        nstm++;
    }
    usesjmp = 0;
    for (i = 0; i < nstm; i++)          /* does it call _setjmp?  (then its temps are volatile) */
        if (stmts[i] != NULL) {
            struct node *s = stmts[i];
            struct node *stk[256];
            int sp = 0;

            stk[sp++] = s;
            while (sp > 0) {
                struct node *x = stk[--sp];

                if (x == NULL)
                    continue;
                if ((x->op == CALL || x->op == UCALL) && x->l->op == ICON && x->l->name != NULL &&
                    (strcmp(x->l->name, "_setjmp") == 0 || strcmp(x->l->name, "setjmp") == 0))
                    usesjmp = 1;
                if (sp < 250) {
                    stk[sp++] = x->l;
                    stk[sp++] = x->r;
                }
            }
        }
    mem = open_memstream(&buf, &len);
    out = mem;
    nval = 0;
    ndead = 0;
    term = 0;
    for (i = 0; i < nstm; i++) {
        if (stmts[i] == NULL) {
            if (!term)
                fprintf(out, "  br label %%L%d\n", labels[i]);
            fprintf(out, "L%d:\n", labels[i]);
            term = 0;
        } else
            statement(stmts[i]);
    }
    if (!term)
        fprintf(out, "  ret i64 0\n");
    fclose(mem);
    out = datafile;
    add(&funcs, fname);
    fprintf(bodyfile, "define %si64 %s(", strncmp(fname, ".L", 2) == 0 ? "internal " : "", gname(fname));
    for (i = 0; i <= maxarg; i++)
        fprintf(bodyfile, "%s%s %%a%d", i ? ", " : "", IPT, i);
    fprintf(bodyfile, ") {\nentry:\n");
    for (i = 0; i < ntemps; i++)
        if (temptype[i])
            fprintf(bodyfile, "  %%t%d = alloca %s, align %s\n  store %s %s, ptr %%t%d, align %s\n", i + 100, ltype(temptype[i]), lalign(temptype[i]),
                ltype(temptype[i]), temptype[i] == T_DOUBLE ? "0.0" : "0", i + 100, lalign(temptype[i]));
    fputs(buf, bodyfile);
    fprintf(bodyfile, "}\n\n");
    free(buf);
}

/* The IR on in, the module on out; flags: bit 0, the pointers are 32 bits; bit 1, the byte order of the target is not the host's. */
int
ir2ll_run(FILE *in, FILE *out, int p32)
{
    char line[600], *body = NULL, *data = NULL;
    size_t blen = 0, dlen = 0;
    FILE *bodyf;
    int i;

    inp = in;
    outp = out;
    {
        unsigned one = 1;

        tbe = (*(unsigned char *)&one == 0) ^ ((p32 >> 1) & 1);
    }
    ptr32 = p32 & 1;
    ntprotos = 0;
    thunks.n = 0;
    bodyf = open_memstream(&body, &blen);
    datafile = open_memstream(&data, &dlen);
    bodyfile = bodyf;
    for (;;) {
        int c = getc(inp);

        if (c == EOF)
            break;
        if (c == '*') {
            if (fgets(line, sizeof line, inp) == NULL)
                break;
            line[strcspn(line, "\n")] = '\0';
            passthrough(line + 1);
        } else if (c == '!') {
            if (fgets(line, sizeof line, inp) == NULL)
                break;
            {
                char hdr[700];

                snprintf(hdr, sizeof hdr, "!%s", line);
                flushdata();
                function(hdr);
            }
        } else {
            while (c != '\n' && c != EOF)
                c = getc(inp);
        }
    }
    flushdata();
    fclose(bodyf);
    fclose(datafile);
    fprintf(outp, "; generated by ir2ll\n\n");
    for (i = 0; i < called.n; i++) {
        const char *nm = called.v[i];

        if (inset(&funcs, nm))
            continue;
        {
            int j;

            for (j = 0; j < ntprotos; j++)
                if (strcmp(tprotos[j].name, nm) == 0)
                    break;
            if (j < ntprotos) {
                fprintf(outp, "%s\n", tprotos[j].decl);
                continue;
            }
        }
        if (strcmp(nm, "_setjmp") == 0 || strcmp(nm, "setjmp") == 0)
            fprintf(outp, "declare %s %s(...) returns_twice\n", IPT, gname(nm));
        else if (variadic_fixed(nm) > 0)
            fprintf(outp, "declare i32 %s(%s, ...)\n", gname(nm), IPT);
        else
            fprintf(outp, "declare %s %s(...)\n", iscfunc(nm) ? IPT : "i64", gname(nm));
    }
    for (i = 0; i < datarefs.n; i++)
        if (!inset(&datadefs, datarefs.v[i]) && !inset(&called, datarefs.v[i]) && !inset(&funcs, datarefs.v[i]))
            fprintf(outp, "%s = external global i8\n", gname(datarefs.v[i]));
    for (i = 0; i < ncomm; i++) {
        if (inset(&called, comms[i].name) || inset(&funcs, comms[i].name))
            continue;
        if (comms[i].local)
            fprintf(outp, "%s = internal global [%ld x i64] zeroinitializer, align 8\n", gname(comms[i].name), (comms[i].size + 7) / 8);
        else
            fprintf(outp, "%s = common global [%ld x i64] zeroinitializer, align 8\n", gname(comms[i].name), (comms[i].size + 7) / 8);
    }
    fputs(data, outp);
    for (i = 0; i < nvarrec; i++) {
        int len;
        char *nm = llstring(varrecs[i].name, &len);

        fprintf(outp, "@\"ivn.%d\" = private constant [%d x i8] c\"%s\"\n", i, len, nm);
        fprintf(outp, "@\"ivr.%d\" = internal global { ptr, %s, ptr } { ptr %s, %s %ld, ptr @\"ivn.%d\" }, section \"%simpvars\", align %d\n", i,
            IPT, gname(varrecs[i].sym), IPT, varrecs[i].size, i, IVSEG, ptr32 ? 4 : 8);    /* the alignment of the record, so that they follow each other as the array that impvarlist walks (12 bytes on a 32 bit machine, not 16) */
    }
    if (nvarrec > 0) {
        fprintf(outp, "@llvm.used = appending global [%d x ptr] [", nvarrec);
        for (i = 0; i < nvarrec; i++)
            fprintf(outp, "%sptr @\"ivr.%d\"", i ? ", " : "", i);
        fprintf(outp, "], section \"llvm.metadata\"\n");
    }
    fputs(body, outp);
    return 0;
}
