#!/bin/sh
# nopie.sh: -no-pie where the C compiler knows it (GCC before 6 does not, and its programs are not position independent anyway)
t=$(mktemp -d "${TMPDIR:-/tmp}/nopie.XXXXXX") || exit 1
printf 'int main(void){return 0;}\n' > $t/n.c
if cc -no-pie -o $t/n $t/n.c >/dev/null 2>&1; then echo -no-pie; fi
rm -rf $t
