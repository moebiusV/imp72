/* Semantic stack, pushdown stack, bit matrices. */
#include "imp.h"
#include "check.h"

int
main(void)
{
    word i, j, s, a[3] = { 0, 0, 0 }, b[3] = { 0, 0, 0 };

    /* semantic stack: three word entries */
    stinit();
    CHECK(stp == -3);
    s = stpush(1, 2, 3);
    CHECK(FREE(s) == 1 && FREE(s + 1) == 2 && FREE(s + 2) == 3);
    for (i = 0; i < 100; i++)       /* force FADDEX */
        stpush(10 * i, 10 * i + 1, 10 * i + 2);
    s = stack(0);
    CHECK(FREE(s) == 990 && FREE(s + 2) == 992);
    s = stack(-1);
    CHECK(FREE(s) == 980);
    stpop(50);
    CHECK(FREE(stack(0)) == 490);
    s = setstk(1, 7, 8, 9);
    CHECK(FREE(s) == 7 && FREE(s + 2) == 9);

    /* pushdown stack */
    for (i = 0; i < 100; i++)
        pushmi(i * i);
    CHECK(pullyu(1) == 99 * 99);
    CHECK(pullyu(0) == 99 * 99);
    CHECK(pullyu(0) == 98 * 98);

    /* bit matrices: set a scattering of bits, growing in both directions */
    for (i = 0; i < 300; i += 7)
        for (j = 0; j < 500; j += 11)
            bits(a, i, j);
    CHECK(a[1] >= 295 && a[2] == 7);
    for (i = 0; i < a[1]; i++)      /* bit() has no bounds check */
        for (j = 0; j < 64 * (a[2] + 1); j++) {
            int want = (i % 7 == 0 && i < 300) && (j % 11 == 0 && j < 500);
            if ((bit(a, i, j) != 0) != want) { CHECK(0); goto out; }
        }
out:
    /* BITOR: row 1 |= row 7 */
    bits(b, 3, 70);
    bits(b, 5, 3);
    bitor_(b, 3, 5);
    CHECK(bit(b, 3, 3) != 0 && bit(b, 3, 70) != 0 && bit(b, 5, 70) == 0);
    bitz(b);
    CHECK(bit(b, 3, 3) == 0 && bit(b, 3, 70) == 0);
    bstats(0);
    DONE();
}
