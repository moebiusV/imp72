#!/bin/sh
# mkrt.sh [TRIPLE [OUTDIR]]: build the runtime library libimp72.a with imp72 itself (build/llvm-ir2/imp72stage-llvm-ir), for this machine or, with a target triple, for another system:
#   sh tools/mkrt.sh aarch64-linux-gnu        build/rt-aarch64-linux-gnu/libimp72.a
# No C compiler is used: the library is IMP (lib/*.imp, and sysgen.imp from tools/sysconst.sh), and imp72 writes the objects of any target.  It is the file to give a cross link
# (imp72 --target=TRIPLE --sysroot=DIR --runtime=build/rt-TRIPLE/libimp72.a prog.imp).  tools/mklib.sh makes the library of this machine with the seed, which the compilers are linked
# with; this one is made by the compiler, so it needs it built first.  A system whose C library the table of tools/sysconst.sh does not know the shape of (the size of a FILE of NetBSD)
# is refused with its message: build the library on that system.
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
triple=$1
w=${MKRT_IMP72:-$root/build/llvm-ir2/imp72stage-llvm-ir}
[ -x "$w" ] || { echo "mkrt: no $w: run make first" >&2; exit 1; }
if [ -n "$triple" ]; then
    arch=${triple%%-*}
    case $triple in
    *darwin*|*apple*) os=darwin ;;
    *cygwin*|*windows-cygnus*) os=cygwin ;;
    *freebsd*) os=freebsd ;;
    *netbsd*) os=netbsd ;;
    *openbsd*) os=openbsd ;;
    *linux*) os=linux ;;
    *) echo "mkrt: no system in the triple $triple (linux, freebsd, netbsd, openbsd, darwin, cygwin)" >&2; exit 1 ;;
    esac
    tgt="--target=$triple"
    out=${2:-build/rt-$triple}
else
    os=$(uname -s | tr 'A-Z' 'a-z')
    arch=
    tgt=
    out=${2:-build/rt-host}
fi
# the byte order and the word size of the target, from the object file the compiler makes for it (ELF: class and data; Mach-O: the magic), not from a list of architectures
mkdir -p "$out"
export IMP72SYNTAX=$root/src/llvm-ir/syntax64.imc IMP72FFI=$root/lib/ffi.imc
printf 'FINI(0)\n%%%%\n' > "$out/probe.imp"
$w -c $tgt -o "$out/probe.o" "$out/probe.imp" > "$out/probe.log" 2>&1 || { echo "mkrt: the compiler cannot make an object for ${triple:-this machine}:" >&2; head -c 600 "$out/probe.log" >&2; exit 1; }
hd=$(od -An -tx1 -N8 "$out/probe.o" | tr -d ' \n')
case $hd in
7f454c46*) case $(echo $hd | cut -c9-10) in 01) bits=32 ;; *) bits=64 ;; esac
           case $(echo $hd | cut -c11-12) in 02) endf=src/impend-be.imc ;; *) endf=src/impend-le.imc ;; esac ;;
6486*) bits=64; endf=src/impend-le.imc ;;          # COFF (Windows, Cygwin): the machine in the first two bytes, x86-64 0x8664, i386 0x14c, aarch64 0xaa64, arm 0x1c4
64aa*) bits=64; endf=src/impend-le.imc ;;
4c01*|c401*) bits=32; endf=src/impend-le.imc ;;
cffaedfe*) bits=64; endf=src/impend-le.imc ;;
cefaedfe*) bits=32; endf=src/impend-le.imc ;;
feedfacf*) bits=64; endf=src/impend-be.imc ;;
feedface*) bits=32; endf=src/impend-be.imc ;;
*) echo "mkrt: the object for ${triple:-this machine} is neither ELF, Mach-O nor COFF" >&2; exit 1 ;;
esac
export SYSBITS=$bits
case $endf in *-be.imc) export SYSBE=1 ;; *) export SYSBE=0 ;; esac
export IMP72CHARPTR=1                       # the library is compiled as it always was: a byte pointer in a variable points at characters, a shift is in line (see src/llvm-ir/demit.imp CHARPTR)
unset IMP72LIB
bd=$(sh tools/llvm.sh bindir)
{ sed '/^%%$/d' lib/imp72rt.imp; sed '1d' lib/imp72io.imp; } > "$out/imp72rt_all.imp"
sh tools/sysconst.sh "$os" "$arch" "$bd" > "$out/sysgen.imp"
case $os in darwin) vfmt=macho ;; *) vfmt=elf ;; esac
objs=
for m in "imp72rt_imp:$out/imp72rt_all.imp" "imp72sys:lib/imp72sys.imp" "sysgen:$out/sysgen.imp" "impvars:lib/impvars.imp" "impvarsx:lib/impvarsx-$vfmt.imp" "imphost:lib/imphost.imp" "imphostx:lib/imphostx-$vfmt.imp" "imp72main:lib/imp72main.imp"; do
    $w -c $tgt -o "$out/${m%%:*}.o" $endf "${m#*:}" > "$out/${m%%:*}.log" 2>&1 || { echo "mkrt: ${m#*:} did not compile:" >&2; head -c 600 "$out/${m%%:*}.log" >&2; exit 1; }
    objs="$objs $out/${m%%:*}.o"
done
# the 32 bit machines call __divdi3 and its kin for the division of 64 bits: compiler-rt's members go in the library, as in tools/mklib.sh (the target's, from this machine's LLVM)
rt=$(sh tools/llvm.sh builtins $triple)
if [ "$bits" = 32 ] && [ -z "$rt" ]; then       # (a link for the target takes libgcc.a from its sysroot where that has it: tools/llvm.sh builtins knows the builtins of i386 and arm)
    echo "mkrt: no compiler-rt builtins for ${triple:-this machine} in the LLVM here: the library has no __divdi3 and its kin, and a program linked with it needs them from elsewhere (the libgcc.a of the sysroot, when the link has one)" >&2
fi
if [ -n "$rt" ]; then
    rm -rf "$out/rt"; mkdir -p "$out/rt"
    case $rt in /*) rtabs=$rt ;; *) rtabs=$root/$rt ;; esac
    ( cd "$out/rt" && ar x "$rtabs" )
    objs="$objs $(ls "$out"/rt/*.o)"
fi
rm -f "$out/libimp72.a"
ar rcs "$out/libimp72.a" $objs
echo "$out/libimp72.a"
