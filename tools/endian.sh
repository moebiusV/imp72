#!/bin/sh
# endian.sh: the file of byte-order macros (src/impend-le.imc or src/impend-be.imc) for the machine the compiler is built on
. "$(dirname "$0")/mktmpdir.sh"; mktmpdir endian
printf 'int main(void){unsigned x=1;return *(unsigned char *)&x;}\n' > $t/e.c
if ${CC:-cc} -o $t/e $t/e.c 2>/dev/null && $t/e; then echo src/impend-be.imc; else echo src/impend-le.imc; fi
