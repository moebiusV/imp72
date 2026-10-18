/* The lexer, over a small file. */
#include <string.h>
#include "imp.h"
#include "check.h"

static const char *want[] = {
    "<", "EXP", ">", ":", ":", "=", "X", "+", "'AB'C", "hi there", "12B", ";",
    "foo", "(", "BAR", ")", "%", NULL
};

int
main(void)
{
    const char *path = "test_lex.tmp";
    FILE *f = fopen(path, "w");
    word s;
    int i;

    fputs("<EXP>::= X + 'AB''C' !hi there! # a comment\n  with ( ) # 12B;\n"
        "foo(BAR)\r\n%\n", f);
    fclose(f);
    CHECK(lexin("no/such/file") == -1);
    CHECK(lexin(path) == 0);
    for (i = 0; want[i] != NULL; i++) {
        lex(&s);
        if (strcmp(gname(s), want[i]) != 0) {
            fprintf(stderr, "token %d: got '%s' want '%s'\n", i, gname(s),
                want[i]);
            CHECK(0);
            break;
        }
    }
    remove(path);
    DONE();
}
