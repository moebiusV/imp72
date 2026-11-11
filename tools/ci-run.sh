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
# a tool of the manual test that crashes: where and why (ktrace and kdump say the fault and its address, the debugger the routine; both only where the system has them)
grep -q 'FAILED: tools/runoff' "$log" && {
    t=build/tools/runoff2mdoc
    command -v ktrace >/dev/null 2>&1 && { ktrace -f build/runoff.ktr $t docs/manual/imp.rno >/dev/null 2>&1; kdump -f build/runoff.ktr 2>&1 | grep -i 'psig\|SIGSEGV\|fault\|addr' | tail -n 6 | ann "runoff-kdump"; }
    command -v egdb >/dev/null 2>&1 && egdb -batch -ex 'run docs/manual/imp.rno >/dev/null' -ex bt -ex 'info registers rip rsp' -ex 'x/3i $rip' --args $t 2>&1 | tail -n 14 | ann "runoff-gdb"
}
{ uname -srm; sh tools/llvm.sh cflags | cut -c1-400; sh tools/llvm.sh libs | cut -c1-400; command -v llvm-config cc c++ gmake; ls /usr/local/bin /usr/local/llvm*/bin 2>/dev/null | grep -i 'llvm-config\\|lld' | tr '\\n' ' '; ls /usr/local/lib /usr/local/llvm*/lib 2>/dev/null | grep -i 'liblld' | tr '\\n' ' '; cc --version | head -1; } 2>&1 | ann "toolchain"
grep -q 'FAILED: image' "$log" && { IMPCOREDEBUG=1 sh tests/image/run.sh 2>&1 | grep -i 'core:\|FAILED\|imp:' | head -12 | ann "image-debug"; }
{ ls /usr/lib /usr/include 2>/dev/null | grep -i 'lld\|libLLVM'; pkg_info -Q lld 2>/dev/null | head -8; pkg_info -Q llvm 2>/dev/null | head -8; pkg_info 2>/dev/null | grep -i 'llvm\|lld'; } 2>&1 | ann "packages"
# the logs of the stages of the compilers' build (tools/llvm-ir.sh, tools/direct.sh): what the step that failed said, or that it said nothing
for f in build/llvm-ir1.log build/llvm-ir2.log build/llvm-ir1/*.log build/llvm-ir2/*.log; do
    [ -s "$f" ] && { dd bs=500 count=1 if="$f" 2>/dev/null; printf ' ... '; tail -c 500 "$f"; } | ann "$f"
done
ls -la build/llvm-ir1 build/llvm-ir2 2>&1 | tail -n 12 | ann "ls"
exit 2
