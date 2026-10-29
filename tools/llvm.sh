#!/bin/sh
# llvm.sh cflags|libs: the flags for the LLVM C API (stage0/llvmorc.c, tools/ir2ll.c), which the compilers link so that the LLVM back end is in the process: no ir2ll,
# no clang.  Nothing is printed where the LLVM libraries are not installed (the compiler is then built without the LLVM back end).  WITH_LLVM=no leaves them out;
# LLVM_CONFIG names llvm-config.
[ "${WITH_LLVM:-yes}" = no ] && exit 0
lc=${LLVM_CONFIG:-$(command -v llvm-config 2>/dev/null || ls /usr/lib/llvm-*/bin/llvm-config /usr/lib/llvm*/bin/llvm-config 2>/dev/null | sort -V | tail -1)}
[ -n "$lc" ] && [ -x "$lc" ] && [ -f "$($lc --includedir)/llvm-c/LLJIT.h" ] || exit 0
# lld as a library (liblld-NN-dev, lld-dev): the linker of the compilers, in the process (stage0/impld.cpp).  Its headers are in the LLVM include directory or
# /usr/include, its libraries (static or shared) in the LLVM library directory or /usr/lib
lldinc=; lldlib=; lldstatic=
for d in "$($lc --includedir)" /usr/include; do [ -f "$d/lld/Common/Driver.h" ] && { lldinc=$d; break; }; done
for d in "$($lc --libdir)" /usr/lib; do
    for e in a so; do [ -f "$d/liblldELF.$e" ] && [ -f "$d/liblldCommon.$e" ] && { lldlib=$d; [ $e = a ] && lldstatic=1; break 2; }; done
done
haslld() { [ -n "$lldinc" ] && [ -n "$lldlib" ]; }
case $1 in
builtins)
    # compiler-rt's builtins archive (Apache-2.0 with the LLVM exception) for the 32 bit targets, which call __divdi3 and its kin for the division of 64 bits: mklib.sh
    # puts its members in libimp72.a, so that nothing of libgcc is needed
    case $(cc -dumpmachine 2>/dev/null) in i?86-*) names="i386" ;; arm*) names="armhf arm" ;; *) exit 0 ;; esac
    for d in "$($lc --libdir)"/clang/*/lib; do
        for n in $names; do
            for f in "$d"/*/libclang_rt.builtins-$n.a "$d"/libclang_rt.builtins-$n.a; do [ -f "$f" ] && { echo "$f"; exit 0; }; done
        done
    done
    exit 0 ;;
cflags)
    f="-DHAVE_LLVM $($lc --cflags | sed 's/-std=[^ ]*//g; s/-W[^ ]*//g; s/-pedantic//g; s/-f[^ ]*//g; s/-O[0-9]//g; s/-g[ 0-9a-z]*//g')"
    grep -q LLVMOrcCreateNewThreadSafeContextFromLLVMContext "$($lc --includedir)/llvm-c/Orc.h" && f="$f -DHAVE_ORC_FROM_CONTEXT"
    haslld && f="$f -DHAVE_LLD"
    haslld && [ "$lldinc" != /usr/include ] && f="$f -I$lldinc"
    echo $f ;;
libs)
    l=; haslld && { l="-L$lldlib -llldELF -llldCommon -lstdc++"; [ -n "$lldstatic" ] && l="$l -lz -lzstd"; [ "$lldlib" != /usr/lib ] && l="$l -Wl,-rpath,$lldlib"; }
    echo "$l $($lc --link-shared --libs 2>/dev/null || $lc --libs) -Wl,-rpath,$($lc --libdir) -L$($lc --libdir) -rdynamic" ;;
esac
