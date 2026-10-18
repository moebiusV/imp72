/* Declarations: DECL / DECLARE on names, and NAME on raw identifiers. */
#include "imp.h"
#include "check.h"

static word
raw(const char *s)
{
    word e = setstk(1, sx36(1 | (dir(s) << 18)), 0, 0);

    return e;
}

int
main(void)
{
    word x = dir("XX"), y = dir("YY"), e;

    sem_init();
    stinit();
    newlist(raw("XX"));
    e = raw("YY");
    enlist(e);
    decl(2, 0);                         /* COMMON */
    decl(3, 0);                         /* REAL */
    declare(setstk(1, 0, 0, 0));
    CHECK(dprop(DP_COM, x) == 1 && dprop(DP_ATYPE, x) == 2);
    CHECK(dprop(DP_COM, y) == 1 && dprop(DP_ATYPE, y) == 2);

    /* the properties are cleared for the next declaration */
    newlist(raw("ZZ"));
    decl(4, 0);                         /* INTEGER */
    declare(setstk(1, 0, 0, 0));
    CHECK(dprop(DP_COM, dir("ZZ")) == 0 && dprop(DP_ATYPE, dir("ZZ")) == 1);

    /* NAME: a constant, a register, a plain identifier */
    e = name(raw("17B"));
    CHECK((FREE(e) & 077) == 4 && FREE(e + 1) == 15 && L18(FREE(e)) == dir("17B"));
    e = name(raw("5R"));
    CHECK((FREE(e) & 077) == 2 && L18(FREE(e + 1)) == 5 + 040 + 1);
    e = name(raw("12R"));
    CHECK((FREE(e) & 077) == 2 && L18(FREE(e + 1)) == 12 + 040 + 1);
    e = name(raw("FOO"));
    CHECK((FREE(e) & 077) == 010 && L18(FREE(e)) == dir("FOO") && FREE(e + 1) == 0);
    CHECK(tstrn(dir("7R")) == 7 && tstrn(dir("15R")) == 15 && tstrn(dir("16R")) == -1
        && tstrn(dir("R7")) == -1 && tstrn(dir("RR")) == -1);

    /* LOCAL: the declared name is replaced by a unique one */
    newlist(raw("LV"));
    decl(11, 0);
    declare(setstk(1, 0, 0, 0));
    e = name(raw("LV"));
    CHECK(L18(FREE(e)) != dir("LV") && L18(FREE(e)) != 0);
    DONE();
}
