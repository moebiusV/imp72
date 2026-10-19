#include <stdio.h>
long getp(long *, long *), walk(long *);
int main(void) {
    long a = 0x1234, n = 0x77, w[2] = { 0x0807060504030201L, 0x1817161514131211L };
    long r = getp(&a, &n);
    printf("getp=%ld a=%lx\n", r, a);
    printf("walk=%lx\n", walk(w));
    return 0;
}
