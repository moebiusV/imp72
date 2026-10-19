/*
 * ir.c -- writing pcc IR text.  See ir.h.
 *
 * A function is
 *
 *      * the assembler directives that begin it
 *      ! type flags prolab tmpbase lblbase name
 *      ^ prolab
 *      " ...                   one tree per statement, a node a line, prefix
 *      ^ label                 order, `" op type qual [lval] [rval|name]`
 *      ...
 *      ^ retlab
 *      " ASSIGN(REG rax, TEMP ret)
 *      % autos retlab tmpend lblend name
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"

static FILE *out;
static int nlabel = 100, ntemp;
static int tmpbase, lblbase;

static const struct irtarget targets[] = {
    { "amd64", 6, { 5, 4, 1, 2, 8, 9 }, 0, 6, 7, "@function", 16, 0 },
    { "aarch64", 8, { 0, 1, 2, 3, 4, 5, 6, 7 }, 0, 26, 27, "%function", 0x1000000, 16 },
};
const struct irtarget *irtgt = &targets[0];

int
ir_settarget(const char *name)
{
    size_t i;

    for (i = 0; i < sizeof targets / sizeof targets[0]; i++)
        if (strcmp(targets[i].name, name) == 0) {
            irtgt = &targets[i];
            return 1;
        }
    return 0;
}
static const char *fname;

struct irn *
ir_new(int op, int type, struct irn *l, struct irn *r)
{
    struct irn *n = calloc(1, sizeof *n);

    if (n == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    n->op = op;
    n->type = type;
    n->l = l;
    n->r = r;
    return n;
}

struct irn *
ir_icon(long long v, int type)
{
    struct irn *n = ir_new(IR_ICON, type, NULL, NULL);

    n->lval = v;
    return n;
}

struct irn *
ir_sym(const char *name, long long off, int type)
{
    struct irn *n = ir_icon(off, type);

    n->name = name;
    return n;
}

struct irn *
ir_name(const char *name, long long off, int type)
{
    struct irn *n = ir_new(IR_NAME, type, NULL, NULL);

    n->lval = off;
    n->name = name;
    return n;
}

struct irn *
ir_temp(int num, int type)
{
    struct irn *n = ir_new(IR_TEMP, type, NULL, NULL);

    n->rval = num;
    return n;
}

struct irn *
ir_reg(int num, int type)
{
    struct irn *n = ir_new(IR_REG, type, NULL, NULL);

    n->rval = num;
    return n;
}

struct irn *
ir_un(int op, int type, struct irn *l)
{
    return ir_new(op, type, l, NULL);
}

struct irn *
ir_bin(int op, int type, struct irn *l, struct irn *r)
{
    return ir_new(op, type, l, r);
}

struct irn *
ir_assign(struct irn *l, struct irn *r)
{
    return ir_new(IR_ASSIGN, l->type, l, r);
}

struct irn *
ir_deref(struct irn *addr, int type)
{
    return ir_un(IR_UMUL, type, addr);
}

void
ir_open(FILE *f)
{
    out = f;
}

int ir_newlabel(void) { return nlabel++; }
int ir_newtemp(void) { return tmpbase + ntemp++; }

/* write a tree, preorder, a node to a line */
static void
put(struct irn *n)
{
    int ty;

    fprintf(out, "\" %d %d 0", n->op, n->type);
    switch (n->op) {
    case IR_NAME:
    case IR_ICON:
        fprintf(out, " %lld %s\n", n->lval, n->name != NULL ? n->name : "");
        return;
    case IR_REG:
    case IR_TEMP:
        fprintf(out, " 0 %d\n", n->rval);
        return;
    default:
        break;
    }
    ty = n->r != NULL ? 2 : 1;
    if (ty == 1)
        fprintf(out, " %d", n->rval);
    fputc('\n', out);
    put(n->l);
    if (n->r != NULL)
        put(n->r);
}

static void
tfree(struct irn *n)
{
    if (n == NULL)
        return;
    tfree(n->l);
    tfree(n->r);
    free(n);
}

void
ir_stmt(struct irn *n)
{
    put(n);
    tfree(n);
}

void
ir_label(int n)
{
    fprintf(out, "^ %d\n", n);
}

void
ir_goto(int label)
{
    ir_stmt(ir_un(IR_GOTO, T_INT, ir_icon(label, T_INT)));
}

/* if (l op r) goto label */
void
ir_cbranch(int op, struct irn *l, struct irn *r, int label)
{
    ir_stmt(ir_bin(IR_CBRANCH, T_INT, ir_bin(op, T_INT, l, r),
        ir_icon(label, T_INT)));
}

void
ir_pass(const char *fmt, ...)
{
    va_list ap;

    fputs("* ", out);
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
    fputc('\n', out);
}

static int prolab;

/* Begin a function: its directives, the prolog, and the label after it. */
void
ir_func_begin(const char *name, int visible)
{
    fname = name;
    tmpbase = 100;
    ntemp = 0;
    lblbase = nlabel;
    prolab = nlabel++;
    ir_pass("\t.text");
    ir_pass("\t.align 4");
    if (visible)
        ir_pass("\t.globl %s", name);
    ir_pass("\t.type %s,%s", name, irtgt->functype);
    ir_pass("%s:", name);
    fprintf(out, "! %d %d %d %d %d %s\n", T_LONG, visible ? 1 : 0, prolab,
        tmpbase, lblbase, name);
    ir_label(prolab);
}

/*
 * End it.  Every return has stored its value in the temp `rettemp` and
 * jumped to `retlab` (rettemp < 0: the function returns no value).
 */
void
ir_func_end(int retlab, int rettemp)
{
    ir_label(retlab);
    if (rettemp >= 0)
        ir_stmt(ir_assign(ir_reg(R_RAX, T_LONG), ir_temp(rettemp, T_LONG)));
    fprintf(out, "%% %d %d %d %d %s\n", irtgt->autos0, retlab, tmpbase + ntemp + 1, nlabel + 1,
        fname);
}
