#!/bin/sh
# endian.sh: the file of byte-order macros (src/impend-le.imc or src/impend-be.imc) for the machine the compiler is built on
t=$(mktemp -d "${TMPDIR:-/tmp}/endian.XXXXXX") || exit 1
printf 'int main(void){unsigned x=1;return *(unsigned char *)&x;}\n' > $t/e.c
if cc -o $t/e $t/e.c 2>/dev/null && $t/e; then echo src/impend-be.imc; else echo src/impend-le.imc; fi
rm -rf $t
