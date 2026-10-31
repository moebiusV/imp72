/* ir2ll_drv IR OUT P32: the LLVM IR writer on a file of IR.  With -DREF it is tools/ir2ll.c (the C the seed links); without, the IMP one (src/ir2ll.imp, src/ir2llx.imp) whose
   routine takes pointers to words, as an IMP routine does (src/llvm.imp calls it so).  tests/direct/ir2ll.sh compares the two. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef REF
extern int ir2ll_run(FILE *in, FILE *out, int p32);
#else
extern int64_t impl_IR2LLRUN(int64_t *in, int64_t *out, int64_t *p32);
static int
ir2ll_run(FILE *in, FILE *out, int p32)
{
    int64_t a = (int64_t)(intptr_t)in, b = (int64_t)(intptr_t)out, c = p32;

    return (int)impl_IR2LLRUN(&a, &b, &c);
}
#endif

int
main(int argc, char **argv)
{
    FILE *in, *out;
    int rc;

    if (argc != 4 || (in = fopen(argv[1], "r")) == NULL || (out = fopen(argv[2], "w")) == NULL)
        return 2;
    rc = ir2ll_run(in, out, atoi(argv[3]));
    fclose(in);
    fclose(out);
    return rc;
}
