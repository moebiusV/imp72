/* Free storage: allocation, relocation, packing, FADD. */
#include "imp.h"
#include "check.h"

static word a, b, c, d;

static void
fill(word p, word n, word base)
{
    word i;
    for (i = 0; i < n; i++)
        FREES(p + i, base + i);
}

static int
ok(word p, word n, word base)
{
    word i;
    for (i = 0; i < n; i++)
        if (FREE(p + i) != base + i)
            return 0;
    return 1;
}

int
main(void)
{
    word fixed[40], i, n;
    word tmp, big, cnt;

    frelot(&a, "A", 10, NULL);
    frelot(&b, "B", 10, NULL);
    frelot(&c, "C", 10, NULL);
    CHECK(flen(a) == 10 && flen(b) == 10);
    fill(a, 10, 100); fill(b, 10, 200); fill(c, 10, 300);
    CHECK(ok(a, 10, 100) && ok(b, 10, 200) && ok(c, 10, 300));

    /* grow A past B: A must move, its variable follow, contents survive */
    tmp = a;
    fallot(a, 5000);
    CHECK(a != tmp);
    CHECK(flen(a) == 5000 && ok(a, 10, 100));
    CHECK(FREE(a + 4999) == 0);
    CHECK(ok(b, 10, 200) && ok(c, 10, 300));

    /* shrink and delete; the freed space is reused */
    fallot(a, 20);
    CHECK(flen(a) == 20 && ok(a, 10, 100));
    fallot(b, 0);
    d = 0;
    frelot(&d, "D", 8, NULL);
    fill(d, 8, 400);
    CHECK(ok(d, 8, 400) && ok(c, 10, 300) && ok(a, 10, 100));

    /* fixed arrays interleaved with relocatable ones, then force a pack */
    for (i = 0; i < 40; i += 2) {
        fixed[i] = fallot_new("FIX", 30);
        fill(fixed[i], 30, 1000 * i);
        word r = 0;
        frelot(&r, "REL", 30, NULL);   /* r is local: dead after the loop */
        fallot(r, 0);                  /* free it again, leaving a gap */
    }
    for (i = 0; i < 40; i += 2)
        CHECK(ok(fixed[i], 30, 1000 * i));
    big = 0;
    frelot(&big, "BIG", 60000, NULL);
    CHECK(flen(big) == 60000);
    CHECK(ok(a, 10, 100) && ok(c, 10, 300) && ok(d, 8, 400));
    for (i = 0; i < 40; i += 2)
        CHECK(ok(fixed[i], 30, 1000 * i));

    /* FADD grows an array one word at a time */
    tmp = 0;
    frelot(&tmp, "ADD", 3, NULL);
    cnt = 0;
    for (n = 0; n < 20000; n++)
        fadd(&tmp, &cnt, n * 3);
    CHECK(cnt == 20000 && flen(tmp) >= 20000);
    for (n = 0; n < 20000; n++)
        if (FREE(tmp + n) != n * 3) { CHECK(0); break; }
    CHECK(ok(a, 10, 100) && ok(c, 10, 300) && ok(d, 8, 400));
    DONE();
}
