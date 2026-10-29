#!/bin/sh
# ci-host.sh NAME: what the host of a CI run is, as a GitHub notice (the log of the step cannot be read afterwards): the machine, the CPU as the kernel names it (model name, or
# isa and uarch on riscv), and the triple, CPU and features LLVM reports for it (IMP72LLVMINFO, stage0/llvmorc.c), which are what the code is generated for.  Never fails.
# Run from the top of the tree, after the build.
name=$1
w=${CI_IMP72:-build/pccir2/imp72stage-pccir}
{
    uname -srm
    [ -r /proc/cpuinfo ] && grep -m4 -i '^model name\|^isa\|^uarch\|^mmu\|^cpu\b\|^machine' /proc/cpuinfo | tr '\t' ' '
    [ "$(uname)" = Darwin ] && sysctl -n machdep.cpu.brand_string hw.model 2>/dev/null
    [ "$(uname)" = OpenBSD ] || [ "$(uname)" = FreeBSD ] || [ "$(uname)" = NetBSD ] && sysctl -n hw.model 2>/dev/null
    if [ -x "$w" ] && [ -f tests/native/hello.imp ]; then
        IMP72SYNTAX=$PWD/src/pccir/syntax64.imc IMP72FFI=$PWD/lib/ffi.imc IMP72LIB=$PWD/build/lib/libimp72.a IMP72LLVMINFO=1 \
            "$w" --run tests/native/hello.imp 2>&1 >/dev/null | grep '^imp72-host:'
    else
        echo "imp72-host: no compiler to ask ($w)"
    fi
} 2>&1 | tr '\n%' '|_' | cut -c1-3000 | { cat; echo; } | { read -r line; printf '::notice::%s host: %s\n' "$name" "$line"; }
exit 0
