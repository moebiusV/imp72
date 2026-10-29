#include <stdio.h>
#include "imp72words.h"
word mixed(word *);
/* a name with a lower case letter in IMP is a foreign name as it is written: CountDown is not countdown; a name in capitals only is the lower case one */
long CountDown(long n) { return n * 10; }
long plain(long n) { return n + 1; }
int main(void) {
    word a = 7;
    printf("mixed=%lld\n", W(mixed(&a)));
    return 0;
}
