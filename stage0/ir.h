/*
 * ir.h -- the intermediate representation, as text.
 *
 * Trees are pcc pass 2 NODEs (mip/node.h): the same operators and types, the
 * same shapes as pass 1 leaves for the pass 2 compiler on amd64.  They are
 * written in the format mip/reader.c reads (`cc1` of pcc built with
 * --enable-twopass reads it from standard input and writes assembly).  The
 * writer in pass 1 of the fork is out of date, so this is written from the
 * reader.
 */
#ifndef IR_H
#define IR_H

#include <stdio.h>

/* operators (mip/node.h) */
enum {
    IR_NAME = 2, IR_ICON = 4, IR_FCON = 5, IR_REG = 6, IR_TEMP = 8,
    IR_PLUS = 10, IR_MINUS = 11, IR_DIV = 12, IR_MOD = 13, IR_MUL = 14,
    IR_AND = 15, IR_OR = 16, IR_ER = 17, IR_LS = 18, IR_RS = 19, IR_COMPL = 20,
    IR_UMUL = 23, IR_UMINUS = 24,
    IR_EQ = 25, IR_NE = 26, IR_LE = 27, IR_LT = 28, IR_GE = 29, IR_GT = 30,
    IR_ULE = 31, IR_ULT = 32, IR_UGE = 33, IR_UGT = 34,
    IR_CBRANCH = 35, IR_SCONV = 37, IR_PCONV = 38,
    IR_CALL = 41, IR_UCALL = 42, IR_CM = 48, IR_ASSIGN = 49, IR_GOTO = 54, IR_FUNARG = 57
};

/* types (mip/manifest.h) */
enum {
    T_UCHAR = 3, T_INT = 6, T_UINT = 7, T_LONG = 8, T_ULONG = 9, T_DOUBLE = 13, T_VOID = 19,
    T_PTR = 0x20, T_FTN = 0x40,
    T_PLONG = T_LONG | T_PTR,                   /* pointer to long */
    T_PFTN = (((T_FTN | T_LONG) & ~0x1f) << 2) | T_PTR | T_LONG
};

/*
 * The machine the IR is lowered for: the numbers of the registers that arguments
 * and results travel in (arch/<target>/macdefs.h of pcc), and how a function is
 * declared.  "amd64" is also what the C back end reads.
 */
struct irtarget {
    const char *name;
    int narg;                   /* arguments in registers */
    int arg[8];
    int ret, fp, sp;
    const char *functype;       /* the .type directive's operand */
    int stackarg0;              /* bytes above fp where stack arguments start */
    int autos0;                 /* bytes of the frame before the first automatic (AUTOINIT) */
    int argslot;                /* bytes a stack argument takes */
    int fnargs;                 /* arguments are FUNARG nodes pushed by pass 2 (i386), not registers */
    int pushargs;               /* arguments beyond the registers are pushed by pass 2 as FUNARG (arm) */
    int ippmem;                 /* the IR has the target's extra lines after the prolog and epilog */
    int wordll;                 /* the 64 bit word is a long long: long is 32 bits (i386) */
};
extern const struct irtarget *irtgt;
int ir_settarget(const char *name);      /* 0 if there is no such target */

#define R_RDI (irtgt->arg[0])
#define R_RSI (irtgt->arg[1])
#define R_RDX (irtgt->arg[2])
#define R_RCX (irtgt->arg[3])
#define R_R8  (irtgt->arg[4])
#define R_R9  (irtgt->arg[5])
#define R_RAX (irtgt->ret)
#define R_RBP (irtgt->fp)
#define R_RSP (irtgt->sp)

struct irn {
    int op, type;
    long long lval;
    int rval;
    int qual;                   /* the node's qualifier: bytes of arguments pushed, of a call */
    const char *name;
    struct irn *l, *r;
};

struct irn *ir_new(int op, int type, struct irn *l, struct irn *r);
struct irn *ir_icon(long long v, int type);
struct irn *ir_sym(const char *name, long long off, int type);  /* &name+off */
struct irn *ir_name(const char *name, long long off, int type); /* name+off */
struct irn *ir_temp(int n, int type);
struct irn *ir_reg(int n, int type);
struct irn *ir_un(int op, int type, struct irn *l);
struct irn *ir_bin(int op, int type, struct irn *l, struct irn *r);
struct irn *ir_assign(struct irn *l, struct irn *r);
struct irn *ir_deref(struct irn *addr, int type);

void ir_open(FILE *f);
int ir_newlabel(void);
int ir_newtemp(void);
void ir_func_begin(const char *name, int visible);
void ir_stmt(struct irn *n);
void ir_label(int n);
void ir_goto(int label);
void ir_cbranch(int op, struct irn *l, struct irn *r, int label);
void ir_func_end(int retlab, int rettemp);
void ir_pass(const char *fmt, ...);             /* a line for the assembler */
void ir_varrec(const char *sym, long size);     /* where a variable of one of the compiler's own modules is (IMP72MODULE), for the image of U and V */

/* for a front end that makes the IR directly */
struct irn *ir_xshift(int right, struct irn *l, struct irn *r);
struct irn *ir_xcall(struct irn *fn, struct irn **a, int n, int cret);
struct irn *ir_param(int k);
struct irn *ir_todouble(struct irn *w, int k);
struct irn *ir_fromdouble(struct irn *d);
extern int ir_usedfs;

#endif
