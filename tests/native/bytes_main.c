#include <stdio.h>
long getf(long *), setf(long *, long *);
int main(void) {
    long a = 0xABCDEF, b = 0x12;
    printf("getf=%lx setf=%lx\n", getf(&a), setf(&a, &b));
    return 0;
}
