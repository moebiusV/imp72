#!/bin/sh
# direct.sh: build build/direct/imp72d, the kernel with the code generator of src/llvm-ir (the translation of the tape's code*.imc and
# impsem.imc whose instructions are those of IR, docs/porting/llvm-ir-machine.md) in place of codegen.c, impsem.c, trans.c and pcode.c.
# The modules are compiled by imp72stage0 (the compiler with the old code generator), each next to the others: their symbols
# are renamed impl_NAME (all of them in all the objects, so that the modules call one another by name), except the variables the C
# side shares (src/llvm-ir/shared.txt and src/shared.txt); there is no C of the compiler's in the result but the LLVM glue and the library.  Run from the top
# of the tree after `make`.  imp72d with the syntax src/llvm-ir/syntax64.imc compiles IMP to IR itself.
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
. tools/limits.sh                  # a runaway job dies of its own limits (file size, CPU), not of the disk
modules=${MODULES:-impsem code code2 code3 demit ix}
srcmodules=${SRC_MODULES:-free debug stack dir graph parse ercor entree cotree dosem rsyn lex image impm ir2ll ir2llx driver llvm llvmgen}   # the modules of src/ (the rest of the compiler)
out=${DIRECT_OUT:-build/direct}
mkdir -p "$out"
imprename=$out/imprename                    # what the build uses of nm and objcopy, for ELF and Mach-O (tools/imprename.imp, built below by the driver)
driver=${DIRECT_DRIVER:-stage0/imp72stage0}
case $driver in stage0/*) unset IMP72SYNTAX IMP72FFI IMP72LIB ;; esac     # the seed reads tape/syntax.imc, whatever the environment says
mkdir -p $out
# the byte order of the machine the compiler is built for: the modules say CHR0, REST and the rest (src/impend-le.imc, src/impend-be.imc)
endf=$(sh tools/endian.sh)
export IMP72ENDIAN=$endf
sh tools/capped.sh $out/imprename.log $driver -o "$imprename" tools/imprename.imp || { dd bs=100000 count=1 if=$out/imprename.log 2>/dev/null >&2; echo "direct: imprename failed" >&2; exit 1; }
for m in $modules; do
    sh tools/capped.sh $out/$m.log env IMP72MODULE=$m $driver -c -o $out/$m.imp.o $endf src/impsyn64.imc src/llvm-ir/irsyn.imc src/llvm-ir/$m.imp \
        || { dd bs=100000 count=1 if=$out/$m.log 2>/dev/null >&2; echo "direct: $m failed" >&2; exit 1; }
done
oldmodules=${OLD_SRC_MODULES:-}               # (debugging) the ones of them that the old code generator compiles
sh tools/llvm.sh imp > $out/llvmgen.imp                          # what the LLVM and the lld of this build decide, as a module (src/llvm.imp)
for m in $srcmodules; do
    x=; case $m in free) x=lib/ffi.imc ;; esac
    f=src/$m.imp; case $m in llvmgen) f=$out/llvmgen.imp ;; esac
    d=$driver
    case " $oldmodules " in *" $m "*) d="env -u IMP72SYNTAX -u IMP72FFI -u IMP72LIB stage0/imp72stage0" ;; esac
    sh tools/capped.sh $out/$m.log env IMP72MODULE=$m $d -c -o $out/$m.imp.o $x $endf src/impsyn64.imc $f \
        || { dd bs=100000 count=1 if=$out/$m.log 2>/dev/null >&2; echo "direct: $m failed" >&2; exit 1; }
done
allmodules="$modules $srcmodules"
# every symbol the modules define (src/llvm-ir and src/), except the ones shared with the C side, gets the prefix in all of them: they call one another by the
# tape's names.  What they call and do not define is the C side's
syms=
for m in $modules $srcmodules; do
    syms="$syms $($imprename -d $out/$m.imp.o)"
done
syms=$(printf '%s\n' $syms | sort -u)
# the names are as the modules write them (ST); the C side's are lower case (st): the ones they share are renamed to the C side's, the others get the prefix
cat src/shared.txt src/llvm-ir/shared.txt > $out/shared.all
for s in $syms; do
    l=$(printf '%s' "$s" | tr 'A-Z' 'a-z')
    if grep -qx -- "$l" $out/shared.all && [ "$s" != "$l" ]; then echo "$s $l"
    elif grep -qx -- "$s" $out/shared.all; then :
    else echo "$s impl_$s"; fi
done > $out/rename.map
objs=
repl=
for m in $modules $srcmodules; do
    $imprename -f $out/rename.map $out/$m.imp.o $out/$m.imp2.o
    objs="$objs $out/$m.imp2.o"
done
for m in $srcmodules; do
    repl="$repl stage0/$m.o"
done
# LLVM in the process (src/llvm.imp, and the IR writer src/ir2ll.imp and src/ir2llx.imp): the compiler needs the LLVM C API (tools/llvm.sh), and links lld's driver (stage0/impld.cpp) where lld is a
# library.  There is no C of the compiler's but that
llvmflags=$(sh tools/llvm.sh cflags); llvmlibs=; llvmobjs=
[ -n "$llvmflags" ] || { echo "direct: imp72 needs the LLVM libraries (llvm-config, tools/llvm.sh)" >&2; exit 1; }
llvmlibs=$(sh tools/llvm.sh libs)
case $llvmflags in *HAVE_LLD*) c++ -std=c++17 -O1 -g -fno-rtti $llvmflags -c stage0/impld.cpp -o $out/impld.o; llvmobjs="$llvmobjs $out/impld.o" ;; esac
keep=
for o in stage0/*.o; do
    case $o in stage0/imp72s0.o|stage0/llvmorc.o|stage0/ir2ll.o|stage0/impld.o|stage0/imphost.o|stage0/error.o|stage0/pr.o|stage0/ir.o|stage0/semtab.o|stage0/codegen.o|stage0/impsem.o|stage0/sem.o|stage0/trans.o|stage0/pcode.o) continue ;; esac
    skip=0
    for r in $repl; do [ "$o" = "$r" ] && skip=1; done
    [ $skip = 1 ] || keep="$keep $o"
done
cc $(sh tools/nopie.sh) -o $out/imp72d $keep $objs $llvmobjs ${DIRECT_LIB:-build/lib/libimp72.a} $llvmlibs
ln -sf imp72d $out/imp72stage-llvm-ir
echo "built $out/imp72d with the code generator of src/llvm-ir: $modules; the compiler's own: $srcmodules"
