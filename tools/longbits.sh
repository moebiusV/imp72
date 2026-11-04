#!/bin/sh
# longbits.sh: 32 or 64, the bits of a C long on this machine.  getconf LONG_BIT where the system has it (OpenBSD and NetBSD do not), else from the machine type.
v=$(getconf LONG_BIT 2>/dev/null)
case $v in 32|64) echo $v; exit 0 ;; esac
case $(uname -m) in
x86_64|amd64|aarch64|arm64|ppc64|ppc64le|powerpc64|powerpc64le|riscv64|s390x|loongarch64|mips64|mips64el|sparc64|alpha) echo 64 ;;
*) echo 32 ;;
esac
