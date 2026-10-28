/*
 * ir2ll -- the LLVM back end: the pcc IR text that imp72s0 writes (-c), read on standard input, becomes a module of LLVM IR on standard output, for
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
enum { T_UCHAR = 3, T_INT = 6, T_LONG = 8, T_ULONG = 9, T_DOUBLE = 13, T_VOID = 19 };
enum { R_RAX = 0, R_RBP = 6, R_RSP = 7 };

struct node {
    int op, type;
    long long lval;
    int rval;
    char *name;
    struct node *l, *r;
};

static FILE *out;                       /* the function being written */
static FILE *datafile;                  /* the global variables */
static FILE *bodyfile;                  /* the functions */
static int ptr32;                       /* the host's pointers are 32 bits */
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
    char *r = malloc(strlen(s) + 1);

    strcpy(r, s);
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
    if (inset(&funcs, nm))                      /* defined in IMP here (imp_main, say): it returns a word */
        return 0;
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
    case T_UCHAR: return "i8";
    case T_INT: return "i32";
    case T_DOUBLE: return "double";
    default: return "i64";              /* longs and every pointer */
    }
}

static int
bits(int t)
{
    switch (t) {
    case T_UCHAR: return 8;
    case T_INT: return 32;
    default: return 64;
    }
}

static int
isunsigned(int t)
{
    return t == T_ULONG || t == T_UCHAR;
}

static const char *
lalign(int t)
{
    switch (t) {
    case T_UCHAR: return "1";
    case T_INT: return "4";
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
    if (from == T_DOUBLE) {
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

/* a call: the value is an i64 */
static char *
callexpr(struct node *n)
{
    struct node *fn = n->l, *args[64] = { NULL }, *list[64], *c;
    char *vals[64], *r, *o;
    int nargs, k, top = -1, nl = 0, nfix, cf;

    if (fn->op != ICON || fn->name == NULL)
        die("call of a computed function", "");
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
            if (!inset(&called, n->name))
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
        p += sprintf(p, "\\%02X", c & 255);
        n++;
    }
    p += sprintf(p, "\\00");
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
            fprintf(datafile, "%s = %s global [%d x i64] [", gname(dlabel), link, ndq);
            for (i = 0; i < ndq; i++)
                fprintf(datafile, "%si64 %lld", i ? ", " : "", dquads[i]);
            fprintf(datafile, "], align 8\n");
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
        if (ndq < 4096)
            dquads[ndq++] = atoll(line + 6);
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

/* The IR on in, the module on out; p32: the host's pointers are 32 bits. */
int
ir2ll_run(FILE *in, FILE *out, int p32)
{
    char line[600], *body = NULL, *data = NULL;
    size_t blen = 0, dlen = 0;
    FILE *bodyf;
    int i;

    inp = in;
    outp = out;
    ptr32 = p32;
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
        fprintf(outp, "@\"ivr.%d\" = internal global { ptr, %s, ptr } { ptr %s, %s %ld, ptr @\"ivn.%d\" }, section \"impvars\", align 8\n", i,
            IPT, gname(varrecs[i].sym), IPT, varrecs[i].size, i);
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
