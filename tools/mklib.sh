#!/bin/sh
# mklib.sh: build build/lib/libimp72.a, the runtime library of the programs IMP72 compiles.  The runtime (PRINT, READ, FINI, CALLI, BLT, IFIX, the shifts, the byte
# pointers) is IMP, lib/imp72rt.imp and lib/imp72io.imp, compiled by the seed (stage0/imp72stage0) into one object; lib/imp72sys.imp, impvars.imp, imphost.imp and the generated sysgen.imp have what the system says in its headers, and
# lib/imp72main.imp is the program's main (in a member of its own, so that a program with a main of its own does not link it).  Run from the top of the tree after `make` has built stage0.
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
out=${LIBOUT:-build/lib}
mkdir -p "$out"
export IMP72CHARPTR=1                       # a byte pointer in a variable points at characters: no calls of the routines this library is made of
endf=$(sh tools/endian.sh)
# one source: the files are two modules of one program (a program ends at its first %%)
{ sed '/^%%$/d' lib/imp72rt.imp; sed '1d' lib/imp72io.imp; } > "$out/imp72rt_all.imp"
env -u IMP72SYNTAX -u IMP72FFI -u IMP72LIB stage0/imp72stage0 -c -o "$out/imp72rt_imp.o" "$endf" "$out/imp72rt_all.imp"
# the system's own constants (tools/sysconst.sh), and what only a system call can say, in IMP too: no C compiler is needed for any of the library
bd=$(sh tools/llvm.sh bindir)                          # where lld is as a program, if it is not in the PATH
sysos=${SYSOS:-$(uname -s | tr 'A-Z' 'a-z')}
sh tools/sysconst.sh "$sysos" "${SYSARCH:-$(uname -m)}" "$bd" > "$out/sysgen.imp"
case $sysos in darwin) vfmt=macho ;; *) vfmt=elf ;; esac
for m in "imp72sys:lib/imp72sys.imp" "sysgen:$out/sysgen.imp" "impvars:lib/impvars.imp" "impvarsx:lib/impvarsx-$vfmt.imp" "imphost:lib/imphost.imp" "imp72main:lib/imp72main.imp"; do
    env -u IMP72SYNTAX -u IMP72FFI -u IMP72LIB stage0/imp72stage0 -c -o "$out/${m%%:*}.o" "$endf" "${m#*:}"
done
rm -f "$out/libimp72.a"
rtobjs=
rt=$(sh tools/llvm.sh builtins)             # the 32 bit targets: the routines for the division of 64 bits, from compiler-rt, are part of the library (no libgcc)
if [ -n "$rt" ]; then
    rm -rf "$out/rt"; mkdir -p "$out/rt"
    case $rt in /*) rtabs=$rt ;; *) rtabs=$root/$rt ;; esac
    ( cd "$out/rt" && ar x "$rtabs" )
    rtobjs=$(ls "$out"/rt/*.o)
fi
ar rcs "$out/libimp72.a" "$out/imp72rt_imp.o" "$out/imp72sys.o" "$out/sysgen.o" "$out/impvars.o" "$out/impvarsx.o" "$out/imp72main.o" "$out/imphost.o" $rtobjs
