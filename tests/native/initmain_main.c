#include <stdio.h>
#include "imp.h"

/* a C program with a main of its own: imp_init gives the runtime the command line, imp_run is the main program of the IMP source */
int main(int argc, char **argv) {
    imp_init(argc, argv);
    printf("before\n");
    fflush(stdout);
    imp_run();
    printf("after\n");
    return 0;
}
