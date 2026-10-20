/*
 * ir2c -- the C-emitting back end: the pcc IR text that imp72s0 writes (-c),
 * read on standard input, becomes a C99 translation unit on standard output.
 *
 * For hosts pcc does not generate code for (macOS arm64 so far): the C
 * compiler of the host does the rest.  A word is int64_t, a real double,
 * an address an int64_t that is cast where it is dereferenced.  The
 * subroutines keep the C calling convention of the pcc output: parameters are
 * int64_t arguments (pointers to the IMP arguments), the result an int64_t.
 *
 * Input (see stage0/ir.c): "* text" assembler lines, which here are only the
 * data directives of trans.c; "! type flags prolab tmpbase lblbase name",
 * "^ n" (label), '" op type qual ...' (one tree node per line, preorder) and
 * "% autos retlab tmpend lblend name".
 *
 * GNU C attributes (common, aligned) are used for data; gcc and clang take them.
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

static FILE *out;                       /* statements of the function being written */
static FILE *datafile;                  /* data definitions */
static FILE *bodyfile;                  /* the functions */

static void
die(const char *msg, const char *line)
{
    fprintf(stderr, "ir2c: %s: %s\n", msg, line);
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

static const char *keywords[] = { "auto", "break", "case", "char", "const",
    "continue", "default", "do", "double", "else", "enum", "extern", "float",
    "for", "goto", "if", "inline", "int", "long", "register", "restrict",
    "return", "short", "signed", "sizeof", "static", "struct", "switch",
    "typedef", "union", "unsigned", "void", "volatile", "while", "main", NULL };

/* an assembler symbol as a C identifier */
static char *
cname(const char *s)
{
    char *r = malloc(strlen(s) * 2 + 8), *p = r;
    int i;

    for (i = 0; keywords[i] != NULL; i++)
        if (strcmp(s, keywords[i]) == 0)
            die("symbol is a C keyword", s);
    for (; *s; s++) {
        if (*s == '.') {
            *p++ = '_';
            *p++ = 'D';
        } else if (*s == '$') {
            *p++ = '_';
            *p++ = 'S';
        } else
            *p++ = *s;
    }
    *p = '\0';
    return r;
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

/* the runtime and C functions: they return a C long, not a word */
static int
iscfunc(const char *nm)
{
    return inset(&cfuncs, nm) || strcmp(nm, "ifix") == 0 || strcmp(nm, "fini") == 0 ||
        strncmp(nm, "imp_", 4) == 0;
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
        n->l = readnode(stdin);
        return n;
    }
    n->l = readnode(stdin);
    n->r = readnode(stdin);
    return n;
}

/* ---- C types and expressions --------------------------------------------- */

static const char *
ctype(int t)
{
    switch (t) {
    case T_UCHAR: return "uint8_t";
    case T_INT: return "int32_t";
    case T_ULONG: return "uint64_t";
    case T_DOUBLE: return "double";
    default: return "int64_t";          /* longs and every pointer */
    }
}

static int
isunsigned(int t)
{
    return t == T_ULONG || t == T_UCHAR;
}

static char *
cast(int t, char *e)
{
    return fmt("((%s)(%s))", ctype(t), e);
}

static char *expr(struct node *n);

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

static char *
lvalue(struct node *n)
{
    switch (n->op) {
    case NAME:
        add(&datarefs, n->name);
        return fmt("(*(%s *)((char *)%s + %lld))", ctype(n->type), cname(n->name), n->lval);
    case TEMP:
        return fmt("t%d", n->rval);
    case UMUL:
        return fmt("(*(%s *)(intptr_t)(%s))", ctype(n->type), expr(n->l));
    default:
        die("not an lvalue", "");
    }
    return NULL;
}

static char *
callexpr(struct node *n)
{
    struct node *fn = n->l, *args[64] = { NULL }, *list[64], *c;
    char *o;
    int nargs, k, top = -1, nl = 0;

    if (fn->op != ICON || fn->name == NULL)
        die("call of a computed function", "");
    add(&called, fn->name);
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
    o = fmt("%s(", cname(fn->name));
    for (k = 0; k < nargs; k++) {
        if (args[k] == NULL)
            die("missing argument", fn->name);
        o = fmt("%s%s(intptr_t)(%s)", o, k ? ", " : "", expr(args[k]));
    }
    o = fmt("%s)", o);
    return iscfunc(fn->name) ? fmt("((int64_t)(long)%s)", o) : o;
}

static char *
expr(struct node *n)
{
    int t = n->type, lt;
    char *l, *r;

    switch (n->op) {
    case ICON:
        if (n->name != NULL) {
            if (!inset(&called, n->name))
                add(&datarefs, n->name);
            return fmt("((int64_t)(uintptr_t)((char *)%s + %lld))", cname(n->name), n->lval);
        }
        return fmt("((%s)%lldLL)", ctype(t), n->lval);
    case NAME:
        return lvalue(n);
    case TEMP:
        return fmt("((%s)t%d)", ctype(t), n->rval);
    case REG: {
        int s = argslot(n->rval);

        if (s < 0)
            die("register not translated", "");
        if (s > maxarg)
            maxarg = s;
        return fmt("((%s)(uintptr_t)a%d)", ctype(t), s);   /* an address: zero extended */
    }
    case UMUL: {
        int sp = stackparam(n);

        if (sp >= 0) {
            if (sp > maxarg)
                maxarg = sp;
            return fmt("((%s)(uintptr_t)a%d)", ctype(t), sp);
        }
        return lvalue(n);
    }
    case CALL:
    case UCALL:
        return cast(t, callexpr(n));
    case PCONV:
    case SCONV:
        return cast(t, expr(n->l));
    case UMINUS:
        return cast(t, fmt("-%s", cast(t, expr(n->l))));
    case COMPL:
        return cast(t, fmt("~%s", cast(t, expr(n->l))));
    default:
        break;
    }
    l = expr(n->l);
    r = expr(n->r);
    lt = n->l->type;
    switch (n->op) {
    case PLUS: case MINUS: case MUL: case DIV: case MOD: case AND: case OR: case ER: {
        const char *op;

        switch (n->op) {
        case PLUS: op = "+"; break;
        case MINUS: op = "-"; break;
        case DIV: op = "/"; break;
        case MOD: op = "%"; break;
        case MUL: op = "*"; break;
        case AND: op = "&"; break;
        case OR: op = "|"; break;
        default: op = "^"; break;
        }
        if (t == T_DOUBLE && (n->op == MOD || n->op == AND || n->op == OR || n->op == ER))
            die("integer operator on a real", "");
        return cast(t, fmt("%s %s %s", cast(t, l), op, cast(t, r)));
    }
    case LS:
        return cast(t, fmt("(uint64_t)%s << (%s & 63)", cast(t, l), cast(T_ULONG, r)));
    case RS:
        if (isunsigned(t))
            return cast(t, fmt("(uint64_t)%s >> (%s & 63)", cast(t, l), cast(T_ULONG, r)));
        return cast(t, fmt("%s >> (%s & 63)", cast(t, l), cast(T_ULONG, r)));
    case EQ: case NE: case LE: case LT: case GE: case GT: {
        static const char *o[] = { "==", "!=", "<=", "<", ">=", ">" };
        int ct = lt == T_DOUBLE ? T_DOUBLE : isunsigned(lt) ? T_ULONG : lt == T_INT ? T_INT : T_LONG;

        return fmt("(%s %s %s)", cast(ct, l), o[n->op - EQ], cast(ct, r));
    }
    case ULE: case ULT: case UGE: case UGT: {
        static const char *o[] = { "<=", "<", ">=", ">" };

        return fmt("(%s %s %s)", cast(T_ULONG, l), o[n->op - ULE], cast(T_ULONG, r));
    }
    default:
        break;
    }
    fprintf(stderr, "ir2c: operator %d\n", n->op);
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
            fprintf(out, "    return (int64_t)(%s);\n", expr(n->r));
            return;
        }
        fprintf(out, "    %s = %s;\n", lvalue(d), cast(d->type, expr(n->r)));
        return;
    }
    case CBRANCH:
        fprintf(out, "    if (%s) goto L%lld;\n", expr(n->l), n->r->lval);
        return;
    case GOTO:
        fprintf(out, "    goto L%lld;\n", n->l->lval);
        return;
    case CALL:
    case UCALL:
        fprintf(out, "    %s;\n", callexpr(n));
        return;
    default:
        fprintf(stderr, "ir2c: statement %d\n", n->op);
        exit(1);
    }
}

/* ---- data ------------------------------------------------------------------ */

static char *dlabel, *dstring;
static long long dquads[4096];
static int ndq;

static void
flushdata(void)
{
    int i;

    if (dlabel != NULL) {
        if (dstring != NULL) {
            fprintf(datafile, "__attribute__((aligned(8))) char %s[] = \"%s\";\n", cname(dlabel), dstring);
            add(&datadefs, dlabel);
        } else if (ndq > 0) {
            fprintf(datafile, "__attribute__((aligned(8))) long long %s[] = {", cname(dlabel));
            for (i = 0; i < ndq; i++)
                fprintf(datafile, "%s%lldLL", i ? ", " : "", dquads[i]);
            fprintf(datafile, "};\n");
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
        if (inset(&locals, name))
            fprintf(datafile, "static long long %s[%ld] __attribute__((aligned(8)));\n",
                cname(name), (sz + 7) / 8);
        else
            fprintf(datafile, "__attribute__((common, aligned(8))) long long %s[%ld];\n",
                cname(name), (sz + 7) / 8);
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
    /* .text .data .align .balign .globl .type: nothing to say in C */
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
        int c = getc(stdin);

        if (c == EOF)
            die("function without an end", fname);
        if (c == '%') {
            if (fgets(line, sizeof line, stdin) == NULL)
                break;
            break;
        }
        if (c == '^') {
            int lab;

            if (fscanf(stdin, "%d", &lab) != 1)
                die("bad label", fname);
            while ((c = getc(stdin)) != '\n' && c != EOF)
                ;
            labels[nstm] = lab;
            stmts[nstm++] = NULL;
            continue;
        }
        if (c == '*') {                 /* a pass-through line inside the body */
            if (fgets(line, sizeof line, stdin) == NULL)
                break;
            line[strcspn(line, "\n")] = '\0';
            passthrough(line + 1);
            continue;
        }
        ungetc(c, stdin);
        stmts[nstm] = readnode(stdin);
        note_temp(stmts[nstm]);
        nstm++;
    }
    mem = open_memstream(&buf, &len);
    out = mem;
    for (i = 0; i < nstm; i++) {
        if (stmts[i] == NULL)
            fprintf(out, "L%d:;\n", labels[i]);
        else
            statement(stmts[i]);
    }
    fclose(mem);
    out = datafile;                     /* the function itself goes after the data: see main */
    add(&funcs, fname);
    fprintf(bodyfile, "int64_t %s(", cname(fname));
    if (maxarg < 0)
        fprintf(bodyfile, "void");
    for (i = 0; i <= maxarg; i++)
        fprintf(bodyfile, "%sintptr_t a%d", i ? ", " : "", i);
    fprintf(bodyfile, ")\n{\n");
    for (i = 0; i < ntemps; i++)
        if (temptype[i])
            fprintf(bodyfile, "    %s t%d = 0;\n", ctype(temptype[i]), i + 100);
    fputs(buf, bodyfile);
    fprintf(bodyfile, "    return 0;\n}\n\n");
    free(buf);
}

int
main(void)
{
    char line[600], *body = NULL, *data = NULL;
    size_t blen = 0, dlen = 0;
    FILE *bodyf = open_memstream(&body, &blen);
    int i;

    datafile = open_memstream(&data, &dlen);
    bodyfile = bodyf;
    {
        for (;;) {
            int c = getc(stdin);

            if (c == EOF)
                break;
            if (c == '*') {
                if (fgets(line, sizeof line, stdin) == NULL)
                    break;
                line[strcspn(line, "\n")] = '\0';
                passthrough(line + 1);
            } else if (c == '!') {
                if (fgets(line, sizeof line, stdin) == NULL)
                    break;
                {
                    char hdr[700];

                    snprintf(hdr, sizeof hdr, "!%s", line);
                    flushdata();
                    function(hdr);
                }
            } else {
                while (c != '\n' && c != EOF)
                    c = getc(stdin);
            }
        }
        flushdata();
    }
    fclose(bodyf);
    fclose(datafile);
    printf("#include <stdint.h>\n\n/* generated by ir2c; compile with -std=gnu99 (unprototyped extern declarations) */\n");
    for (i = 0; i < called.n; i++)
        printf("extern %s %s();\n", iscfunc(called.v[i]) ? "long" : "int64_t", cname(called.v[i]));
    for (i = 0; i < datarefs.n; i++)
        if (!inset(&datadefs, datarefs.v[i]) && !inset(&called, datarefs.v[i]) &&
            !inset(&funcs, datarefs.v[i]))
            printf("extern long long %s[];\n", cname(datarefs.v[i]));
    fputs(data, stdout);
    fputs(body, stdout);
    return 0;
}
