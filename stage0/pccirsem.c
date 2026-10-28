/*
 * pccirsem.c -- the kernel's side of the code generator that compiles to pcc IR (src/pccir/*.imp, docs/porting/pccir-machine.md).
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

const int imp_pccir = 1;                /* the compiler with this code generator (the driver reads it) */

/* the IMP routines, by reference */
/* the IMP routines (src/pccir/*.imp, symbols renamed impl_NAME): they take their arguments by reference; the table of semantic routines calls them with
 * pointers into the parameters (dsemi) */
#define RT(n) extern word impl_##n();
RT(impsem) RT(codei) RT(addop) RT(addr) RT(anycode) RT(areg) RT(areg1) RT(assemc) RT(bytep) RT(callme) 
RT(cjump) RT(conop) RT(coppy) RT(copyco) RT(datast) RT(decl) RT(declare) RT(demit) 
RT(dewfun) RT(dewop) RT(enlist) RT(fetch) RT(fix) RT(float) RT(freeze) RT(getlist) 
RT(hisreg) RT(hook) RT(iinit) RT(jop) RT(mcode) RT(moveblk) RT(n) RT(name) 
RT(newlist) RT(nsubp) RT(ojumpop) RT(oneword) RT(pairfirst) RT(princal) RT(prinpar) RT(reg0) 
RT(reg2) RT(reg2s) RT(regeq) RT(regof) RT(regsim) RT(remot) RT(return) RT(rmatch) 
RT(samereg) RT(stackup) RT(store) RT(subbeg) RT(subcall) RT(subend) RT(subpr0) RT(subprm) 
RT(subrcall) RT(subreg) RT(subrpar) RT(subscript) RT(subtmpf) RT(sval) RT(swtch) RT(tag) 
RT(tmpreg) RT(unpck) RT(val) RT(pcode) 

extern word arr, narr, regn, co, nco, anco;

/* the parts of the kernel that call the code generator, by value: the cotree matcher compares and copies code, the statement
 * reducer tells whether there is any */
word oneword(word s) { return impl_oneword(&s); }
word mcode(word s, word t) { return impl_mcode(&s, &t); }
void copyco(word s) { impl_copyco(&s); }
word anyco(void) { word z = 0; return impl_anycode(&z); }
word subtmpf(void) { word z = 0; return impl_subtmpf(&z); }
word imponeword(word *s) { return impl_oneword(s); }       /* entree.imp's call */
word newlist(word s) { return impl_newlist(&s); }       /* the lists are src/pccir/impsem.imp's; C callers use these */
word enlist(word s) { return impl_enlist(&s); }
word getlist(word s) { return impl_getlist(&s); }
word impsem_narr(void) { word z = 0; impl_iinit(&z); return narr; }

/* STRTOD for FCON (src/pccir/impsem.imp): the double of the text s, as its bits in *out (a call cannot carry a double, nor on a 32 bit machine a 64 bit word) */
void
impstrtod(const char *s, int64_t *out)
{
    double v = strtod(s, NULL);
    int64_t bits;

    memcpy(&bits, &v, sizeof bits);
    *out = bits;
}

/* ---- what the IMP code calls by value (CCALL) ---------------------------------------------------------------------------------- */

/* An integer as the bits of the double, and back (a REAL is the bits of an IEEE double).  A 64 bit value crosses a CCALL as two longs (a 32 bit
 * machine passes a long as 32 bits): its low half, which is never negative, and its high half; the result comes by halves too. */
static int64_t
join(long lo, long hi)
{
    return (int64_t)(((uint64_t)(uint32_t)hi << 32) | (uint64_t)(uint32_t)lo);
}

static long diresult[2];

long
diri2d(long lo, long hi)
{
    double d = (double)join(lo, hi);
    uint64_t w;

    memcpy(&w, &d, sizeof w);
    diresult[0] = (long)(w & 0xFFFFFFFFU);
    diresult[1] = (long)(w >> 32);
    return diresult[0];
}

long
dird2i(long lo, long hi)
{
    uint64_t w = (uint64_t)join(lo, hi);
    double d;
    int64_t v;

    memcpy(&d, &w, sizeof d);
    v = (int64_t)d;
    diresult[0] = (long)((uint64_t)v & 0xFFFFFFFFU);
    diresult[1] = (long)((uint64_t)v >> 32);
    return diresult[0];
}

long dirhigh(long z) { (void)z; return diresult[1]; }              /* the high half of the last result of diri2d, dird2i or irstrval */

/* The assembler name of directory entry v: lower case, characters an assembler would not take as _xx. */
static char **symcache;
static long nsymcache;

const char *
irsymname(long v)
{
    char buf[128], out[300];
    size_t i, k = 0;

    if (v >= nsymcache) {
        long n = v + 64;

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
long
irkind(long v)
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

/* The value of a text constant: its first 8 characters, the first in the low byte, so that the word is the start of the string in memory.  The low half is
 * the result; dirhigh() has the high one. */
long
irstrval(long v)
{
    char buf[40];
    unsigned char b[8] = { 0 };
    uint64_t x;
    int k;

    dname_str(v, buf, sizeof buf);
    for (k = 1; k < 9 && buf[k]; k++)
        b[k - 1] = (unsigned char)buf[k];
    memcpy(&x, b, sizeof x);                    /* in memory order */
    diresult[0] = (long)(x & 0xFFFFFFFFU);
    diresult[1] = (long)(x >> 32);
    return diresult[0];
}

const char *
irlabname(long l)
{
    char *s = malloc(24);

    sprintf(s, ".L%ld", (long)l);
    return s;
}

/* the body of a pooled constant: the text, or the number */
long
irpoolemit(long v)
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
long
irfnkind(long v)
{
    const char *nm = irsymname(v);

    return strcmp(nm, "prn_2e") == 0 ? 1 : strcmp(nm, "red_2e") == 0 ? 2 : 0;
}

long irvarrec(const char *sym, long size) { ir_varrec(sym, size); return 0; }

/* a routine of the runtime library, which returns a C long, not a long */
long
irruntime(long v)
{
    const char *nm = irsymname(v);

    return strcmp(nm, "ifix") == 0 || strcmp(nm, "fini") == 0 || strncmp(nm, "imp_", 4) == 0;
}

/* the error for a GO TO a tag that is not in the subroutine, with the tag's name */
long
irtagerr(long d)
{
    char msg[160];

    snprintf(msg, sizeof msg, "GO TO %s, WHICH IS NOT IN THE SUBROUTINE.", irsymname(d));
    imp_error(1, msg);
    return 0;
}

/* the name of the jump buffer of tag d: a GO TO from another routine is a longjmp to it (see EXPORTTAGS in src/pccir/demit.imp) */
long
irjbname(long d)
{
    char buf[200], *s;

    snprintf(buf, sizeof buf, "imp_jb_%s", irsymname(d));
    s = malloc(strlen(buf) + 1);
    strcpy(s, buf);
    return (long)s;
}

long irreset(long z) { (void)z; ir_usedfs = 0; return 0; }
long irfsused(long z) { (void)z; return ir_usedfs; }
long irflush(long z) { (void)z; fflush(NULL); return 0; }


/* ---- the IR builders of ir.c, by value, under names IMP can write (no underscore) ---------------------------------------------- */

#define N(x) ((struct irn *)(intptr_t)(x))
#define CS(x) ((const char *)(intptr_t)(x))
#define W(x) ((long)(intptr_t)(x))
long ixnewtemp(long z) { (void)z; return W(ir_newtemp()); }
long ixnewlabel(long z) { (void)z; return W(ir_newlabel()); }
long ixtemp(long n, long t) { return W(ir_temp((int)n, (int)t)); }
long ixicon(long lo, long hi, long t) { return W(ir_icon(join(lo, hi), (int)t)); }
long ixquad(long lo, long hi) { ir_pass(" .quad %lld", (long long)join(lo, hi)); return 0; }
long ixsym(long nm, long off, long t) { return W(ir_sym(CS(nm), off, (int)t)); }
long ixname(long nm, long off, long t) { return W(ir_name(CS(nm), off, (int)t)); }
long ixun(long op, long t, long l) { return W(ir_un((int)op, (int)t, N(l))); }
long ixbin(long op, long t, long l, long r) { return W(ir_bin((int)op, (int)t, N(l), N(r))); }
long ixassign(long l, long r) { return W(ir_assign(N(l), N(r))); }
long ixderef(long a, long t) { return W(ir_deref(N(a), (int)t)); }
long ixstmt(long n) { ir_stmt(N(n)); return 0; }
long ixlabel(long n) { ir_label((int)n); return 0; }
long ixgoto(long n) { ir_goto((int)n); return 0; }
long ixcbranch(long op, long l, long r, long lab) { ir_cbranch((int)op, N(l), N(r), (int)lab); return 0; }
long ixfuncbegin(long nm, long vis) { ir_func_begin(CS(nm), (int)vis); return 0; }
long ixfuncend(long retlab, long rettemp) { ir_func_end((int)retlab, (int)rettemp); return 0; }
long ixpass(long fmt, long a, long b, long c) { ir_pass(CS(fmt), a, b, c); return 0; }
long ixshift(long right, long l, long r) { return W(ir_xshift((int)right, N(l), N(r))); }
/* the arguments are an array of IMP words (64 bits each, whatever the machine), holding pointers */
long
ixcall(long fn, long a, long n, long cret)
{
    const int64_t *w = (const int64_t *)(intptr_t)a;
    struct irn *v[16];
    long k;

    for (k = 0; k < n && k < 16; k++)
        v[k] = (struct irn *)(intptr_t)w[k];
    return W(ir_xcall(N(fn), v, (int)n, (int)cret));
}
long ixparam(long k) { return W(ir_param((int)k)); }
long ixtodouble(long w, long k) { return W(ir_todouble(N(w), (int)k)); }
long ixfromdouble(long d) { return W(ir_fromdouble(N(d))); }

/* by reference, for the IMP modules (the glue calls of src/pccir) */
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

/* the list of the code of an entry (/A): PCODE of src/pccir/code3.imp, all the lines of the chain */
void
pcode(word a)
{
    word n = 0x7fffffff;

    impl_pcode(&a, &n);
}

/* the cache of assembler names is made again after an image is loaded (the directory is another) */
void
ir_symreset(void)
{
    long i;

    for (i = 0; i < nsymcache; i++)
        free(symcache[i]);
    free(symcache);
    symcache = NULL;
    nsymcache = 0;
}
