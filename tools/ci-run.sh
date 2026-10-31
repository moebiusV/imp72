#!/bin/sh
# ci-run.sh NAME: the build and the tests (CI runs it in the VM of a BSD, whose log cannot be read afterwards, and on Linux).  MAKE names make (default gmake).  When something
# fails, the lines with an error, the end of the log and what the build found of LLVM and lld go out as GitHub annotations.  The log is tmp/ci-NAME.log.
name=$1
mkdir -p tmp
log=tmp/ci-$name.log
make=${MAKE:-gmake}
if $make > "$log" 2>&1; then
    sh tools/ci-host.sh "$name"                                 # the host, in the annotations of the run, whether the tests pass or not
    if $make check >> "$log" 2>&1; then
        echo "$name: ok"
        exit 0
    fi
fi
ann() { printf '::error::%s %s: %s\n' "$name" "$1" "$(tr '\n%' ' _' | cut -c1-1800)"; }
annend() { printf '::error::%s %s: %s\n' "$name" "$1" "$(tr '\n%' ' _' | tail -c 1800)"; }       # the END of what comes in (the beginning of a long tail says nothing of why it failed)
grep -n -i 'error\|undefined\|not found\|cannot\|FAIL' "$log" | head -25 | ann "errors"
tail -n 25 "$log" | annend "tail"
{ uname -srm; sh tools/llvm.sh cflags | cut -c1-400; sh tools/llvm.sh libs | cut -c1-400; command -v llvm-config cc c++ gmake; ls /usr/local/bin /usr/local/llvm*/bin 2>/dev/null | grep -i 'llvm-config\\|lld' | tr '\\n' ' '; ls /usr/local/lib /usr/local/llvm*/lib 2>/dev/null | grep -i 'liblld' | tr '\\n' ' '; cc --version | head -1; } 2>&1 | ann "toolchain"
grep -q 'FAILED: image' "$log" && { IMP72COREDEBUG=1 sh tests/image/run.sh 2>&1 | grep -i 'core:\|FAILED\|imp72:' | head -12 | ann "image-debug"; }
{ ls /usr/lib /usr/include 2>/dev/null | grep -i 'lld\|libLLVM'; pkg_info -Q lld 2>/dev/null | head -8; pkg_info -Q llvm 2>/dev/null | head -8; pkg_info 2>/dev/null | grep -i 'llvm\|lld'; } 2>&1 | ann "packages"
exit 2
