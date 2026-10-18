#include <stdio.h>
long fill(long *), total(long *), fact(long *), gcd(long *, long *), twice(long *), caller(long *), bits(long *), rem(long *, long *);
int main(void) {
    long n = 10, a = 5, b = 84, c = 36, d = 12345, e = 17, f = 5;
    fill(&n);
    printf("total=%ld fact=%ld gcd=%ld twice=%ld caller=%ld bits=%ld rem=%ld\n", total(&n), fact(&a), gcd(&b, &c), twice(&e), caller(&f), bits(&d), rem(&e, &f));
    return 0;
}
