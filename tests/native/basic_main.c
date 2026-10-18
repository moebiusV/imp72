#include <stdio.h>
long add2(long *, long *);
long mul3(long *);
long maxv(long *, long *);
long sumto(long *);
int main(void) {
    long a = 20, b = 22, n = 10, x = 14, p = 7, q = 9;
    printf("add2=%ld mul3=%ld maxv=%ld,%ld sumto=%ld\n", add2(&a, &b), mul3(&x),
        maxv(&p, &q), maxv(&q, &p), sumto(&n));
    return 0;
}
