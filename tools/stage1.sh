#!/bin/sh
# stage1.sh: build build/stage1/imp72s1, the stage 0 compiler with the modules
# listed in MODULES replaced by the compiler's own sources (src/NAME.imp)
# compiled by imp72stage0.  Each IMP module is built next to the C one, its
# functions renamed impl_NAME, and stage0/NAME_imp.c adapts the C interface
# to it.  Run from the top of the tree after `make`.
set -e
# Which objcopy renames the symbols.  Prefer LLVM's: OpenBSD's base GNU objcopy is binutils 2.17, which renumbers the
# symbol table but not the signature index of the COMDAT groups clang emits for RETGUARD, so lld then drops the cookie
# sections and the cookies resolve to address 0 (every function faults on its first instruction).
if [ -z "$OBJCOPY" ]; then
    if command -v llvm-objcopy >/dev/null 2>&1; then OBJCOPY=llvm-objcopy
    elif command -v objcopy >/dev/null 2>&1; then OBJCOPY=objcopy
    else echo "stage1: no objcopy (ELF only)" >&2; exit 77; fi
fi
# macOS has no ELF objcopy (the test is skipped); elsewhere the link is non-PIE like the pcc-made objects need.
if [ -n "${STAGE1_NOPIE+x}" ]; then nopie=$STAGE1_NOPIE; else
    case "$(uname)" in Darwin) nopie= ;; *) nopie=-no-pie ;; esac
fi
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
modules=${MODULES:-free debug stack dir graph parse ercor entree cotree dosem rsyn lex}
out=${STAGE_OUT:-build/stage1}
driver=${STAGE_DRIVER:-stage0/imp72stage0}
name=${STAGE_NAME:-imp72s1}
mkdir -p $out
export CC1=${CC1:-$root/build/pcc/cc/ccom/cc1}
objs=
repl=
for m in $modules; do
    $driver -c -o $out/$m.imp.o src/impsyn64.imc src/$m.imp >$out/$m.log 2>&1 \
        || { cat $out/$m.log >&2; echo "stage1: $m failed" >&2; exit 1; }
    # every symbol the module defines, functions and the common variables, gets
    # the prefix, except the names it shares with the C side (src/shared.txt):
    # IMP has one flat name space and C has functions called val and pstack
    syms=$(nm $out/$m.imp.o | awk '$2 ~ /^[TCBD]$/ { print $3 }' | grep -vx -f src/shared.txt || true)
    args=
    for s in $syms; do args="$args --redefine-sym $s=impl_$s"; done
    $OBJCOPY $args $out/$m.imp.o $out/$m.imp2.o
    cc -std=c99 -fcommon -O1 $STAGE1_CFLAGS -g -Wall -Wextra -pedantic -Istage0 -c stage0/${m}_imp.c -o $out/${m}_c.o
    objs="$objs $out/$m.imp2.o $out/${m}_c.o"
    repl="$repl stage0/$m.o"
done
# the C routines that IMP modules call, by reference
cc -std=c99 -fcommon -O1 $STAGE1_CFLAGS -g -Wall -Wextra -pedantic -Istage0 -c stage0/impglue.c -o $out/impglue.o
objs="$objs $out/impglue.o"
keep=
for o in stage0/*.o; do
    case $o in stage0/imp72s0.o) continue ;; esac
    skip=0
    for r in $repl; do [ "$o" = "$r" ] && skip=1; done
    [ $skip = 1 ] || keep="$keep $o"
done
cc $nopie -o $out/$name stage0/imp72s0.o $keep $objs build/lib/libimp72.a
echo "built $out/$name with the compiler's own: $modules"
