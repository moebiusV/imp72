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

extern word impl_rsyn(word *), impl_synarg(word *), impl_defcase(word *), impl_rcase(word *, word *),
    impl_quosem(word *), impl_equose(word *), impl_sembit(word *), impl_sytrm(word *), impl_synts(word *),
    impl_semantics(word *), impl_syntax(word *), impl_semr(word *, word *), impl_semp(word *, word *),
    impl_synt(word *, word *), impl_valu(word *, word *), impl_repval(word *, word *);


word
synarg(word i)
{
    return impl_synarg(&i);
}

void
rsyn(void)
{
    word tr = 0;

    impl_rsyn(&tr);
}

/* the by-value entry points of the semantic routines (impsem.c and others call some) */
word synt(word a, word b) { word r; r = impl_synt(&a, &b); return r; }
word sytrm(word a) { word r; r = impl_sytrm(&a); return r; }
word synts(word n) { word r; r = impl_synts(&n); return r; }
word semantics(void) { word r, nil = 0; r = impl_semantics(&nil); return r; }
word syntax(word n) { word r; r = impl_syntax(&n); return r; }
word semr(word a, word b) { word r; r = impl_semr(&a, &b); return r; }
word semp(word a, word b) { word r; r = impl_semp(&a, &b); return r; }
word quosem(word a) { word r; r = impl_quosem(&a); return r; }
word equose(word a) { word r; r = impl_equose(&a); return r; }
word defcase(word n) { word r; r = impl_defcase(&n); return r; }
word sembit(word a) { word r; r = impl_sembit(&a); return r; }
word repval(word s, word a) { word r; r = impl_repval(&s, &a); return r; }
word valu(word n, word s) { word r; r = impl_valu(&n, &s); return r; }
