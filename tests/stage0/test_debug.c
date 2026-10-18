/* PRINT formats, and the graph dump on a small grammar. */
#include <string.h>
#include "imp.h"
#include "check.h"

static char buf[16384];

/* What has been printed so far; then a fresh listing file. */
static const char *
captured(void)
{
    size_t n;

    fflush(listing);
    rewind(listing);
    n = fread(buf, 1, sizeof buf - 1, listing);
    buf[n] = '\0';
    fclose(listing);
    listing = tmpfile();
    return buf;
}

static void
prod(int pn, int n, const char *const *syms)
{
    word a = fallot_new("SYNT", n + 1);
    int i;

    for (i = 0; i <= n; i++)
        FREES(a + i, dir(syms[i]));
    graph(a, n, pn);
    gmatr(0);
    fallot(a, 0);
}

int
main(void)
{
    static const char *const p1[] = { "#S", "A", "#S", "B" };
    static const char *const p2[] = { "#S", "C" };
    static const char *const p3[] = { "#S", "#S", "+", "C" };
    const char *s;

    listing = tmpfile();

    /* the formats of the manual's examples */
    pr_stg(1, "A"); pr_igr(4, 65); pr_oct(6, 65); pr_oct(6, 65); pr_nl();
    CHECK(strcmp(captured(), "A  65000101000101\n") == 0);
    pr_igr(3, 0); pr_igr(3, 1); pr_igr(3, 2); pr_nl();
    CHECK(strcmp(captured(), "  0  1  2\n") == 0);
    pr_igr(2, 12345); pr_igr(0, -7); pr_oct(0, 0); pr_oct(14, -1); pr_nl();
    CHECK(strcmp(captured(), "12345-70  777777777777\n") == 0);
    pr_stg(8, "SCR"); pr_stg(2, "["); pr_stg(3, "ABCDEF"); pr_nl();
    CHECK(strcmp(captured(), "     SCR [ABC\n") == 0);

    prod(1, 3, p1);
    prod(2, 1, p2);
    prod(3, 3, p3);
    gprint();
    s = captured();
    CHECK(strstr(s, "HERE IS THE GRAPH FOLLOWING ENTRY OF PRODUCTION NR. 3\n"
        "   INDEX     SCR     ALT    TYPE      NB    NAME     NT#   ENTRY\n"));
    /* the circled node of #S: successor 23 (the +), type 2, NB row 1, NT# 1,
     * entry 1 */
    CHECK(strstr(s, "\n       1      23       0       2       1    #S    1    1\n"));
    /* + is required right after the circled #S (the production #S ::= #S + C)
     * and nowhere else, so it is the only terminal with a chain */
    CHECK(strstr(s, "TERMINAL +  23\n") != NULL);
    CHECK(strstr(s, "TERMINAL A") == NULL);
    CHECK(strstr(s, "TERMINAL #S") == NULL);
    CHECK(strstr(s, "NODE#") == NULL);          /* no matrices without /N */
    finam5 |= SW_MATRICES;
    gprint();
    s = captured();
    CHECK(strstr(s, "CONNECTIVITY MATRICES BITM AND NBITM.\n") != NULL);
    CHECK(strstr(s, "NODE#   1  1") != NULL);
    DONE();
}
