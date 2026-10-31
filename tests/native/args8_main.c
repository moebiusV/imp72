#include <stdio.h>
#include "imp72words.h"
word SUM8(word*,word*,word*,word*,word*,word*,word*,word*), VIA(word*);
int main(void) {
    word a[8] = {1,2,3,4,5,6,7,8}, x = 10;
    printf("sum8=%lld via=%lld\n", W(SUM8(a,a+1,a+2,a+3,a+4,a+5,a+6,a+7)), W(VIA(&x)));
    return 0;
}
