#include <stdio.h>
long t(long *), u(long *), neg(long *);
int main(void) {
    long n = 10;
    printf("t=%ld u=%ld neg=%ld\n", t(&n), u(&n), neg(&n));
    return 0;
}
