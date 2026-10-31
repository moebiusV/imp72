#include <stdio.h>
#include "imp72words.h"
word MIXED(word *);
/* IMP names are case sensitive, as the original's are, and a symbol is the name as it is written: MIXED is the SUBR, CountDown and plain are C's */
long CountDown(long n) { return n * 10; }
long plain(long n) { return n + 1; }
int main(void) {
    word a = 7;
    printf("mixed=%lld\n", W(MIXED(&a)));
    return 0;
}
