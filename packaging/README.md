# Packaging

## Which machines

Any 64 bit machine: the language's word is 64 bits (so i386, armhf, armel and
the other 32 bit ports are out; their `long` is not a word). The suite
(`make check`, 23 checks) passes in Alpine edge containers, native or under QEMU
user mode (`tools/ci-arch.sh ARCH STAGE`, stages build, check and link), on:

| Alpine | Debian | tested here |
|---|---|---|
| x86_64 | amd64 | native and in a musl container |
| aarch64 | arm64 | musl container, QEMU |
| ppc64le | ppc64el | musl container, QEMU |
| riscv64 | riscv64 | musl container, QEMU |
| s390x | s390x | musl container, QEMU |
| loongarch64 | loong64 | not run: no emulator for it in CI |

So both distributions list those architectures. The code generator is LLVM's, in
the compiler's process, so a machine needs the LLVM libraries and a target in them. Most of
these runs were made before LLVM was the only back end (the others used the C back end, which is
to go); the whole suite on s390x with LLVM is the one rerun that is recorded. mips64el has
not been tried (no container for it). Only the amd64 build of the Debian package
has been made (not run through lintian); the Alpine recipe was built with `abuild` (x86_64, edge, from a
`git archive` of the tree standing in for the release tarball), its tests passed, and the package
compiled and ran a program once installed; the container runs build the programs from the repository, not from the packages.

## Debian: `debian/`

A native source package (3.0 native), using debhelper (compat 13) with `dh` and a few overrides in `debian/rules`:

    dpkg-buildpackage -us -uc          # source and binary, with the tests
    DEB_BUILD_OPTIONS=nocheck dpkg-buildpackage -us -uc -b

The source package leaves out build products, git data, and the PDP-10 binaries
of the original compiler.

## Alpine: `packaging/alpine/APKBUILD`

For `aports` (`community/imp72`). The `sha512sums` of the release tarball must be
filled in with `abuild checksum` once the tag `v0.1.0` exists.
