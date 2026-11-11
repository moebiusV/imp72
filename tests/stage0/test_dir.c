/* The directory: hashing (checked against indices worked out by hand from
 * the algorithm in src/dir.imc: eight characters to a word, the first in the
 * low byte, HASH = NM xor NM RS 32, RS 16, RS 9, and 777B), constants, and names. */
#include <string.h>
#include "imp.h"
#include "check.h"

/* the words of a text constant are in memory order (convc): the word of the name for the first character in the low byte on a little endian host, swapped on a big endian one */
static word
memw(word w)
{
    unsigned x = 1;
    uword u = (uword)w, r = 0;
    int i;

    if (*(unsigned char *)&x != 0)
        return w;
    for (i = 0; i < 8; i++, u >>= 8)
        r = (r << 8) | (u & 0377);
    return (word)r;
}

int
main(void)
{
    word a, cdq, chr, cls, cpt, n, x, v;
    int count = 0;
    char buf[32];
    int i;

    /* hash("A") = 65 (0x41), so its node is 3*65 */
    a = dir("A");
    CHECK(a == 195);
    CHECK(dir("A") == a);
    CHECK(strcmp(gname(a), "A") == 0);
    CHECK(dir("B") == 198);
    CHECK(dir("IMP") == 957);
    /* AAAAA AAADC AAAIE AAALG all hash to 65 too: two alternate buckets (65^2
     * and 65^2^1, which is B's), then a chain of overflow nodes from 1536 */
    cdq = dir("AAAAA"); chr = dir("AAADC"); cls = dir("AAAIE"); cpt = dir("AAALG");
    CHECK(cdq == 201);
    CHECK(chr == 1536);
    CHECK(cls == 1539);
    CHECK(cpt == 1542);
    CHECK(dir("AAAIE") == cls && dir("AAALG") == cpt && dir("AAAAA") == cdq);
    CHECK(strcmp(gname(cpt), "AAALG") == 0);

    /* a name of exactly eight characters needs its terminator word */
    CHECK(dname(dir("ABCDEFG"), 0) == 0x47464544434241LL);
    x = dir("ABCDEFGH");
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
    CHECK(count == 3000 + 10);

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
    CHECK(convc(dir("777777777777B")) == -1 && convc_const == 0xFFFFFFFFFLL);   /* (36 bits of 1: a number of 64 bits) */
    CHECK(convc(dir("1777777777777777777777B")) == -1 && convc_const == -1);
    v = convc(dir("'ABC"));                     /* string: pointer to words */
    CHECK(v > 0 && FREE(v) == 1);
    CHECK(convc_const == memw(dname(dir("ABC"), 0)));
    v = convc(dir("'ABCDEFGH"));
    CHECK(FREE(v) == 2 && FREE(v + 1) == memw(dname(dir("ABCDEFGH"), 0))
        && FREE(v + 2) == 0);
    CHECK(strcmp(gname(stcon(15)), "17B") == 0);
    CHECK(strcmp(gname(stcon(0)), "0B") == 0);
    CHECK(strcmp(gname(stcon(-1)), "1777777777777777777777B") == 0);
    CHECK(stcon(15) == dir("17B"));
    CHECK(convc(stcon(-77)) == -1 && convc_const == -77);
    CHECK(strcmp(gname(newname("ABCDEFG")), "%ABCD1") == 0);
    CHECK(strcmp(gname(newname("X")), "%X2") == 0);
    CHECK(radi50(dir("ABC")) == (11 * 40 + 12) * 40 + 13);
    CHECK(radi50(dir("A.B$C%D")) == ((((11L * 40 + 37) * 40 + 12) * 40 + 38) * 40 + 13) * 40 + 39);
    DONE();
}
