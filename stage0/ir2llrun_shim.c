/*
 * ir2llrun_shim.c -- until tools/ir2ll.c is IMP: the entry src/llvm.imp calls, with the arguments by reference as an IMP routine takes them (the build renames the symbols
 * that a module defines, and this one is only referenced: the name stays), over the C ir2ll_run.
 */
#include <stdio.h>

extern int ir2ll_run(FILE *in, FILE *out, int p32);

long
ir2llrun(long *in, long *out, long *p32)
{
    return ir2ll_run((FILE *)(long)*in, (FILE *)(long)*out, (int)*p32);
}
