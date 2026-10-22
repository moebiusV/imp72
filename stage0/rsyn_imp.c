/*
 * rsyn_imp.c -- rsyn.c's interface, over src/rsyn.imp compiled by imp72stage0: all of decus/rsyn.imc.  The semantic
 * routines are entered in the table of DSEM by C thunks (the table holds C functions that take the array of
 * parameters; the IMP routines take their parameters by reference, so a thunk passes the addresses of the
 * elements).  Ports
 * decus/rsyn.imc: RSYN and the routines that enter syntax statements into
 * the syntax graph and the semantics array (SYNT, SYTRM, SYTERM, ADDCHAR,
 * SYNTS, SEMANTICS, FORMARG, SYNARG, DEFCASE, CASE, QUOSEM, EQUOSE, SEMP,
 * GETARG, SEMR, SYNTAX, SEMBIT, REPVAL, VALU, RSERR).
 *
 * RSYN reads, with LEX alone, the part of the file SYNTAX up to the first
 * %%: the syntax statements
 *
 *      <CLASS> ::= elements ::= semantics ;
 *
 * of a tiny notation (a class, its elements which are <CLASS,ARG> or
 * symbols or NAM" and SYM", and the semantics, a call of one routine or an
 * N-ary routine with names for arguments, optionally with the 'ELSE'
 * extensions that syntax.imc itself then defines).  It makes the graph,
 * and for each production its semantics in SEMS and its place in the code
 * tree, exactly as the full compiler does once the syntax for syntax is in.
 *
 * The routines here are semantic routines too: the bootstrapped syntax
 * invokes them (as SYNT, SEMR, ...) from the productions that read the rest
 * of syntax.imc, so they are registered by name.
 *
 * Notes on the original, which this follows:
 *  - The production under construction is in free array SN, the argument
 *    names (0 for a symbol that has none, -1 for NAM#/SYM#) in NA.  A
 *    nonterminal in SN has a '#' prefixed to its name (ADDCHAR).
 *  - SEMK is the index in SEMS of the semantics being built; SEMN is the
 *    production number, which is SEMS's index of its first semantics;
 *    PRODN is the number in the tree, SEMN except for the cases of a class.
 *  - RST is a stack of calls whose arguments are still being read.
 */
#include <stdint.h>
#include "imp.h"

extern word fb;                         /* the base of free storage, for IMP */
extern word impl_rsyn(word *), impl_synarg(word *), impl_defcase(word *), impl_rcase(word *, word *),
    impl_quosem(word *), impl_equose(word *), impl_sembit(word *), impl_sytrm(word *), impl_synts(word *),
    impl_semantics(word *), impl_syntax(word *), impl_semr(word *, word *), impl_semp(word *, word *),
    impl_synt(word *, word *), impl_valu(word *, word *), impl_repval(word *, word *);

static void
sync(void)
{
    fb = (word)(intptr_t)fmem;
}

#define P(i) ((word *)(uintptr_t)&p[i])
static word t_defcase(const word *p) { return impl_defcase(P(0)); }
static word t_quosem(const word *p) { return impl_quosem(P(0)); }
static word t_equose(const word *p) { return impl_equose(P(0)); }
static word t_sembit(const word *p) { return impl_sembit(P(0)); }
static word t_sytrm(const word *p) { return impl_sytrm(P(0)); }
static word t_synts(const word *p) { return impl_synts(P(0)); }
static word t_semantics(const word *p) { return impl_semantics(P(0)); }
static word t_syntax(const word *p) { return impl_syntax(P(0)); }
static word t_semr(const word *p) { return impl_semr(P(0), P(1)); }
static word t_semp(const word *p) { return impl_semp(P(0), P(1)); }
static word t_synt(const word *p) { return impl_synt(P(0), P(1)); }
static word t_enstack(const word *p) { return enstack(p[0]); }
static word t_valu(const word *p) { return impl_valu(P(0), P(1)); }
static word t_repval(const word *p) { return impl_repval(P(0), P(1)); }
static word t_case(const word *p) { return impl_rcase(P(0), P(1)); }

/* called by RSYN (through impglue.c) */
void
rsyn_dsems(void)
{
    dsem("CASE", t_case);
    dsem("DEFCASE", t_defcase);
    dsem("QUOSEM", t_quosem);
    dsem("EQUOSE", t_equose);
    dsem("SEMBIT", t_sembit);
    dsem("SYTRM", t_sytrm);
    dsem("SYNTS", t_synts);
    dsem("SEMANTICS", t_semantics);
    dsem("SYNTAX", t_syntax);
    dsem("SEMR", t_semr);
    dsem("SEMP", t_semp);
    dsem("SYNT", t_synt);
    dsem("ENSTACK", t_enstack);
    dsem("VALU", t_valu);
    dsem("REPVAL", t_repval);
}

word
synarg(word i)
{
    sync();
    return impl_synarg(&i);
}

void
rsyn(void)
{
    word tr = 0;

    sync();
    impl_rsyn(&tr);
    sync();
}

/* the by-value entry points of the semantic routines (impsem.c and others call some) */
word synt(word a, word b) { word r; sync(); r = impl_synt(&a, &b); sync(); return r; }
word sytrm(word a) { word r; sync(); r = impl_sytrm(&a); sync(); return r; }
word synts(word n) { word r; sync(); r = impl_synts(&n); sync(); return r; }
word semantics(void) { word r, nil = 0; sync(); r = impl_semantics(&nil); sync(); return r; }
word syntax(word n) { word r; sync(); r = impl_syntax(&n); sync(); return r; }
word semr(word a, word b) { word r; sync(); r = impl_semr(&a, &b); sync(); return r; }
word semp(word a, word b) { word r; sync(); r = impl_semp(&a, &b); sync(); return r; }
word quosem(word a) { word r; sync(); r = impl_quosem(&a); sync(); return r; }
word equose(word a) { word r; sync(); r = impl_equose(&a); sync(); return r; }
word defcase(word n) { word r; sync(); r = impl_defcase(&n); sync(); return r; }
word docase_(word a, word f) { word r; sync(); r = impl_rcase(&a, &f); sync(); return r; }
word sembit(word a) { word r; sync(); r = impl_sembit(&a); sync(); return r; }
word repval(word s, word a) { word r; sync(); r = impl_repval(&s, &a); sync(); return r; }
word valu(word n, word s) { word r; sync(); r = impl_valu(&n, &s); sync(); return r; }
