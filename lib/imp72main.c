/*
 * imp72main.c -- the C main of an IMP72 program: the main program of the
 * source (the statements before its first SUBR) is the function imp_main.
 * It is in a library of its own so that a program with a main of its own
 * does not link this one.
 */
extern long imp_main(void);

int
main(void)
{
    imp_main();
    return 0;
}
