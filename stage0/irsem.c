/*
 * irsem.c -- the kernel's side of the code generator that compiles to pcc IR (src/ir/*.imp, docs/porting/ir-machine.md).
 *
 * The code generating routines, the register and declaration routines and the emitter are IMP modules, compiled by imp72stage0 and
 * linked into imp72d by tools/direct.sh with their symbols renamed impl_NAME.  What is here: the registration of the routines by name
 * (the table of semantic routines is a table of C function pointers, so each routine gets a wrapper that calls the IMP one, which
 * takes its arguments by reference), the routines of the kernel the IMP code calls by value (CCALL), and the parts of the semantic
 * routines that need the C library (floating constants, LET, the table of the names called with values).  It stands in, in imp72d,
 * for codegen.c, impsem.c, trans.c and pcode.c.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "imp.h"
#include "ir.h"

/* the IMP routines, by reference */
#define R1(n) extern word impl_##n(word *);
#define R2(n) extern word impl_##n(word *, word *);
#define R3(n) extern word impl_##n(word *, word *, word *);
#define R4(n) extern word impl_##n(word *, word *, word *, word *);
#define R5(n) extern word impl_##n(word *, word *, word *, word *, word *);
R1(iinit) R1(areg) R1(reg2) R2(areg1) R1(name) R1(val) R1(sval) R1(stackup) R1(swtch) R1(nsubp) R1(hisreg)
R1(pairfirst) R1(subreg) R1(tmpreg) R1(samereg) R2(regeq) R2(regsim) R1(reg0) R3(rmatch) R1(subpr0) R1(subprm) R1(subtmpf)
R1(subbeg) R1(subend) R1(remot) R1(callme) R1(reg2s) R2(decl) R1(declare)
R1(unpck) R1(regof) R1(fetch) R3(addop) R3(dewop) R1(tag) R2(store) R2(mcode) R1(copyco)
R3(hook) R1(anycode) R3(dewfun) R1(freeze) R1(oneword) R1(coppy) R1(subrcall) R1(return) R1(float) R1(fix) R1(datast)
R2(subrpar) R5(prinpar) R2(princal) R1(ojumpop) R1(addr) R5(conop) R4(cjump) R1(jop) R3(moveblk)
R2(subcall) R2(subscript) R4(bytep) R1(demit)

extern word arr, narr, regn, co, nco, anco;

#define ONE(f, g) static word w_##f(const word *p) { word a = p[0]; return impl_##g(&a); }
#define TWO(f, g) static word w_##f(const word *p) { word a = p[0], b = p[1]; return impl_##g(&a, &b); }
#define THREE(f, g) static word w_##f(const word *p) { word a = p[0], b = p[1], c = p[2]; return impl_##g(&a, &b, &c); }
#define FOUR(f, g) static word w_##f(const word *p) { word a = p[0], b = p[1], c = p[2], d = p[3]; return impl_##g(&a, &b, &c, &d); }
#define FIVE(f, g) static word w_##f(const word *p) { word a = p[0], b = p[1], c = p[2], d = p[3], e = p[4]; \
    return impl_##g(&a, &b, &c, &d, &e); }

ONE(name, name) TWO(areg1, areg1) ONE(areg, areg) ONE(reg2, reg2) ONE(val, val) ONE(sval, sval) TWO(decl, decl)
ONE(declare, declare) ONE(stackup, stackup) ONE(switch, swtch) ONE(subbeg, subbeg) ONE(subend, subend) ONE(subpr0, subpr0)
ONE(remot, remot) ONE(callme, callme) ONE(reg2s, reg2s)
TWO(store, store) ONE(fetch, fetch) ONE(regof, regof) THREE(dewop, dewop) THREE(addop, addop) ONE(tag, tag) THREE(dewfun, dewfun)
THREE(hook, hook) ONE(freeze, freeze) ONE(subrcall, subrcall) ONE(return, return) TWO(subrpar, subrpar) FIVE(conop, conop)
ONE(float, float) ONE(fix, fix) ONE(ojumpop, ojumpop) ONE(datast, datast) FIVE(prinpar, prinpar) TWO(princal, princal)
ONE(addr, addr) ONE(copy, coppy) TWO(subscript, subscript) TWO(subcall, subcall) FOUR(bytep, bytep) FOUR(cjump, cjump)
ONE(jop, jop) THREE(moveblk, moveblk)

/* the parts of the kernel that call the code generator, by value: the cotree matcher compares and copies code, the statement
 * reducer tells whether there is any */
word oneword(word s) { return impl_oneword(&s); }
word mcode(word s, word t) { return impl_mcode(&s, &t); }
void copyco(word s) { impl_copyco(&s); }
word anyco(void) { word z = 0; return impl_anycode(&z); }
word subtmpf(void) { word z = 0; return impl_subtmpf(&z); }
word impsem_narr(void) { word z = 0; impl_iinit(&z); return narr; }

/* LETSYN(S): `LET X = Y` (see impsem.c) */
word
letsyn(word s)
{
    word max = FREE(s + 1), j, k;

    synt(enstack(dir("VBL")), 0);
    sytrm(enstack(-FREE(max)));
    syntax(1);
    for (j = max; (k = FREE(j + 1)) != 0; j++)
        FREES(j, k);
    FREES(j, 0);
    equose(s);
    newlist(0);
    semantics();
    return enstack(0);
}

/* CVAL(A): the name A is a C function that takes its arguments by value */
static word cvalnames[64];
static int ncval;

word
ccall_mark(word a)
{
    word d = L18(FREE(a));
    int i;

    for (i = 0; i < ncval; i++)
        if (cvalnames[i] == d)
            return a;
    if (ncval < 64)
        cvalnames[ncval++] = d;
    return a;
}

int
is_cval(word d)
{
    int i;

    for (i = 0; i < ncval; i++)
        if (cvalnames[i] == d)
            return 1;
    return 0;
}

/* FCON(A,B,E): the floating constant A.B"E: the nearest IEEE 754 double; its bits are the word (no encoding here) */
word
fcon(word a, word b, word e)
{
    char ai[64], bi[64], txt[160];
    const char *p;
    int ex = 0;
    double v;
    word bits;

    dname_str(L18(FREE(a)), ai, sizeof ai);
    dname_str(L18(FREE(b)), bi, sizeof bi);
    for (p = ai; *p; p++)
        if (*p < '0' || *p > '9')
            goto bad;
    for (p = bi; *p; p++)
        if (*p < '0' || *p > '9')
            goto bad;
    if (e) {
        if ((FREE(e) & 077) != 4)
            goto bad;
        ex = (int)FREE(e + 1);
        if (ex > 308 || ex < -330) {
            imp_error(1, "EXPONENT OUT OF RANGE.");
            ex = 0;
        }
    }
    snprintf(txt, sizeof txt, "%s.%se%d", ai[0] ? ai : "0", bi[0] ? bi : "0", ex);
    v = strtod(txt, NULL);
    goto done;
bad:
    imp_error(1, "ILLEGAL CHARACTER IN FLOATING CONSTANT.");
    v = 0;
done:
    memcpy(&bits, &v, sizeof bits);
    FREES(a, 0204);
    FREES(a + 1, bits);
    return a;
}

static word w_letsyn(const word *p) { return letsyn(p[0]); }
static word w_cval(const word *p) { return ccall_mark(p[0]); }
static word w_fcon(const word *p) { return fcon(p[0], p[1], p[2]); }

/* ---- what the IMP code calls by value (CCALL) ---------------------------------------------------------------------------------- */

/* an integer as the bits of the double, and back (a REAL is the bits of an IEEE double) */
word
diri2d(word i)
{
    double d = (double)i;
    word w;

    memcpy(&w, &d, sizeof w);
    return w;
}

word
dird2i(word w)
{
    double d;

    memcpy(&d, &w, sizeof d);
    return (word)d;
}

/* The assembler name of directory entry v: lower case, characters an assembler would not take as _xx. */
static char **symcache;
static word nsymcache;

const char *
irsymname(word v)
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

/* 0 a variable or function, 1 a text constant, 2 a number */
word
irkind(word v)
{
    char buf[40];

    if (v == 0)
        return 0;
    dname_str(v, buf, sizeof buf);
    if (buf[0] == '\'')
        return 1;
    if (buf[0] >= '0' && buf[0] <= '9' && convc(v) == -1)
        return 2;
    return 0;
}

/* The value of a text constant: its first 8 characters, the first in the low byte, so that the word is the start of the string in memory. */
word
irstrval(word v)
{
    char buf[40];
    uint64_t x = 0;
    int k;

    dname_str(v, buf, sizeof buf);
    for (k = 1; k < 9 && buf[k]; k++)
        x |= (uint64_t)(unsigned char)buf[k] << (8 * (k - 1));
    return (word)x;
}

const char *
irlabname(word l)
{
    char *s = malloc(24);

    sprintf(s, ".L%ld", (long)l);
    return s;
}

/* the body of a pooled constant: the text, or the number */
word
irpoolemit(word v)
{
    char buf[400];

    if (irkind(v) == 1) {
        char *s;
        size_t k, m = 0;

        dname_str(v, buf, sizeof buf);
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
        ir_pass(" .string \"%s\"", s);
        free(s);
        ir_pass(" .balign 8");
    } else {
        convc(v);
        ir_pass(" .quad %lld", (long long)convc_const);
    }
    return 0;
}

/* 1 for the print routine, 2 for the read routine, else 0 */
word
irfnkind(word v)
{
    const char *nm = irsymname(v);

    return strcmp(nm, "prn_2e") == 0 ? 1 : strcmp(nm, "red_2e") == 0 ? 2 : 0;
}

word ircval(word v) { return is_cval(v); }

/* a routine of the runtime library, which returns a C long, not a word */
word
irruntime(word v)
{
    const char *nm = irsymname(v);

    return strcmp(nm, "ifix") == 0 || strcmp(nm, "fini") == 0 || strncmp(nm, "imp_", 4) == 0;
}

word irreset(word z) { (void)z; ir_usedfs = 0; return 0; }
word irfsused(word z) { (void)z; return ir_usedfs; }
word irflush(word z) { (void)z; fflush(stdout); return 0; }


/* ---- the IR builders of ir.c, by value, under names IMP can write (no underscore) ---------------------------------------------- */

#define N(x) ((struct irn *)(x))
#define W(x) ((word)(x))
word ixnewtemp(word z) { (void)z; return W(ir_newtemp()); }
word ixnewlabel(word z) { (void)z; return W(ir_newlabel()); }
word ixtemp(word n, word t) { return W(ir_temp((int)n, (int)t)); }
word ixicon(word v, word t) { return W(ir_icon(v, (int)t)); }
word ixsym(word nm, word off, word t) { return W(ir_sym((const char *)nm, off, (int)t)); }
word ixname(word nm, word off, word t) { return W(ir_name((const char *)nm, off, (int)t)); }
word ixun(word op, word t, word l) { return W(ir_un((int)op, (int)t, N(l))); }
word ixbin(word op, word t, word l, word r) { return W(ir_bin((int)op, (int)t, N(l), N(r))); }
word ixassign(word l, word r) { return W(ir_assign(N(l), N(r))); }
word ixderef(word a, word t) { return W(ir_deref(N(a), (int)t)); }
word ixstmt(word n) { ir_stmt(N(n)); return 0; }
word ixlabel(word n) { ir_label((int)n); return 0; }
word ixgoto(word n) { ir_goto((int)n); return 0; }
word ixcbranch(word op, word l, word r, word lab) { ir_cbranch((int)op, N(l), N(r), (int)lab); return 0; }
word ixfuncbegin(word nm, word vis) { ir_func_begin((const char *)nm, (int)vis); return 0; }
word ixfuncend(word retlab, word rettemp) { ir_func_end((int)retlab, (int)rettemp); return 0; }
word ixpass(word fmt, word a, word b, word c) { ir_pass((const char *)fmt, a, b, c); return 0; }
word ixshift(word right, word l, word r) { return W(ir_xshift((int)right, N(l), N(r))); }
word ixcall(word fn, word a, word n, word cret) { return W(ir_xcall(N(fn), (struct irn **)a, (int)n, (int)cret)); }
word ixparam(word k) { return W(ir_param((int)k)); }
word ixtodouble(word w) { return W(ir_todouble(N(w))); }
word ixfromdouble(word d) { return W(ir_fromdouble(N(d))); }

/* by reference, for the IMP modules (the glue calls of src/ir) */
word irstcon(word *v) { return stcon(*v); }
word irconvc(word *c) { return convc(*c); }
word irconst(word *z) { (void)z; return convc_const; }
word irdir0(word *z) { (void)z; return dir("0"); }
word irnewnamex(word *s) { return newname((const char *)*s); }
word irdirsx(word *s) { return dir((const char *)*s); }

/* ---- the translator's interface, for the driver and sem.c ---------------------------------------------------------------------- */

static FILE *irfile;

void
trans_open(FILE *f)
{
    irfile = f;
    ir_open(f);
}

void
trans_unit(word a)
{
    word x = a;

    if (irfile == NULL)
        return;
    impl_demit(&x);
    fflush(irfile);
}

/* the list of the code of an entry (/A): one line for each record of the chain (docs/porting/ir-machine.md) */
void
pcode(word a)
{
    static const char *const opn[72] = { "?", "LOAD", "NEG", "COMPL", "ADDR", "STORE", "ADD", "SUB", "MUL", "DIV", "MOD",
        "AND", "ANDCA", "ANDCM", "ANDCB", "IOR", "ORCA", "ORCM", "ORCB", "XOR", "EQV", "LSH", "ASH", "ROT", "?", "?", "?", "?", "?",
        "?", "FADD", "FSUB", "FMUL", "FDIV", "FLOAT", "FIX", "FNEG", "?", "?", "?", "JUMP", "JEQ", "JLT", "JLE", "JGE", "JGT",
        "JNE", "?", "?", "?", "?", "FJEQ", "FJLT", "FJLE", "FJGE", "FJGT", "FJNE", "?", "?", "?", "?", "?", "CALLB", "ARG",
        "CALLE", "PITEM", "?", "?", "?", "?", "ENTRY", "RET" };
    word r = (word)((uword)FREE(a + 2) >> 32);
    FILE *f = imp_out();

    while (r) {
        word h = FREE(co + r), op = (h >> 8) & 0xff, kind = h & 0xff;
        char nm[16], d[40];

        if (kind == 2)
            fprintf(f, "%6ld  TAG    %s\n", (long)r, (dname_str(FREE(co + r + 4), d, sizeof d), d));
        else if (kind == 3)
            fprintf(f, "%6ld  WORD   %ld\n", (long)r, (long)FREE(co + r + 3));
        else {
            if (op < 72 && opn[op][0] != '?')
                snprintf(nm, sizeof nm, "%s", opn[op]);
            else
                snprintf(nm, sizeof nm, "%ld", (long)op);
            fprintf(f, "%6ld  %-6s R%ld  V %lo,%lo  X %ld\n", (long)r, nm, (long)FREE(co + r + 1), (long)(FREE(co + r + 2) & 0777777777777L),
                (long)(FREE(co + r + 3) & 0777777777777L), (long)FREE(co + r + 4));
        }
        r = (word)((uword)h >> 32);
    }
}

void
impsem_init(void)
{
    dsem("NAME", w_name);
    dsem("AREG1", w_areg1);
    dsem("AREG", w_areg);
    dsem("REG2", w_reg2);
    dsem("VAL", w_val);
    dsem("SVAL", w_sval);
    dsem("DECL", w_decl);
    dsem("DECLARE", w_declare);
    dsem("STACKUP", w_stackup);
    dsem("SWITCH", w_switch);
    dsem("LETSYN", w_letsyn);
    dsem("SUBBEG", w_subbeg);
    dsem("SUBEND", w_subend);
    dsem("SUBPR0", w_subpr0);
    dsem("REMOT", w_remot);
    dsem("CALLME", w_callme);
    dsem("CVAL", w_cval);
    dsem("FCON", w_fcon);
    dsem("REG2S", w_reg2s);
}

void
codegen_init(void)
{
    static int done;

    if (done)
        return;
    done = 1;
    dsem("STORE", w_store);
    dsem("FETCH", w_fetch);
    dsem("REGOF", w_regof);
    dsem("DEWOP", w_dewop);
    dsem("ADDOP", w_addop);
    dsem("TAG", w_tag);
    dsem("DEWFUN", w_dewfun);
    dsem("HOOK", w_hook);
    dsem("FREEZE", w_freeze);
    dsem("SUBRCALL", w_subrcall);
    dsem("RETURN", w_return);
    dsem("SUBRPAR", w_subrpar);
    dsem("CONOP", w_conop);
    dsem("FLOAT", w_float);
    dsem("FIX", w_fix);
    dsem("OJUMPOP", w_ojumpop);
    dsem("DATAST", w_datast);
    dsem("PRINPAR", w_prinpar);
    dsem("PRINCAL", w_princal);
    dsem("ADDR", w_addr);
    dsem("COPY", w_copy);
    dsem("SUBSCRIPT", w_subscript);
    dsem("SUBCALL", w_subcall);
    dsem("BYTEP", w_bytep);
    dsem("CJUMP", w_cjump);
    dsem("JOP", w_jop);
    dsem("MOVEBLK", w_moveblk);
}
