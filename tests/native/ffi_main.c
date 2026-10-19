#include <stdio.h>
#include <stdlib.h>
long f(long *, long *), g(long *);
long cadd3(long a, long b, long c) { return a * 10000 + b * 100 + c; }
long cnone(void) { return 42; }
int main(void) {
    long a = 3, b = 4;
    printf("f=%ld g=%ld\n", f(&a, &b), g(&a));
    return 0;
}
