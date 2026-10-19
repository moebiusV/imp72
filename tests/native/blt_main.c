#include <stdio.h>
long fill(long *), copy(long *), sumq(long *);
int main(void) {
    long n = 10;
    fill(&n);
    copy(&n);
    printf("sumq=%ld\n", sumq(&n));
    return 0;
}
