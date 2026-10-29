/*
 * ir2ll_shim.c -- ir2ll_run for stage0/llvmorc.c, which is C and calls it with its arguments by value: the LLVM IR writer of imp72 is IMP (src/ir2ll.imp, src/ir2llx.imp), whose routine
 * takes pointers to words (IMP passes by reference) and returns a word.  The seed does not use this: it links tools/ir2ll.c.  This file goes when llvmorc.c moves to IMP, which will call
 * IR2LLRUN itself.
 */
#include <stdint.h>
#include <stdio.h>

extern int64_t impl_ir2llrun(int64_t *in, int64_t *out, int64_t *p32);          /* src/ir2ll.imp (tools/direct.sh gives its symbols the prefix impl_) */

int
ir2ll_run(FILE *in, FILE *out, int p32)
{
    int64_t a = (int64_t)(intptr_t)in, b = (int64_t)(intptr_t)out, c = p32;

    return (int)impl_ir2llrun(&a, &b, &c);
}
