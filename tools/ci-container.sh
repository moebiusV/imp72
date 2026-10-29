#!/bin/sh
# ci-container.sh: run inside an Alpine container (the repository at /src,
# read only): build a copy and run the tests.  tools/ci-arch.sh starts it.
set -e
apk add --no-cache build-base binutils clang llvm llvm-dev >/dev/null
mkdir /work
cd /src
tar cf - --exclude=./build --exclude=./.git --exclude=./stage0/imp72s0 --exclude=./stage0/imp72stage0 --exclude=./stage0/libstage0.a --exclude='*.o' . | (cd /work && tar xf -)
cd /work
uname -m
make "$@" 2>&1 | tail -3
make check "$@" 2>&1 | grep -E "FAIL|DIFFER|skipped|^ok|== |rror" | sort | uniq -c | sort -rn | head -40
