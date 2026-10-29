#!/bin/sh
# llvm.sh cflags|libs: the flags for the LLVM C API (stage0/llvmorc.c, tools/ir2ll.c), which the compilers link so that the LLVM back end is in the process: no ir2ll,
# no clang.  Nothing is printed where the LLVM libraries are not installed (the compiler is then built without the LLVM back end).  WITH_LLVM=no leaves them out;
# LLVM_CONFIG names llvm-config.
[ "${WITH_LLVM:-yes}" = no ] && exit 0
lc=${LLVM_CONFIG:-$(command -v llvm-config 2>/dev/null || ls /usr/lib/llvm-*/bin/llvm-config /usr/lib/llvm*/bin/llvm-config 2>/dev/null | sort -V | tail -1)}
[ -n "$lc" ] && [ -x "$lc" ] && [ -f "$($lc --includedir)/llvm-c/LLJIT.h" ] || exit 0
# lld as a library (liblld-NN-dev): the linker of the compilers, in the process (stage0/impld.cpp)
haslld() { [ -f "$($lc --includedir)/lld/Common/Driver.h" ] && [ -f "$($lc --libdir)/liblldELF.a" ] && [ -f "$($lc --libdir)/liblldCommon.a" ]; }
case $1 in
cflags)
    f="-DHAVE_LLVM $($lc --cflags | sed 's/-std=[^ ]*//g; s/-W[^ ]*//g; s/-pedantic//g; s/-f[^ ]*//g; s/-O[0-9]//g; s/-g[ 0-9a-z]*//g')"
    grep -q LLVMOrcCreateNewThreadSafeContextFromLLVMContext "$($lc --includedir)/llvm-c/Orc.h" && f="$f -DHAVE_ORC_FROM_CONTEXT"
    haslld && f="$f -DHAVE_LLD"
    echo $f ;;
libs)
    l=; haslld && l="-L$($lc --libdir) -llldELF -llldCommon -lstdc++ -lz -lzstd"
    echo "$l $($lc --link-shared --libs 2>/dev/null || $lc --libs) -Wl,-rpath,$($lc --libdir) -L$($lc --libdir) -rdynamic" ;;
esac
