#!/bin/sh
# ci-arch.sh ARCH STAGE [make args]: one stage (build, check or link: tools/ci-container.sh) of the build and tests in an Alpine edge container of one architecture (arm64,
# riscv64, ppc64le, s390x, amd64, 386, arm/v7 in Docker's names).  Other architectures than the host's need QEMU user mode registered with binfmt (docker/setup-qemu-action in CI,
# tonistiigi/binfmt).  The work (the copy of the tree, the build, the logs) is in CI_WORK, default tmp/ci-ARCH of the tree, and stays there: the next stage, or the same one run
# again, goes on from it.  Run the stages in turn: for s in build check link; do tools/ci-arch.sh riscv64 $s || break; done
arch=$1 stage=$2; shift 2
here=$(cd "$(dirname "$0")" && pwd)
root=$(dirname "$here")
image=${IMAGE:-alpine:edge}
work=${CI_WORK:-$root/tmp/ci-$(echo "$arch" | tr / -)}
mkdir -p "$work"
exec docker run --rm ${CI_CPUSET:+--cpuset-cpus $CI_CPUSET} --platform linux/$arch -v "$root":/src:ro -v "$work":/work -v "$here/ci-container.sh":/in.sh:ro "$image" sh /in.sh "$stage" "$@"
