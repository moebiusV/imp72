#include <stdio.h>
#include "imp72words.h"
word sum8(word*,word*,word*,word*,word*,word*,word*,word*), via(word*);
int main(void) {
    word a[8] = {1,2,3,4,5,6,7,8}, x = 10;
    printf("sum8=%lld via=%lld\n", W(sum8(a,a+1,a+2,a+3,a+4,a+5,a+6,a+7)), W(via(&x)));
    return 0;
}
