#!/bin/sh
# mklib.sh: build build/lib/libimp.a, the runtime library of the programs IMP72 compiles.  The runtime (PRINT, READ, FINI, CALLI, BLT, IFIX, the shifts, the byte
# pointers) is IMP, lib/imprt.imp and lib/impio.imp, compiled by the seed (stage0/impstage0) into one object; lib/impsys.imp, impvars.imp, imphost.imp and the generated sysgen.imp have what the system says in its headers, and
# lib/impmain.imp is the program's main (in a member of its own, so that a program with a main of its own does not link it).  Run from the top of the tree after `make` has built stage0.
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
out=${LIBOUT:-build/lib}
mkdir -p "$out"
export IMPCHARPTR=1                       # a byte pointer in a variable points at characters: no calls of the routines this library is made of
endf=$(sh tools/endian.sh)
# one source: the files are two modules of one program (a program ends at its first %%)
{ sed '/^%%$/d' lib/imprt.imp; sed '1d' lib/impio.imp; } > "$out/imprt_all.imp"
env -u IMPSYNTAX -u IMPFFI -u IMPLIB stage0/impstage0 -c -o "$out/imprt_imp.o" -C "$endf" "$out/imprt_all.imp"
# the system's own constants (tools/sysconst.sh), and what only a system call can say, in IMP too: no C compiler is needed for any of the library
bd=$(sh tools/llvm.sh bindir)                          # where lld is as a program, if it is not in the PATH
sysos=${SYSOS:-$(uname -s | tr 'A-Z' 'a-z')}
sh tools/sysconst.sh "$sysos" "$SYSARCH" "$bd" > "$out/sysgen.imp"
case $sysos in darwin) vfmt=macho ;; *) vfmt=elf ;; esac
for m in "impsys:lib/impsys.imp" "sysgen:$out/sysgen.imp" "impvars:lib/impvars.imp" "impvarsx:lib/impvarsx-$vfmt.imp" "imphost:lib/imphost.imp" "imphostx:lib/imphostx-$vfmt.imp" "impmain:lib/impmain.imp"; do
    x=; case ${m%%:*} in sysgen) x='-C lib/ffi.imc' ;; esac                 # (sysgen declares syscall: tools/sysconst.sh)
    env -u IMPSYNTAX -u IMPFFI -u IMPLIB stage0/impstage0 -c -o "$out/${m%%:*}.o" $x -C "$endf" "${m#*:}"
done
rm -f "$out/libimp.a"
rtobjs=
rt=$(sh tools/llvm.sh builtins)             # the 32 bit targets: the routines for the division of 64 bits, from compiler-rt, are part of the library (no libgcc)
if [ -n "$rt" ]; then
    rm -rf "$out/rt"; mkdir -p "$out/rt"
    case $rt in /*) rtabs=$rt ;; *) rtabs=$root/$rt ;; esac
    ( cd "$out/rt" && ar x "$rtabs" )
    rtobjs=$(ls "$out"/rt/*.o)
fi
ar rcs "$out/libimp.a" "$out/imprt_imp.o" "$out/impsys.o" "$out/sysgen.o" "$out/impvars.o" "$out/impvarsx.o" "$out/impmain.o" "$out/imphost.o" "$out/imphostx.o" $rtobjs
