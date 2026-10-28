#!/bin/sh
# ci-arch.sh ARCH [make args]: build and test the repository in an Alpine edge
# container of one architecture (arm64, riscv64, ppc64le, s390x, amd64, loong64
# in Docker's names).  Other architectures than the host's need QEMU user mode
# registered with binfmt (docker/setup-qemu-action in CI, tonistiigi/binfmt).
# Alpine's image for loongarch64 is alpine-loongarch64:edge (see ci.yml).
arch=$1; shift
here=$(cd "$(dirname "$0")" && pwd)
root=$(dirname "$here")
image=${IMAGE:-alpine:edge}
exec docker run --rm ${CI_CPUSET:+--cpuset-cpus $CI_CPUSET} --platform linux/$arch -v "$root":/src:ro -v "$here/ci-container.sh":/in.sh:ro "$image" sh /in.sh "$@"
