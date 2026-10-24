/*
 * dosem_imp.c -- dosem.c's interface, over src/dosem.imp compiled by imp72stage0: all of decus/dosem.imc but PAR, the
 * entry for STACK, and SEMPER, which stay here, with the by-value entry points.  Ports decus/dosem.imc:
 * DOSEM, SEMFIX, SEMFX1, DOCASE, ENPROD, MATCHP, NOTEPROD, PLIES, SNODEN,
 * ENSTACK, RESETSEM, SETSEM, PAR, SEMPER, SNARG, SEMGET, SEMED, SEMOR, CKSVAL.
 *
 * The semantics of a production is a run of words in free array SEMS (36 bit
 * words; V = bits 17-0, N = bits 29-18, TYPE = bits 35-30):
 *
 *   0  start of semantics with N arguments; bits 29-27 are a priority for
 *      immediate execution; V is the production's entry in PROSEM
 *   1  end of semantics (so is a second type 0)
 *   2  push the stack pointer of parameter V on PSTAK
 *   3  call semantic routine V with the bottom N entries of PSTAK (at most
 *      10) and push the result
 *   4  push the value -V as the next parameter
 *   5  enter a node with value V at the next position of the code stack
 *   6  enter parameter V of the stack in the next position
 *   7  copy parameter V of the stack to the next position
 *   10 load a constant on the stack of parameters for 'CASE'
 *   11 add the last two stack elements
 *   12 put a type 0 node for [V], value N, on the stack
 *
 * PROSEM records, for each production, where it can match in the code tree;
 * see ENPROD below.  Its words, like those of SEMS, keep the original
 * layout.
 *
 * Semantic routines are C functions called through the table of sem.c; the
 * original jumps to the routine's address.
 */
#include <stdint.h>
#include <string.h>
#include "imp.h"

extern word casep;
extern word impl_dosem(word *), impl_setsem(word *, word *), impl_resetsem(word *, word *),
    impl_semfix(word *, word *), impl_semfx1(word *, word *, word *),
    impl_docase(word *, word *, word *), impl_enprod(word *, word *, word *),
    impl_matchp(word *, word *), impl_noteprod(word *, word *), impl_plies(word *),
    impl_snoden(word *), impl_enstack(word *), impl_snarg(word *), impl_semget(word *),
    impl_semed(word *), impl_semor(word *, word *), impl_cksval(word *, word *), impl_semper(word *);


word dosem(word ppo) { word r; r = impl_dosem(&ppo); return r; }
word setsem(word ty, word val) { word r; r = impl_setsem(&ty, &val); return r; }
void resetsem(word val, word a) { impl_resetsem(&val, &a); }
word semfix(word i, word a) { word r; r = impl_semfix(&i, &a); return r; }
word semfx1(word i, word a, word na) { word r; r = impl_semfx1(&i, &a, &na); return r; }
void docase(word id, word semn, word prodn) { impl_docase(&id, &semn, &prodn); }
void enprod(word pn, word no, word sem) { impl_enprod(&pn, &no, &sem); }
word matchp(word pn, word n) { return impl_matchp(&pn, &n); }
void noteprod(word pn, word n) { impl_noteprod(&pn, &n); }
word plies(word i) { return impl_plies(&i); }
word snoden(word i) { return impl_snoden(&i); }
word enstack(word v) { word r; r = impl_enstack(&v); return r; }
word snarg(word n) { return impl_snarg(&n); }
word semget(word n) { return impl_semget(&n); }
word semed(word a) { return impl_semed(&a); }
void semor(word a, word n) { impl_semor(&a, &n); }
word cksval(word v, word i) { return impl_cksval(&v, &i); }

void
semper(void)
{
    word z = 0;

    impl_semper(&z);
}
