#!/bin/sh
# ci-container.sh STAGE [make args]: run inside an Alpine container, one stage of the build and the tests of a copy of the repository (at /src, read only).  The copy, the build
# and the logs are in /work, which is kept between the stages and between the runs (tools/ci-arch.sh mounts it from the host, the workflow caches it):
#   build  the copy and make          check  make check          link  the test of the link in the process
# A stage that finished leaves /work/.done-STAGE and is skipped when it is asked for again; one that failed leaves none, and runs again where make left off.
# What it leaves in /work/logs, whatever happens to it: STAGE.log (everything the stage printed, with times), STAGE.status (passed, failed, or died with the exit status when the
# shell itself was stopped), STAGE.heartbeat (the time, the memory and the biggest processes every 30 seconds: the last lines say what a killed or hung stage was doing).
# tools/ci-arch.sh starts it, and reports a container that was killed before it could say so.
stage=$1; shift
w=/work
logs=$w/logs
mkdir -p $logs
chmod a+rwx $logs 2>/dev/null                  # (the host writes its own logs there too, and reads these: the container is root)
log=$logs/$stage.log
status=$logs/$stage.status
hb=$logs/$stage.heartbeat
if [ -f $w/.done-$stage ]; then
    echo "== $stage: finished by an earlier run, skipped"
    exit 0
fi
rm -f $status
: > $hb
hbpid=
trap 'rc=$?; [ -n "$hbpid" ] && kill $hbpid 2>/dev/null; [ -s $status ] || echo "died (exit $rc)" > $status; chmod -R a+rwX $logs 2>/dev/null' EXIT
stamp() { date -u +%H:%M:%S; }
{ echo "== $(stamp) $stage on $(uname -m), $(nproc) cpus"; df -h $w | tail -1; free -m | sed -n 2p; } >> $log 2>&1
( while :; do { echo "-- $(stamp)"; free -m | sed -n 2p; ps -o pid,rss,etime,args 2>/dev/null | sort -k2 -nr | head -5 | cut -c1-150; } >> $hb 2>&1; sleep 30; done ) &
hbpid=$!
uname -m
# the failure of a stage, as GitHub annotations (the log of the step is not readable afterwards)
ann() { printf '::error::%s %s %s: %s\n' "$(uname -m)" "$stage" "$1" "$(tr '\n%' ' _' | cut -c1-1800)"; }
fail() {
    echo "== $(stamp) $stage FAILED" | tee -a $log
    echo failed > $status
    grep -n -i 'error\|undefined\|not found\|cannot\|FAIL' $log 2>/dev/null | grep -v 'warning' | head -25 | ann "errors"
    tail -n 25 $log 2>/dev/null | ann "tail"
    tail -n 12 $hb 2>/dev/null | ann "last heartbeat"
    exit 1
}
{ apk add --no-cache build-base binutils clang llvm llvm-dev lld-dev compiler-rt >/dev/null; } >> $log 2>&1 || fail
if [ ! -f $w/.done-src ]; then
    mkdir -p $w/tree
    (cd /src && tar cf - --exclude=./build --exclude=./.git --exclude=./tmp --exclude=./work --exclude=./stage0/imp72s0 --exclude=./stage0/imp72stage0 --exclude=./stage0/libstage0.a --exclude='*.o' .) | (cd $w/tree && tar xf -) || fail
    touch $w/.done-src
fi
cd $w/tree || fail
echo "== $(stamp) $stage begins" >> $log
case $stage in
build) make "$@" >> $log 2>&1 || fail
       sh tools/ci-host.sh "$(uname -m)" ;;                      # the host, in the annotations of the run
check) make check "$@" >> $log 2>&1 || fail ;;
link) sh tests/link/run.sh >> $log 2>&1 || fail ;;
*) echo "ci-container: no stage $stage" >&2; exit 2 ;;
esac
echo "== $(stamp) $stage ends" >> $log
tail -n 15 $log
grep -E "FAIL|DIFFER|skipped|^ok|== " $log | sort | uniq -c | sort -rn | head -40
echo passed > $status
touch $w/.done-$stage
echo "== $stage passed"
