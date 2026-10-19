#include <stdio.h>
#include "imp72words.h"
word getf(word *), setf(word *, word *);
int main(void) {
    word a = 0xABCDEF, b = 0x12;
    printf("getf=%llx setf=%llx\n", W(getf(&a)), W(setf(&a, &b)));
    return 0;
}
