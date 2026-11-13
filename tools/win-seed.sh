#!/bin/sh
# win-seed.sh [OUTDIR]: cross-build the seed (stage0) for Windows, x86_64 with the UCRT, from Linux: build/win-seed/impstage0.exe, which runs under Wine (tools/wine-setup.sh)
# and on Windows 10.  The C and C++ are compiled by clang for x86_64-w64-windows-gnu against the packages of MSYS2's ucrt64 repository, unpacked under WROOT/ucrt64 (headers, crt,
# gcc for libstdc++, llvm and lld with their static libraries, zlib, zstd, libxml2, libiconv), and linked by lld; nothing is installed.  WROOT defaults to tmp/msys/w (fetched there by tools/msys2-fetch.sh if it is not).  The result is the seed with LLVM and lld in the process, as on any
# other host: it makes objects and executables for Windows (stage0/imphost-win.c is its host layer).  Run from the top of the tree.
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
out=${1:-build/win-seed}
W=${WROOT:-$root/tmp/msys/w}/ucrt64
cb=${CLANG_BIN:-$(sh tools/llvm.sh bindir 2>/dev/null)}; [ -n "$cb" ] || cb=$(ls -d /usr/lib/llvm-*/bin | sort -V | tail -1)
tr=x86_64-w64-windows-gnu
cres=$($cb/clang --print-resource-dir)/include
if ! { [ -f "$W/lib/crt2.o" ] && [ -d "$W/include/llvm-c" ] && [ -f "$W/lib/liblldCOFF.a" ]; }; then          # fetch what is not there: about 1 GB unpacked
    sh tools/msys2-fetch.sh "${WROOT:-$root/tmp/msys/w}" headers crt winpthreads libwinpthread gcc llvm lld llvm-libs zlib zstd libxml2 libiconv libffi \
        || { echo "win-seed: cannot get the MSYS2 ucrt64 tree with LLVM at $W" >&2; exit 1; }
fi
mkdir -p "$out"
cdef="--target=$tr -nostdinc -O1 -g0 -DHAVE_LLVM -DHAVE_STRL -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS"
cinc="-isystem $cres -isystem $W/include"
xinc="-isystem $W/include/c++/16.2.0 -isystem $W/include/c++/16.2.0/x86_64-w64-mingw32 -isystem $W/include/c++/16.2.0/backward $cinc"     # (the C++ headers first: they include_next the C ones)
llvmdef="-DHAVE_LLD -DHAVE_LLD_ELF -DHAVE_LLD_MACHO -DHAVE_LLD_COFF -DHAVE_LLD_MINGW"
objs=
for f in stage0/*.c; do
    b=$(basename "$f" .c)
    case $b in imphost) continue ;; esac
    $cb/clang $cdef $cinc $llvmdef -std=gnu99 -c "$f" -o "$out/$b.o"
    objs="$objs $out/$b.o"
done
$cb/clang $cdef $cinc -std=gnu99 -DIR2LL_LIB -c tools/ir2ll.c -o "$out/ir2ll.o"          # (the IR writer, as the library the seed calls: ir2ll_run)
objs="$objs $out/ir2ll.o"
for f in lvtriple impld; do
    $cb/clang++ $cdef $xinc $llvmdef -nostdinc++ -std=c++17 -fno-rtti -c stage0/$f.cpp -o "$out/$f.o"
    objs="$objs $out/$f.o"
done
gccd=$(ls -d $W/lib/gcc/x86_64-w64-mingw32/* | head -1)
llvmlibs=$(ls $W/lib/libLLVM*.a | grep -v '\.dll\.a$' | tr '\n' ' ')
lldlibs="$W/lib/liblldCOFF.a $W/lib/liblldELF.a $W/lib/liblldMachO.a $W/lib/liblldMinGW.a $W/lib/liblldWasm.a $W/lib/liblldCommon.a"
$cb/ld.lld -m i386pep -o "$out/impstage0.exe" $W/lib/crt2.o $objs $lldlibs $llvmlibs -L $W/lib -L $gccd \
    -Bstatic -lstdc++ -lwinpthread -Bdynamic -lxml2 -liconv -lz -lzstd -lmingw32 -lgcc -lgcc_eh -lmoldname -lmingwex -lucrt \
    -lkernel32 -luser32 -lbcrypt -lpsapi -lshell32 -lole32 -luuid -ladvapi32 -lws2_32 -lntdll -lversion 2>&1 | head -40 || true
# the DLLs LLVM was built against (libxml2, zlib, zstd, iconv: MSYS2 builds them shared), beside the program
for d in libxml2-16 libz-1 libzstd libiconv-2; do cp "$W/bin/$d.dll" "$out/" 2>/dev/null || echo "win-seed: no $d.dll in $W/bin" >&2; done
ls -la "$out/impstage0.exe" 2>/dev/null | awk '{print $5, $9}'
