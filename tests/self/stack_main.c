/* The compiler's own stack module (src/stack.imp, compiled by imp72c) with
 * C stand-ins for the free storage package. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

long fb;                                /* the arena, as IMP sees it */
static long *arena;
static long top = 16;

long stinit(long *), stpush(long *, long *, long *), stack(long *), stpop(long *);
long pushmi(long *), pullyu(long *);
long bits(long *, long *, long *), bitz(long *), bitor(long *, long *, long *), tbit(long *, long *, long *);

/* arrays: index a, header word a-2 holds the length in its right 18 bits */
static long newarr(long n)
{
    long a = top + 4;
    top += n + 300;            /* slack: BITS indexes past the length it asks for */
    arena[a - 2] = n;
    memset(arena + a, 0, (size_t)n * 8);
    return a;
}

long impfallot(long *a, long *n)
{
    long b = newarr(*n);
    if (*a >= 4 && *a < (1L << 18)) {
        long old = arena[*a - 2] & 0777777;
        memcpy(arena + b, arena + *a, (size_t)(old + 250) * 8);
    }
    *a = b;
    return b;
}
long impfaddex(long *a, long *n)
{
    long m = *n + 10;
    impfallot(a, &m);
    return *a;
}
long impfrelot(long *a, long *aa, long *n, long *s) { (void)aa; (void)s; return impfallot(a, n); }
long imperror(long *n, long *s) { (void)s; fprintf(stderr, "error %ld\n", *n); exit(1); }

int main(void)
{
    long nil = 0, a = 1, b = 2, c = 3, s, i, sum = 0, x;

    arena = calloc(1 << 16, 8);
    fb = (long)arena;
    stinit(&nil);
    for (i = 0; i < 40; i++) {
        a = i; b = i * 2; c = i * 3;
        s = stpush(&a, &b, &c);
    }
    nil = 0;
    s = stack(&nil);
    printf("top entry: %ld %ld %ld\n", arena[s], arena[s + 1], arena[s + 2]);
    x = 1;
    stpop(&x);
    nil = 0;
    s = stack(&nil);
    printf("after pop: %ld %ld %ld\n", arena[s], arena[s + 1], arena[s + 2]);
    for (i = 1; i <= 100; i++) pushmi(&i);
    nil = 0;
    for (i = 0; i < 100; i++) sum += pullyu(&nil);
    printf("sum of pops (switch 0): %ld\n", sum);
    {
        long d[3] = { 0, 0, 0 }, r, cc, ok = 1, k;
        static const long set[][2] = { {0,0}, {0,63}, {3,64}, {3,200}, {17,5}, {40,129} };

        for (k = 0; k < 6; k++) {
            r = set[k][0]; cc = set[k][1];
            bits(d, &r, &cc);
        }
        printf("d=%ld %ld %ld\n", d[0], d[1], d[2]); fflush(stdout);
        for (r = 0; r < 45; r++)
            for (cc = 0; cc < 192; cc++) {
                int want = 0;
                for (k = 0; k < 6; k++)
                    if (set[k][0] == r && set[k][1] == cc)
                        want = 1;
                if (tbit(d, &r, &cc) != want) {
                    printf("bit (%ld,%ld) is %ld\n", r, cc, tbit(d, &r, &cc));
                    ok = 0;
                }
            }
        printf("bit matrix %s: %ld rows, %ld words per row less 1\n", ok ? "ok" : "BAD", d[1], d[2]);
        {
            long i2 = 1, j2 = 0, one = 1, z = 3;
            bitor(d, &one, &z);   /* row 1 |= row 3 */
            (void)i2; (void)j2;
            r = 1; cc = 64;
            printf("after bitor row1 col64: %ld\n", tbit(d, &r, &cc));
        }
    }
    return 0;
}
