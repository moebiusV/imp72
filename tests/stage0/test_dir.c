/* The directory: hashing (checked against indices worked out by hand from
 * the algorithm in dir.imc), constants, and names. */
#include <string.h>
#include "imp.h"
#include "check.h"

int
main(void)
{
    word a, cdq, chr, cls, cpt, n, x, v;
    int count = 0;
    char buf[32];
    int i;

    /* hash("A") = 260, so its node is 3*260 */
    a = dir("A");
    CHECK(a == 780);
    CHECK(dir("A") == a);
    CHECK(strcmp(gname(a), "A") == 0);
    CHECK(dir("B") == 792);
    CHECK(dir("IMP") == 1524);
    /* CDQ CHR CLS CPT all hash to 260 too: two alternate buckets (260^2 and
     * 260^2^1), then a chain of overflow nodes from 1536 */
    cdq = dir("CDQ"); chr = dir("CHR"); cls = dir("CLS"); cpt = dir("CPT");
    CHECK(cdq == 786);
    CHECK(chr == 789);
    CHECK(cls == 1536);
    CHECK(cpt == 1539);
    CHECK(dir("CLS") == cls && dir("CPT") == cpt && dir("CDQ") == cdq);
    CHECK(strcmp(gname(cpt), "CPT") == 0);

    /* a name of exactly five characters needs its terminator word */
    x = dir("ABCDE");
    CHECK(dname(x, 0) != 0 && dname(x, 1) == 0);
    x = dir("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    CHECK(strcmp(gname(x), "ABCDEFGHIJKLMNOPQRSTUVWX") == 0);   /* 24 chars */

    /* many names: all distinct, all found again, all enumerated once */
    for (i = 0; i < 3000; i++) {
        sprintf(buf, "NAME%d", i);
        dir(buf);
    }
    for (i = 0; i < 3000; i++) {
        sprintf(buf, "NAME%d", i);
        x = dir(buf);
        if (strcmp(gname(x), buf) != 0) { CHECK(0); break; }
    }
    for (n = dseq(-1); n >= 0; n = dseq(n))
        count++;
    CHECK(count == 3000 + 9);

    /* properties live in the node */
    dprops(DP_SEM, a, 4242);
    dprops(DP_ENTRY, a, 7);
    dprops(DP_NODE, a, 9);
    dprops(DP_USE, a, 5);
    dprops(DP_ATYPE, a, 3);
    dprops(DP_COM, a, 2);
    dprops(DP_ISNM, a, 1);
    dprops(DP_LXTY, a, 2);
    CHECK(dprop(DP_SEM, a) == 4242 && dprop(DP_ENTRY, a) == 7
        && dprop(DP_NODE, a) == 9 && dprop(DP_USE, a) == 5
        && dprop(DP_ATYPE, a) == 3 && dprop(DP_COM, a) == 2
        && dprop(DP_ISNM, a) == 1 && dprop(DP_LXTY, a) == 2);
    dprops(DP_USE, a, 1);
    CHECK(dprop(DP_USE, a) == 1 && dprop(DP_ATYPE, a) == 3
        && dprop(DP_SEM, a) == 4242);

    /* constants */
    CHECK(convc(dir("123")) == -1 && convc_const == 123);
    CHECK(convc(dir("17B")) == -1 && convc_const == 15);
    CHECK(convc(dir("101B2")) == -1 && convc_const == 5);
    CHECK(convc(dir("0FFB16")) == -1 && convc_const == 255);
    CHECK(convc(dir("12AB")) == 0);
    CHECK(convc(dir("ABC")) == 0);
    CHECK(convc(dir("9B8")) == 0);              /* 9 is not an octal digit */
    CHECK(convc(dir("777777777777B")) == -1 && convc_const == -1);
    v = convc(dir("'ABC"));                     /* string: pointer to words */
    CHECK(v > 0 && FREE(v) == 1);
    CHECK(convc_const == dname(dir("ABC"), 0));
    v = convc(dir("'ABCDEFGH"));
    CHECK(FREE(v) == 2 && FREE(v + 1) == dname(dir("ABCDE"), 0)
        && FREE(v + 2) == dname(dir("FGH"), 0));
    CHECK(strcmp(gname(stcon(15)), "17B") == 0);
    CHECK(strcmp(gname(stcon(0)), "0") == 0);
    CHECK(strcmp(gname(stcon(-1)), "777777777777B") == 0);
    CHECK(stcon(15) == dir("17B"));
    CHECK(convc(stcon(-77)) == -1 && convc_const == -77);
    CHECK(strcmp(gname(newname("ABCDEFG")), "%ABCD1") == 0);
    CHECK(strcmp(gname(newname("X")), "%X2") == 0);
    CHECK(radi50(dir("ABC")) == (11 * 40 + 12) * 40 + 13);
    CHECK(radi50(dir("A.B$C%D")) == ((((11L * 40 + 37) * 40 + 12) * 40 + 38) * 40 + 13) * 40 + 39);
    DONE();
}
