#include <stdio.h>
long t(long *), u(long *);
int main(void) {
    long n = 10;
    printf("t=%ld u=%ld\n", t(&n), u(&n));
    return 0;
}
