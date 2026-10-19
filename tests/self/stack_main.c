/* The compiler's own stack module (src/stack.imp, compiled by imp72stage0) with
 * C stand-ins for the free storage package. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int64_t word;
#define W(x) ((long long)(x))

word fb;                                /* the arena, as IMP sees it */
static word *arena;
static word top = 16;

word stinit(word *), stpush(word *, word *, word *), stack(word *), stpop(word *);
word pushmi(word *), pullyu(word *);
word bits(word *, word *, word *), bitz(word *), bitor(word *, word *, word *), tbit(word *, word *, word *);

/* arrays: index a, header word a-2 holds the length in its right 18 bits */
static word newarr(word n)
{
    word a = top + 4;
    top += n + 300;            /* slack: BITS indexes past the length it asks for */
    arena[a - 2] = n;
    memset(arena + a, 0, (size_t)n * 8);
    return a;
}

word impfallot(word *a, word *n)
{
    word b = newarr(*n);
    if (*a >= 4 && *a < (1L << 18)) {
        word old = arena[*a - 2] & 0777777;
        memcpy(arena + b, arena + *a, (size_t)(old + 250) * 8);
    }
    *a = b;
    return b;
}
word impfaddex(word *a, word *n)
{
    word m = *n + 10;
    impfallot(a, &m);
    return *a;
}
word impfrelot(word *a, word *aa, word *n, word *s) { (void)aa; (void)s; return impfallot(a, n); }
word imperror(word *n, word *s) { (void)s; fprintf(stderr, "error %lld\n", W(*n)); exit(1); }

int main(void)
{
    word nil = 0, a = 1, b = 2, c = 3, s, i, sum = 0, x;

    arena = calloc(1 << 16, 8);
    fb = (word)(intptr_t)arena;
    stinit(&nil);
    for (i = 0; i < 40; i++) {
        a = i; b = i * 2; c = i * 3;
        s = stpush(&a, &b, &c);
    }
    nil = 0;
    s = stack(&nil);
    printf("top entry: %lld %lld %lld\n", W(arena[s]), W(arena[s + 1]), W(arena[s + 2]));
    x = 1;
    stpop(&x);
    nil = 0;
    s = stack(&nil);
    printf("after pop: %lld %lld %lld\n", W(arena[s]), W(arena[s + 1]), W(arena[s + 2]));
    for (i = 1; i <= 100; i++) pushmi(&i);
    nil = 0;
    for (i = 0; i < 100; i++) sum += pullyu(&nil);
    printf("sum of pops (switch 0): %lld\n", W(sum));
    {
        word d[3] = { 0, 0, 0 }, r, cc, ok = 1, k;
        static const word set[][2] = { {0,0}, {0,63}, {3,64}, {3,200}, {17,5}, {40,129} };

        for (k = 0; k < 6; k++) {
            r = set[k][0]; cc = set[k][1];
            bits(d, &r, &cc);
        }
        printf("d=%lld %lld %lld\n", W(d[0]), W(d[1]), W(d[2])); fflush(stdout);
        for (r = 0; r < 45; r++)
            for (cc = 0; cc < 192; cc++) {
                int want = 0;
                for (k = 0; k < 6; k++)
                    if (set[k][0] == r && set[k][1] == cc)
                        want = 1;
                if (tbit(d, &r, &cc) != want) {
                    printf("bit (%lld,%lld) is %lld\n", W(r), W(cc), W(tbit(d, &r, &cc)));
                    ok = 0;
                }
            }
        printf("bit matrix %s: %lld rows, %lld words per row less 1\n", ok ? "ok" : "BAD", W(d[1]), W(d[2]));
        {
            word i2 = 1, j2 = 0, one = 1, z = 3;
            bitor(d, &one, &z);   /* row 1 |= row 3 */
            (void)i2; (void)j2;
            r = 1; cc = 64;
            printf("after bitor row1 col64: %lld\n", W(tbit(d, &r, &cc)));
        }
    }
    return 0;
}
