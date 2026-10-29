#!/bin/sh
# ci-container.sh: run inside an Alpine container (the repository at /src,
# read only): build a copy and run the tests.  tools/ci-arch.sh starts it.
set -e
apk add --no-cache build-base binutils clang llvm llvm-dev lld-dev compiler-rt >/dev/null
mkdir /work
cd /src
tar cf - --exclude=./build --exclude=./.git --exclude=./stage0/imp72s0 --exclude=./stage0/imp72stage0 --exclude=./stage0/libstage0.a --exclude='*.o' . | (cd /work && tar xf -)
cd /work
uname -m
fail=0
# each step's own status counts (a pipeline's status is its last command's): the logs are kept and summarised
make "$@" > make.log 2>&1 || fail=1
tail -15 make.log
# LINK_ONLY=1: the build and the test of the link in the process (an emulated architecture is slow)
if [ -z "$LINK_ONLY" ]; then
    make check "$@" > check.log 2>&1 || fail=1
    grep -E "FAIL|DIFFER|skipped|^ok|== |rror" check.log | sort | uniq -c | sort -rn | head -40
fi
echo "== the link in the process"
sh tests/link/run.sh > link.log 2>&1 || fail=1
tail -5 link.log
[ $fail = 0 ] && echo "== all passed" || echo "== FAILED"
exit $fail
