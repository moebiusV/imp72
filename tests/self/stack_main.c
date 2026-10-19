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

/* arrays: index a, header word a-2 holds the length in its right 18 bits */
static long newarr(long n)
{
    long a = top + 4;
    top += n + 8;
    arena[a - 2] = n;
    memset(arena + a, 0, (size_t)n * 8);
    return a;
}

long fallot(long *a, long *n)
{
    long b = newarr(*n);
    if (*a < (1L << 18)) {
        long old = arena[*a - 2] & 0777777;
        memcpy(arena + b, arena + *a, (size_t)(old < *n ? old : *n) * 8);
    }
    *a = b;
    return b;
}
long faddex(long *a, long *n)
{
    long m = *n + 10;
    fallot(a, &m);
    return *a;
}
long frelot(long *a, long *aa, long *n, long *s) { (void)aa; (void)s; return fallot(a, n); }
long error(long *n, long *s) { (void)s; fprintf(stderr, "error %ld\n", *n); exit(1); }

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
    return 0;
}
