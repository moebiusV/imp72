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
case $(uname -s) in Darwin) lldname=lldMachO ;; *) lldname=lldELF ;; esac       # the driver of the system's executable format (stage0/impld.cpp)
for d in "$($lc --includedir)" /usr/include /opt/homebrew/include /usr/local/include; do [ -f "$d/lld/Common/Driver.h" ] && { lldinc=$d; break; }; done
for d in "$($lc --libdir)" /usr/lib /opt/homebrew/lib /usr/local/lib; do
    for e in a so dylib; do [ -f "$d/lib$lldname.$e" ] && [ -f "$d/liblldCommon.$e" ] && { lldlib=$d; [ $e = a ] && lldstatic=1; break 2; }; done
done
haslld() { [ "${WITH_LLD:-yes}" != no ] && [ -n "$lldinc" ] && [ -n "$lldlib" ]; }          # WITH_LLD=no builds without it: the compiler then runs ld.lld
# the C++ library the shim and LLVM need: libc++ on FreeBSD, OpenBSD and macOS (and libc++abi on OpenBSD, where libLLVM does not record that it needs them), else libstdc++
case $(uname -s) in
OpenBSD) cxxlib="-lc++ -lc++abi -lm -lpthread" ;;        # libLLVM there does not record that it needs libm and threads either
FreeBSD) cxxlib="-lc++ -lpthread" ;;
Darwin) cxxlib="-lc++" ;;
*) cxxlib="-lstdc++" ;;
esac
case $1 in
bindir)
    # LLVM's own bin directory: lld is there as a program (ld.lld, ld64.lld) where it is not in the PATH; the compiler is built knowing it (lib/imphost.c, imprunld)
    $lc --bindir
    exit 0 ;;
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
    # the run path of the LLVM libraries, unless LLVM_NO_RPATH is set (a distribution package, whose libraries are where the dynamic linker looks: dpkg-shlibdeps would find
    # the -dev package's symlink through the run path, which carries no information about the shared library)
    rp="-Wl,-rpath,$($lc --libdir)"; [ -n "$LLVM_NO_RPATH" ] && rp=
    # the packages of the BSDs (zstd, for a static lld) are in /usr/local/lib, which their linker does not look in; OpenBSD's libLLVM needs libc++ and libm and does not say so
    l=
    case $(uname -s) in
    OpenBSD) l="-L/usr/local/lib $cxxlib" ;;
    FreeBSD) l="-L/usr/local/lib" ;;
    esac
    if haslld; then
        l="$l -L$lldlib -l$lldname -llldCommon"
        [ "$(uname -s)" != OpenBSD ] && l="$l $cxxlib"
        [ -n "$lldstatic" ] && l="$l -lz -lzstd"
        [ -z "$lldstatic" ] && [ "$lldlib" != /usr/lib ] && [ -z "$LLVM_NO_RPATH" ] && l="$l -Wl,-rpath,$lldlib"
    fi
    [ "$(uname -s)" = Linux ] && l="$l -ldl"                                  # dladdr, for older glibc (src/driver.imp finds the C library with it)
    echo "$l $($lc --link-shared --libs 2>/dev/null || $lc --libs) $rp -L$($lc --libdir) -rdynamic" ;;
esac
