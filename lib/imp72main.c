/*
 * imp72main.c -- the C main of an IMP72 program: the main program of the
 * source (the statements before its first SUBR) is the function imp_main.
 * It is in a library of its own so that a program with a main of its own
 * does not link this one.  The command line is kept for the program to ask
 * for with CCALL: impargc(0) and impargvp(0) for the array.
 */
extern long imp_main(void);

static int ac;
static char **av;

long impargc(long z) { (void)z; return ac; }
long impargvp(long z) { (void)z; return (long)(void *)av; }

int
main(int argc, char **argv)
{
    ac = argc;
    av = argv;
    imp_main();
    return 0;
}
