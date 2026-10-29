#!/bin/sh
# ci-bsd.sh NAME: the build and the tests on a BSD (CI runs it in a VM, whose log cannot be read afterwards).  When something fails, the lines with an error, the end of the
# log and what the build found of LLVM and lld go out as GitHub annotations.  The log is tmp/ci-NAME.log.
name=$1
mkdir -p tmp
log=tmp/ci-$name.log
if gmake > "$log" 2>&1 && gmake check >> "$log" 2>&1; then
    echo "$name: ok"
    exit 0
fi
ann() { printf '::error::%s %s: %s\n' "$name" "$1" "$(tr '\n%' ' _' | cut -c1-1800)"; }
grep -n -i 'error\|undefined\|not found\|cannot\|FAIL' "$log" | head -25 | ann "errors"
tail -n 25 "$log" | ann "tail"
{ uname -srm; sh tools/llvm.sh cflags | head -c 400; echo; sh tools/llvm.sh libs | head -c 400; echo; command -v llvm-config llvm-config-19 cc c++ gmake; cc --version | head -1; } 2>&1 | ann "toolchain"
exit 2
