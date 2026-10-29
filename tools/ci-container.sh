#!/bin/sh
# ci-container.sh STAGE [make args]: run inside an Alpine container, one stage of the build and the tests of a copy of the repository (at /src, read only).  The copy, the build
# and the logs are in /work, which is kept between the stages and between the runs (tools/ci-arch.sh mounts it from the host, the workflow caches it):
#   build  the copy and make          check  make check          link  the test of the link in the process
# A stage that finished leaves /work/.done-STAGE and is skipped when it is asked for again; one that failed leaves none, and runs again where make left off.
# The log of each stage is /work/logs/STAGE.log.  tools/ci-arch.sh starts it.
stage=$1; shift
w=/work
mkdir -p $w/logs
log=$w/logs/$stage.log
if [ -f $w/.done-$stage ]; then
    echo "== $stage: finished by an earlier run, skipped"
    exit 0
fi
uname -m
# the failure of a stage, as GitHub annotations (the log of the step is not readable afterwards)
ann() { printf '::error::%s %s %s: %s\n' "$(uname -m)" "$stage" "$1" "$(tr '\n%' ' _' | cut -c1-1800)"; }
fail() {
    echo "== $stage FAILED"
    grep -n -i 'error\|undefined\|not found\|cannot\|FAIL' $log 2>/dev/null | grep -v 'warning' | head -25 | ann "errors"
    tail -n 25 $log 2>/dev/null | ann "tail"
    exit 1
}
{ apk add --no-cache build-base binutils clang llvm llvm-dev lld-dev compiler-rt >/dev/null; } > $log 2>&1 || fail
if [ ! -f $w/.done-src ]; then
    mkdir -p $w/tree
    (cd /src && tar cf - --exclude=./build --exclude=./.git --exclude=./tmp --exclude=./work --exclude=./stage0/imp72s0 --exclude=./stage0/imp72stage0 --exclude=./stage0/libstage0.a --exclude='*.o' .) | (cd $w/tree && tar xf -) || fail
    touch $w/.done-src
fi
cd $w/tree || fail
case $stage in
build) make "$@" >> $log 2>&1 || fail
       sh tools/ci-host.sh "$(uname -m)" ;;                      # the host, in the annotations of the run
check) make check "$@" >> $log 2>&1 || fail ;;
link) sh tests/link/run.sh >> $log 2>&1 || fail ;;
*) echo "ci-container: no stage $stage" >&2; exit 2 ;;
esac
tail -n 15 $log
grep -E "FAIL|DIFFER|skipped|^ok|== " $log | sort | uniq -c | sort -rn | head -40
touch $w/.done-$stage
echo "== $stage passed"
