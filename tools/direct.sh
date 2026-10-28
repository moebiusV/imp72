#!/bin/sh
# direct.sh: build build/direct/imp72d, the kernel with the code generator of src/pccir (the translation of the tape's code*.imc and
# impsem.imc whose instructions are those of pcc IR, docs/porting/pccir-machine.md) in place of codegen.c, impsem.c, trans.c and pcode.c.
# The modules are compiled by imp72stage0 (the compiler with the old code generator), each next to the others: their symbols
# are renamed impl_NAME (all of them in all the objects, so that the modules call one another by name), except the variables the C
# side shares (src/pccir/shared.txt and src/shared.txt); stage0/pccirsem.c registers the routines and has the C helpers.  Run from the top
# of the tree after `make`.  imp72d with the syntax src/pccir/syntax64.imc compiles IMP to pcc IR itself.
set -e
if [ -z "$OBJCOPY" ]; then
    if command -v llvm-objcopy >/dev/null 2>&1; then OBJCOPY=llvm-objcopy
    elif command -v objcopy >/dev/null 2>&1; then OBJCOPY=objcopy
    else echo "direct: no objcopy (ELF only)" >&2; exit 77; fi
fi
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
modules=${MODULES:-impsem code code2 code3 demit}
srcmodules=${SRC_MODULES:-free debug stack dir graph parse ercor entree cotree dosem rsyn lex impm}   # the modules of src/ (the rest of the compiler)
out=${DIRECT_OUT:-build/direct}
driver=${DIRECT_DRIVER:-stage0/imp72stage0}
case $driver in stage0/*) unset IMP72SYNTAX IMP72FFI IMP72LIB ;; esac     # the seed reads decus/syntax.imc, whatever the environment says
mkdir -p $out
export IMP72WIDE=1                          # the seed folds the constants of the tape in 64 bits
# the byte order of the machine the compiler is built for: the modules say CHR0, REST and the rest (src/impend-le.imc, src/impend-be.imc)
endf=$(sh tools/endian.sh)
export IMP72ENDIAN=$endf
export CC1=${CC1:-$root/build/pcc/cc/ccom/cc1}
for m in $modules; do
    IMP72MODULE=$m $driver -c -o $out/$m.imp.o $endf src/impsyn64.imc src/pccir/irsyn.imc src/pccir/$m.imp >$out/$m.log 2>&1 \
        || { cat $out/$m.log >&2; echo "direct: $m failed" >&2; exit 1; }
done
oldmodules=${OLD_SRC_MODULES:-}               # (debugging) the ones of them that the old code generator compiles
for m in $srcmodules; do
    x=; case $m in free) x=lib/ffi.imc ;; esac
    d=$driver
    case " $oldmodules " in *" $m "*) d="env -u IMP72SYNTAX -u IMP72FFI -u IMP72LIB stage0/imp72stage0" ;; esac
    IMP72MODULE=$m $d -c -o $out/$m.imp.o $x $endf src/impsyn64.imc src/$m.imp >$out/$m.log 2>&1 \
        || { cat $out/$m.log >&2; echo "direct: $m failed" >&2; exit 1; }
done
allmodules="$modules $srcmodules"
# every symbol the modules define (src/pccir and src/), except the ones shared with the C side, gets the prefix in all of them: they call one another by the
# tape's names.  What they call and do not define is the C side's
syms=
for m in $modules $srcmodules; do
    syms="$syms $(nm $out/$m.imp.o | awk '$2 ~ /^[TCBD]$/ { print $3 }')"
done
syms=$(printf '%s\n' $syms | sort -u | grep -vx -f src/shared.txt | grep -vx -f src/pccir/shared.txt || true)
args=
for s in $syms; do args="$args --redefine-sym $s=impl_$s"; done
objs=
repl=
for m in $modules $srcmodules; do
    $OBJCOPY $args $out/$m.imp.o $out/$m.imp2.o
    objs="$objs $out/$m.imp2.o"
done
for m in $srcmodules; do
    if [ -f stage0/${m}_imp.c ]; then
        cc -std=c99 -fcommon -O1 -g -Wall -Wextra -pedantic -Istage0 -c stage0/${m}_imp.c -o $out/${m}_c.o
        objs="$objs $out/${m}_c.o"
    fi
    repl="$repl stage0/$m.o"
done
cc -std=c99 -fcommon -O1 -g -Wall -Wextra -pedantic -Istage0 -c stage0/pccirsem.c -o $out/pccirsem.o
cc -std=c99 -fcommon -O1 -g -Wall -Wextra -pedantic -Istage0 -c stage0/sys.c -o $out/sys.o
cc -std=c99 -fcommon -O1 -g -Wall -Wextra -pedantic -Istage0 -c stage0/image.c -o $out/image.o
cc -std=c99 -fcommon -O1 -g -Wall -Wextra -pedantic -Istage0 -DIMP_IMPK -c stage0/imp72s0.c -o $out/imp72s0.o
objs="$objs $out/pccirsem.o $out/sys.o $out/image.o"
keep=
for o in stage0/*.o; do
    case $o in stage0/imp72s0.o|stage0/error.o|stage0/flavour0.o|stage0/codegen.o|stage0/impsem.o|stage0/sem.o|stage0/trans.o|stage0/pcode.o) continue ;; esac
    skip=0
    for r in $repl; do [ "$o" = "$r" ] && skip=1; done
    [ $skip = 1 ] || keep="$keep $o"
done
cc -no-pie -o $out/imp72d $out/imp72s0.o $keep $objs ${DIRECT_LIB:-build/lib/libimp72.a}
ln -sf imp72d $out/imp72stage-pccir
echo "built $out/imp72d with the code generator of src/pccir: $modules; the compiler's own: $srcmodules"
