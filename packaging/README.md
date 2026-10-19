# Packaging

## Which machines

imp72 needs a 64 bit little endian machine: its word is 64 bits and its
strings are packed little endian, so 32 bit machines (i386, armhf, armel, mipsel,
powerpc) and big endian ones are out. Of the 64 bit little endian machines the
distributions build for, pcc has a back end for these, and those are what the
packages list:

| | architectures |
|---|---|
| Debian | `amd64` `arm64` `mips64el` |
| Alpine | `x86_64` `aarch64` |

pcc's code generator (its pass 2, `cc1`) is used only on amd64, because that is
the only target the translator's IR suits; on every other machine `imp72stage0` uses
the C back end (`tools/ir2c.c`) and the system's C compiler, and pcc is not
built (`make WITH_PCC=no`, which is what `auto` means off x86_64). The tests
run in both configurations. Only the amd64 build of the Debian package has been
made and tested (not run through lintian); the Alpine recipe and the other machines have not been built.

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
