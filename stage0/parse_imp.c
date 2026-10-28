/*
 * parse_imp.c -- parse.c's interface, over src/parse.imp compiled by imp72stage0.
 * The IMP module has PARSE, SYMBOL, PFORCE, COPY, POUT and PARSEF of decus/parse.imc; here are the
 * by-value C entry points, COTREE's hook, and PSTATS, which prints the statistics that the module
 * keeps in common variables.  ERCOR, AMBIG and ISNAME are in ercor.c.
 */
#include <stdint.h>
#include <string.h>
#include "imp.h"

extern word impl_pstats(word *), impl_parse(word *), impl_pforce(word *), impl_pout(word *, word *), impl_parsef(void);
extern word psts, pspn, pspnt, psps, pspst, psos, amtime, pstt, srtt;

void (*parse_cotree)(word k);


word
parse(const char *goal)
{
    word g[4] = { 0, 0, 0, 0 }, r;

    strncpy((char *)g, goal, 3 * sizeof g[0]);
    r = impl_parse(g);
    return r;
}

void
pforce(word sy)
{
    impl_pforce(&sy);
}

word
pout(word qq, word vv)
{
    return impl_pout(&qq, &vv);
}

void
parsef(void)
{
    impl_parsef();
}

void
pstats(void)
{
    word z = 0;

    impl_pstats(&z);
}
