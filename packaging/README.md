# Packaging

## Which machines

Any 64 bit machine: the language's word is 64 bits (so i386, armhf, armel and
the other 32 bit ports are out; their `long` is not a word). The suite
(`make check`, 23 checks) passes in Alpine edge containers, native or under QEMU
user mode (`tools/ci-arch.sh ARCH`), on:

| Alpine | Debian | pcc's code generator | tested here |
|---|---|---|---|
| x86_64 | amd64 | **yes** (cc1 built, both back ends tested) | native and in a musl container |
| aarch64 | arm64 | **yes** (the fork's AAPCS64 back end; both back ends tested) | musl container, QEMU |
| ppc64le | ppc64el | no | musl container, QEMU |
| riscv64 | riscv64 | no (pcc does not configure) | musl container, QEMU |
| s390x | s390x | no | musl container, QEMU |
| loongarch64 | loong64 | no | musl container, QEMU |

So both distributions list those architectures. pcc's code generator (its
pass 2, `cc1`) is built and used on amd64 and aarch64; everywhere
else `imp72stage0` uses the C back end (`tools/ir2c.c`) and the system's C
compiler (`make WITH_PCC=no`, which is what `auto` means off x86_64). When pcc's
port for another machine works, that machine gets `cc1` too
(`docs/porting/pcc-targets.md`). mips64el, which pcc also has a target for, has
not been tried (no container for it). Only the amd64 build of the Debian package
has been made (not run through lintian); the Alpine recipe was built with `abuild` (x86_64, edge, from a
`git archive` of the tree standing in for the release tarball), its tests passed, and the package
compiled and ran a program once installed; the container runs build the programs from the repository, not from the packages.

## Debian: `debian/`

A native source package (3.0 native), using debhelper (compat 13) with `dh` and a few overrides in `debian/rules`:

    dpkg-buildpackage -us -uc          # source and binary, with the tests
    DEB_BUILD_OPTIONS=nocheck dpkg-buildpackage -us -uc -b

The source package contains the pcc source (the `third_party/pcc` submodule
must be checked out when it is made) and leaves out build products, git data,
and the PDP-10 binaries of the original compiler.

## Alpine: `packaging/alpine/APKBUILD`

For `aports` (`community/imp72`). The `sha512sums` of the release tarball must be
filled in with `abuild checksum` once the tag `v0.1.0` exists; the one for pcc is
that of the pinned revision. The pcc source is fetched separately because
GitHub's archive of a tag does not include submodules.
