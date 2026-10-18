/*
 * pr.c -- IMP PRINT formats.  The part of PMOD (the formatted print module of
 * the PDP-10 compiler, which is not in the tape's sources) that the
 * diagnostic print-outs of debug.c use, written from the manual, section
 * 2.2.7:
 *
 *   IGR n   base 10 in a field n wide, leading zeros blank, minus sign if
 *           negative; a number too big for the field widens it; n = 0 is as
 *           wide as the number.
 *   OCT n   base 8, unsigned, zero filled, in a field n wide; an n over 12
 *           gets n-12 blanks on the left.  (12 digits: it is a 36 bit word.)
 *   STG n   characters up to a 0 byte or n characters; n = 0 is the whole
 *           string.
 *   /       end the line.
 *
 * The manual does not say whether STG pads a short string.  The headings of
 * GPRIN1 (`STG 8,'INDEX','SCR',...` above rows printed `IGR 8`) only line up
 * if it pads on the left, so it does; if the oracle says otherwise this is
 * the one place to change.
 */
#include <string.h>
#include "imp.h"

void
pr_stg(int w, const char *s)
{
    FILE *o = imp_out();
    int n = (int)strlen(s);

    if (w > 0 && n > w)
        n = w;
    if (w > n)
        fprintf(o, "%*s", w - n, "");
    fwrite(s, 1, (size_t)n, o);
}

void
pr_igr(int w, word v)
{
    fprintf(imp_out(), "%*ld", w, (long)v);
}

void
pr_oct(int w, word v)
{
    FILE *o = imp_out();
    unsigned long long u = (unsigned long long)v & 0xFFFFFFFFFULL;

    if (w == 0) {
        fprintf(o, "%llo", u);
        return;
    }
    if (w > 12) {
        fprintf(o, "%*s", w - 12, "");
        w = 12;
    }
    fprintf(o, "%0*llo", w, u);
}

void
pr_nl(void)
{
    fputc('\n', imp_out());
}
