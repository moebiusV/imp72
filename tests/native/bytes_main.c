#include <stdio.h>
#include "imp72words.h"
word GETF(word *), SETF(word *, word *);
int main(void) {
    word a = 0xABCDEF, b = 0x12;
    printf("getf=%llx setf=%llx\n", W(GETF(&a)), W(SETF(&a, &b)));
    return 0;
}
