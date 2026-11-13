#!/bin/sh
# win-imp.sh [OUTDIR]: cross-build the compiler written in IMP (the modules of tools/llvm-ir.sh, built as tools/direct.sh builds them) for Windows, x86_64 with the UCRT, from Linux:
# build/win-imp/impstage-llvm-ir.exe, which runs under Wine (tools/wine-setup.sh) and on Windows 10.  The IMP modules are compiled to COFF by the Linux compiler (build/llvm-ir2,
# --target=x86_64-w64-mingw32-ucrt), their symbols renamed with tools/imprename (COFF too), and linked with the library made for the target (tools/mkrt.sh: lib/imphost-win.iml is its host
# layer), the C++ shims and the portable string functions of the seed, and the LLVM and lld libraries of MSYS2's ucrt64 packages (tools/win-seed.sh, tools/msys2-fetch.sh), by lld.  Run from the top
# of the tree after make; run tools/win-seed.sh first or let this fetch what it needs.
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
. tools/limits.sh
out=${1:-build/win-imp}
W=${WROOT:-$root/tmp/msys/w}/ucrt64
imp=${WIN_IMP_COMPILER:-build/llvm-ir2/impstage-llvm-ir}
imprename=${WIN_IMPRENAME:-build/llvm-ir2/imprename}
[ -x "$imp" ] && [ -x "$imprename" ] || { echo "win-imp: no $imp or $imprename: run make first" >&2; exit 1; }
cb=$(ls -d /usr/lib/llvm-*/bin 2>/dev/null | sort -V | tail -1); cb=${CLANG_BIN:-$cb}
tr=x86_64-w64-mingw32-ucrt
if ! { [ -f "$W/lib/crt2.o" ] && [ -d "$W/include/llvm-c" ] && [ -f "$W/lib/liblldCOFF.a" ]; }; then
    sh tools/msys2-fetch.sh "${WROOT:-$root/tmp/msys/w}" headers crt winpthreads libwinpthread gcc llvm lld llvm-libs zlib zstd libxml2 libiconv libffi || { echo "win-imp: cannot get the MSYS2 tree" >&2; exit 1; }
fi
mkdir -p "$out"
endf=src/impend-le.imc                         # (x86-64 is little endian)
export IMPENDIAN=$endf
export IMPSYNTAX=$root/src/syntax.imc IMPFFI=$root/lib/ffi.imc
unset IMPLIB
# the runtime library for the target, and the compiler's own C: the two C++ shims and the portable string functions, for Windows
sh tools/mkrt.sh $tr "$out/rt" > "$out/mkrt.log" 2>&1 || { tail -5 "$out/mkrt.log" >&2; echo "win-imp: libimp.a for $tr" >&2; exit 1; }
cdef="--target=x86_64-w64-windows-gnu -nostdinc -O1 -g0 -DHAVE_LLVM -DHAVE_STRL -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS"
cres=$($cb/clang --print-resource-dir)/include
cinc="-isystem $cres -isystem $W/include"
xinc="-isystem $W/include/c++/16.2.0 -isystem $W/include/c++/16.2.0/x86_64-w64-mingw32 -isystem $W/include/c++/16.2.0/backward $cinc"
llvmdef="-DHAVE_LLD -DHAVE_LLD_ELF -DHAVE_LLD_MACHO -DHAVE_LLD_COFF -DHAVE_LLD_MINGW"
cobjs=
for f in strl strlcat strlcpy; do $cb/clang $cdef $cinc -std=gnu99 -c stage0/$f.c -o "$out/$f.o"; cobjs="$cobjs $out/$f.o"; done
for f in lvtriple impld; do $cb/clang++ $cdef $xinc $llvmdef -nostdinc++ -std=c++17 -fno-rtti -c stage0/$f.cpp -o "$out/$f.o"; cobjs="$cobjs $out/$f.o"; done
# llvmgen: what the LLVM and lld decide, from the headers of the MSYS2 tree (llvm.sh asks an llvm-config for them)
cat > "$out/llvm-config" <<LC
#!/bin/sh
case \$1 in --includedir) echo $W/include ;; --libdir) echo $W/lib ;; --bindir) echo $W/bin ;; *) exit 1 ;; esac
LC
chmod +x "$out/llvm-config"
LLVM_CONFIG="$root/$out/llvm-config" sh tools/llvm.sh imp > "$out/llvmgen.imc"
# the modules, as tools/direct.sh makes them
modules="impsem code code2 code3 demit ix"
srcmodules="free debug stack dir graph parse ercor entree cotree dosem rsyn lex image impm ir2ll ir2llx driver linktab llvm llvmgen"
for m in $modules; do
    sh tools/capped.sh $out/$m.log env IMPMODULE=$m $imp -c --target=$tr -o $out/$m.imp.o -C $endf -C src/impsyn.imc -C src/irsyn.imc src/$m.imc \
        || { dd bs=100000 count=1 if=$out/$m.log 2>/dev/null >&2; echo "win-imp: $m failed" >&2; exit 1; }
done
for m in $srcmodules; do
    x=; case $m in free) x='-C lib/ffi.imc' ;; esac
    f=src/$m.imc; case $m in llvmgen) f=$out/llvmgen.imc ;; esac
    sh tools/capped.sh $out/$m.log env IMPMODULE=$m $imp -c --target=$tr -o $out/$m.imp.o $x -C $endf -C src/impsyn.imc $f \
        || { dd bs=100000 count=1 if=$out/$m.log 2>/dev/null >&2; echo "win-imp: $m failed" >&2; exit 1; }
done
syms=
for m in $modules $srcmodules; do syms="$syms $($imprename -d $out/$m.imp.o)"; done
syms=$(printf '%s\n' $syms | sort -u)
for s in $syms; do
    l=$(printf '%s' "$s" | tr 'A-Z' 'a-z')
    if grep -qx -- "$l" src/shared.txt && [ "$s" != "$l" ]; then echo "$s $l"
    elif grep -qx -- "$s" src/shared.txt; then :
    else echo "$s impl_$s"; fi
done > $out/rename.map
objs=
for m in $modules $srcmodules; do $imprename -f $out/rename.map $out/$m.imp.o $out/$m.imp2.o; objs="$objs $out/$m.imp2.o"; done
gccd=$(ls -d $W/lib/gcc/x86_64-w64-mingw32/* | head -1)
llvmlibs=$(ls $W/lib/libLLVM*.a | grep -v '\.dll\.a$' | tr '\n' ' ')
lldlibs="$W/lib/liblldCOFF.a $W/lib/liblldELF.a $W/lib/liblldMachO.a $W/lib/liblldMinGW.a $W/lib/liblldWasm.a $W/lib/liblldCommon.a"
$cb/ld.lld -m i386pep -o "$out/impstage-llvm-ir.exe" $W/lib/crt2.o $objs $cobjs "$out/rt/libimp.a" $lldlibs $llvmlibs -L $W/lib -L $gccd \
    -Bstatic -lstdc++ -lwinpthread -Bdynamic -lxml2 -liconv -lz -lzstd -lmingw32 -lgcc -lgcc_eh -lmoldname -lmingwex -lucrt \
    -lkernel32 -luser32 -lbcrypt -lpsapi -lshell32 -lole32 -luuid -ladvapi32 -lws2_32 -lntdll -lversion > "$out/link.log" 2>&1 || { grep -v aligncomm "$out/link.log" | head -40 >&2; echo "win-imp: the link failed" >&2; exit 1; }
for d in libxml2-16 libz-1 libzstd libiconv-2; do cp "$W/bin/$d.dll" "$out/" 2>/dev/null || echo "win-imp: no $d.dll in $W/bin" >&2; done
ls -la "$out/impstage-llvm-ir.exe" 2>/dev/null | awk '{print $5, $9}'
