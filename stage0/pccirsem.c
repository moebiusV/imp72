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

/* the list of the code of an entry (/A): PCODE of src/pccir/code3.imp, all the lines of the chain */
void
pcode(word a)
{
    word n = 0x7fffffff;

    impl_pcode(&a, &n);
}

