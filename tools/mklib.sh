#!/bin/sh
# mklib.sh: build build/lib/libimp72.a, the runtime library of the programs IMP72 compiles.  The runtime (PRINT, READ, FINI, CALLI, BLT, IFIX, the shifts, the byte
# pointers) is IMP, lib/imp72rt.imp and lib/imp72io.imp, compiled by the seed (stage0/imp72stage0) into one object; lib/imp72sys.c has what only C can say and
# lib/imp72main.c is the program's main.  Run from the top of the tree after `make` has built stage0.
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
out=${LIBOUT:-build/lib}
mkdir -p "$out"
export IMP72WIDE=1                          # the seed folds the constants in 64 bits
export IMP72CHARPTR=1                       # a byte pointer in a variable points at characters: no calls of the routines this library is made of
endf=$(sh tools/endian.sh)
# one source: the files are two modules of one program (a program ends at its first %%)
{ sed '/^%%$/d' lib/imp72rt.imp; sed '1d' lib/imp72io.imp; } > "$out/imp72rt_all.imp"
env -u IMP72SYNTAX -u IMP72FFI -u IMP72LIB stage0/imp72stage0 -c -o "$out/imp72rt_imp.o" "$endf" "$out/imp72rt_all.imp"
# no stack protector: the program is linked by lld with the C library's files alone, and a compiler that protects the stack by default (Alpine's) wants a helper that is not there
cc -std=c99 ${CFLAGS:--O1 -g -Wall} -fno-stack-protector -c -o "$out/imp72sys.o" lib/imp72sys.c
cc -std=c99 ${CFLAGS:--O1 -g -Wall} -fno-stack-protector -c -o "$out/imp72main.o" lib/imp72main.c
bd=$(sh tools/llvm.sh bindir); hostdef=; [ -n "$bd" ] && hostdef="-DIMP72_LLVM_BINDIR=$bd"            # where lld is as a program, if it is not in the PATH
cc -std=c99 ${CFLAGS:--O1 -g -Wall} -fno-stack-protector $hostdef -c -o "$out/imphost.o" lib/imphost.c
rm -f "$out/libimp72.a"
rtobjs=
rt=$(sh tools/llvm.sh builtins)             # the 32 bit targets: the routines for the division of 64 bits, from compiler-rt, are part of the library (no libgcc)
if [ -n "$rt" ]; then
    rm -rf "$out/rt"; mkdir -p "$out/rt"
    case $rt in /*) rtabs=$rt ;; *) rtabs=$root/$rt ;; esac
    ( cd "$out/rt" && ar x "$rtabs" )
    rtobjs=$(ls "$out"/rt/*.o)
fi
ar rcs "$out/libimp72.a" "$out/imp72rt_imp.o" "$out/imp72sys.o" "$out/imp72main.o" "$out/imphost.o" $rtobjs
