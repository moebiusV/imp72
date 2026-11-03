# Packaging

## Which machines

The language's word is 64 bits; on a 32-bit machine the compiler holds it in a
`long long`. The code generator is LLVM's, in the compiler's process, so a
machine needs the LLVM libraries with a target for it, and the C library's
development files (the driver links with the C library's start files and never
runs a C compiler).

The packages list these architectures:

| distribution | architectures |
|---|---|
| Debian (`debian/control`) | amd64, i386, arm64, armhf, ppc64el, riscv64, s390x, loong64 |
| Alpine (`packaging/alpine/APKBUILD`) | x86_64, x86, aarch64, armv7, ppc64le, riscv64, s390x, loongarch64 |

CI runs `make check` in Alpine edge containers for amd64, 386, arm64, armv7,
riscv64, ppc64le, s390x and loong64, natively or under QEMU user mode
(`tools/ci-arch.sh ARCH STAGE`, stages `build`, `check` and `link`). The container
runs build the programs from the repository, not from the packages. CI builds,
installs and exercises the Debian package on amd64 only.

## Debian: `debian/`

A native source package (3.0 native) using debhelper (compat 13) with `dh` and
a few overrides in `debian/rules`. Build dependencies are `llvm-dev`,
`liblld-dev`, `binutils`, and `libclang-rt-dev` on i386 and armhf (the division
of 64 bits that the 32-bit targets take from compiler-rt). The package depends
on `libc6-dev | libc-dev`.

    dpkg-buildpackage -us -uc          # source and binary, with the tests
    DEB_BUILD_OPTIONS=nocheck dpkg-buildpackage -us -uc -b

The source package leaves out build products, git data, and the PDP-10 binaries
of the original compiler (`debian/source/options`).

## Alpine: `packaging/alpine/APKBUILD`

For `aports` (`community/imp72`). It depends on `musl-dev` and builds with
`llvm-dev`, `lld-dev` and `compiler-rt`. The `sha512sums` of the release
tarball for `pkgver` are filled in with `abuild checksum` once the tag
`v$pkgver` exists.
