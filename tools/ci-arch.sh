#!/bin/sh
# ci-arch.sh ARCH STAGE [make args]: one stage (build, check or link: tools/ci-container.sh) of the build and tests in an Alpine edge container of one architecture (arm64,
# riscv64, ppc64le, s390x, amd64, 386, arm/v7, loong64 in Docker's names; IMAGE names the image, alpine:edge, or alpine-loongarch64:edge for loong64).  Other architectures than the
# host's need QEMU user mode registered with binfmt (docker/setup-qemu-action in CI, tonistiigi/binfmt).  The work (the copy of the tree, the build, the logs) is in CI_WORK,
# default tmp/ci-ARCH of the tree, and stays there: the next stage, or the same one run again, goes on from it.  Run the stages in turn:
#   for s in build check link; do tools/ci-arch.sh riscv64 $s || break; done
# Whatever happens to the container, this script is left to say so: logs/STAGE.host.log has the host, the emulators registered and the time it took, and a container that was
# stopped without telling (killed for memory, a crash of the emulator, a timeout) is reported here, with the end of its log, as a GitHub annotation.
arch=$1 stage=$2; shift 2
here=$(cd "$(dirname "$0")" && pwd)
root=$(dirname "$here")
image=${IMAGE:-alpine:edge}
work=${CI_WORK:-$root/tmp/ci-$(echo "$arch" | tr / -)}
logs=$work/logs
mkdir -p "$logs"
hostlog=$logs/$stage.host.log
{
    echo "== $(date -u +%FT%TZ) $arch $stage, image $image"
    uname -srm
    docker version --format 'docker {{.Server.Version}}' 2>&1
    echo "binfmt: $(ls /proc/sys/fs/binfmt_misc 2>/dev/null | grep -i qemu | tr '\n' ' ')"
    df -h "$work" | tail -1
    free -m 2>/dev/null | sed -n 2p
} >> "$hostlog" 2>&1
start=$(date +%s)
docker run --rm ${CI_CPUSET:+--cpuset-cpus $CI_CPUSET} --platform linux/$arch -v "$root":/src:ro -v "$work":/work -v "$here/ci-container.sh":/in.sh:ro "$image" sh /in.sh "$stage" "$@"
rc=$?
echo "== exit $rc after $(( $(date +%s) - start )) seconds" >> "$hostlog"
if [ $rc -ne 0 ] && [ "$(cat "$logs/$stage.status" 2>/dev/null)" != failed ]; then
    # no word from the container: it was stopped (137 killed, 139 segmentation fault, 124 timeout, 125 docker itself)
    {
        echo "== the container of $stage stopped without saying why (exit $rc)"
        tail -n 12 "$logs/$stage.log" 2>/dev/null
        echo "-- last heartbeat"
        tail -n 8 "$logs/$stage.heartbeat" 2>/dev/null
    } 2>&1 | tr '\n%' ' _' | cut -c1-1800 | { cat; echo; } | { read -r m; printf '::error::%s %s: stopped, exit %s: %s\n' "$arch" "$stage" "$rc" "$m"; }
fi
exit $rc
