#!/bin/sh
# stage1.sh: build build/stage1/imp72s1, the stage 0 compiler with the modules
# listed in MODULES replaced by the compiler's own sources (src/NAME.imp)
# compiled by imp72stage0.  Each IMP module is built next to the C one, its
# functions renamed impl_NAME, and stage0/NAME_imp.c adapts the C interface
# to it.  Run from the top of the tree after `make`.
set -e
command -v objcopy >/dev/null 2>&1 || { echo "stage1: no objcopy (ELF only)" >&2; exit 77; }
nopie=-no-pie; [ "$(uname)" = Darwin ] && nopie=
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
modules=${MODULES:-stack dir graph parse ercor entree cotree dosem rsyn lex}
out=build/stage1
mkdir -p $out
export CC1=${CC1:-$root/build/pcc/cc/ccom/cc1}
objs=
repl=
for m in $modules; do
    stage0/imp72stage0 -c -o $out/$m.imp.o src/impsyn64.imc src/$m.imp >$out/$m.log 2>&1 \
        || { cat $out/$m.log >&2; echo "stage1: $m failed" >&2; exit 1; }
    # every symbol the module defines, functions and the common variables, gets
    # the prefix, except the names it shares with the C side (src/shared.txt):
    # IMP has one flat name space and C has functions called val and pstack
    syms=$(nm $out/$m.imp.o | awk '$2 ~ /^[TCBD]$/ { print $3 }' | grep -vx -f src/shared.txt || true)
    args=
    for s in $syms; do args="$args --redefine-sym $s=impl_$s"; done
    objcopy $args $out/$m.imp.o $out/$m.imp2.o
    cc -std=c99 -fcommon -O1 -g -Wall -Wextra -pedantic -Istage0 -c stage0/${m}_imp.c -o $out/${m}_c.o
    objs="$objs $out/$m.imp2.o $out/${m}_c.o"
    repl="$repl stage0/$m.o"
done
# the C routines that IMP modules call, by reference
cc -std=c99 -fcommon -O1 -g -Wall -Wextra -pedantic -Istage0 -c stage0/impglue.c -o $out/impglue.o
objs="$objs $out/impglue.o"
keep=
for o in stage0/*.o; do
    case $o in stage0/imp72s0.o) continue ;; esac
    skip=0
    for r in $repl; do [ "$o" = "$r" ] && skip=1; done
    [ $skip = 1 ] || keep="$keep $o"
done
cc $nopie -o $out/imp72s1 stage0/imp72s0.o $keep $objs build/lib/libimp72.a
echo "built $out/imp72s1 with the compiler's own: $modules"
