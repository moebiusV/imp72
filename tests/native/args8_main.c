#include <stdio.h>
long sum8(long*,long*,long*,long*,long*,long*,long*,long*), via(long*);
int main(void) {
    long a[8] = {1,2,3,4,5,6,7,8}, x = 10;
    printf("sum8=%ld via=%ld\n", sum8(a,a+1,a+2,a+3,a+4,a+5,a+6,a+7), via(&x));
    return 0;
}
