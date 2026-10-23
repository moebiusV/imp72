/*
 * lex_imp.c -- the lexical analyser is src/lex.imp (LEXIN, LEX, NEXTCHAR, PLINE1, PLINE); nothing of it is C any more.
 * lex() and pline1() are for the C that is left.
 */
#include "imp.h"

extern word impl_lex(word *), impl_pline1(word *);

word
lex(word *s)
{
    return impl_lex(s);
}

void
pline1(word ol)
{
    impl_pline1(&ol);
}
