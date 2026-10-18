/* Graph, parser, error corrector, disambiguator, on a small grammar:
 *   1: #S ::= A #S B     2: #S ::= C     3: #PG ::= G #S %     4: #S ::= NAM#
 * (production 3's % is the one RSYN's #PG ends with; a second % is the
 * lookahead that lets the parser put out production 3.) */
#include <string.h>
#include "imp.h"
#include "check.h"

static word out[64];
static int nout;

static void
collect(word k)
{
    if (nout < 64)
        out[nout++] = k;
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

static int
run(const char *text)
{
    FILE *f = fopen("test_parse.tmp", "w");

    fputs(text, f);
    fclose(f);
    nout = 0;
    lexin("test_parse.tmp");
    parse_cotree = collect;
    parse("#PG");
    remove("test_parse.tmp");
    return nout;
}

#define EXPECT(...) do { word want[] = { __VA_ARGS__ }; \
    int n = (int)(sizeof want / sizeof want[0]), i; \
    if (nout != n) { fprintf(stderr, "got %d outputs, want %d:", nout, n); \
        for (i = 0; i < nout; i++) fprintf(stderr, " %ld", (long)out[i]); \
        fputc('\n', stderr); CHECK(0); } \
    else for (i = 0; i < n; i++) CHECK(out[i] == want[i]); } while (0)

int
main(void)
{
    static const char *const p1[] = { "#S", "A", "#S", "B" };
    static const char *const p2[] = { "#S", "C" };
    static const char *const p3[] = { "#PG", "G", "#S", "%" };
    static const char *const p4[] = { "#S", "NAM#" };

    prod(1, 3, p1);
    prod(2, 1, p2);
    prod(3, 3, p3);

    run("G A A C B B % %\n");
    EXPECT(2, 1, 1, 3);
    run("G C % %\n");
    EXPECT(2, 3);

    /* with an identifier production, C is ambiguous: literal C wins, since
     * the parse that matched NAM# put out a name */
    prod(4, 1, p4);
    run("G C % %\n");
    EXPECT(2, 3);
    run("G XYZ % %\n");
    EXPECT(-(word)dir("XYZ"), 4, 3);
    run("G A XYZ B % %\n");
    EXPECT(-(word)dir("XYZ"), 4, 1, 3);

    /* a missing B is supplied by the error corrector.  (G keeps the circled
     * node of #S from having a tail that ercor would take for a way on:
     * see docs/internals/stage0-parser.md) */
    finam5 &= ~SW_LIST;
    run("G A C % %\n");
    /* production 2 is put out twice: the round in which % died had already
     * put it on the surviving parse's chain, and the retry does so again.
     * parse.imc does the same as far as I can tell (to check against the
     * oracle). */
    EXPECT(2, 2, 1, 3);
    CHECK(finam5 & SW_ERRORS);
    DONE();
}
