# Building and running

    make                  # the seed, the runtime, then stage 1 and stage 2
    make check            # all the tests (a few minutes)
    make install PREFIX=/usr DESTDIR=/tmp/root

The seed (`stage0/`) is C99 and needs libc and the LLVM C API (below); it builds with any C99 compiler
(`make CC=clang`, `make CC=gcc`). The compiler is built in three steps
(`docs/BOOTSTRAP.md`): the seed compiles `src/` into stage 1, stage 1 compiles `src/` again into stage 2, and stage 2
is installed as `bin/imp72` with `share/imp72/syntax64.imc` and `lib/imp72/libimp72.a`. The seed stays as
`bin/imp72stage0`. The steps need the LLVM libraries (linked into the compilers); what the build used of `nm` and `objcopy` is `tools/imprename.imp` (IMP, compiled by the driver in use), for ELF and Mach-O. Where lld is installed as a
library too (`liblld-NN-dev`, `lld-dev`, Homebrew's `lld`; the same version as LLVM), the driver links executables in the process; otherwise it runs `ld.lld` (`ld64.lld`) as a
program, found in the `PATH` and then in the `bin` directory of the LLVM it was built with (OpenBSD's own linker is lld and has no library). It never runs `cc`.

Make variables: `CC`, `CFLAGS`, `LDFLAGS`, `DESTDIR`, `PREFIX`.

By hand:

    tools/pccir.sh                     # stage 1 in build/pccir1, stage 2 in build/pccir2/imp72d
    tools/direct.sh                    # the compiler built by the seed: build/direct/imp72d and imp72stage-pccir
    build/direct/imp72stage-pccir -o hello tests/native/hello.imp
    build/direct/imp72d -s TSX -o dump.txt -c ir.txt src/pccir/syntax64.imc lib/ffi.imc prog.imp

(`pccir` in these names is the directory and the IR's old name; no pcc is involved.)

`imp72d` is the compiler proper (`src/impm.imp`): options `-i` (the tape's commands from the terminal; `imp72 -i` does the same), `-I IMAGE` (start from an image saved with `NAME_/U`), `-s LETTERS` (the original's switches), `-o FILE` (listing and
dumps), `-c FILE` (the IR), `-T TARGET`, then the syntax file and the files to compile. `imp72stage-pccir` and
`imp72` are the same program with the driver around it (`-S`, `-c`, `-o`): they find the syntax file and the library,
make the object with LLVM in the process and link.

The seed alone: `cd stage0 && make`, then `./imp72s0 -s TSX -o dump.txt ../decus/syntax.imc`, or
`stage0/imp72stage0 -o prog prog.imp`.

LLVM in the process: where `llvm-config` and the LLVM C API headers are installed (`llvm-NN-dev`, `llvm-dev`), `tools/llvm.sh` gives the flags
and the seed (`stage0`) and the compilers `tools/direct.sh` builds link `libLLVM` (`WITH_LLVM=no` leaves it out; `LLVM_CONFIG` names another
`llvm-config`). The `llvm` back end then makes the object with the host's target machine in the process: there is no clang, and no `ir2ll`
program, in it. `imp72 --run prog.imp` compiles the program with ORC (LLJIT) and runs it in the process, finding the runtime and the C
library in the program itself. The link is lld in the process (`stage0/impld.cpp`, built where `tools/llvm.sh` finds `liblldELF` or `liblldMachO` and its headers), or lld as a program where there is no library
(`WITH_LLD=no` builds so anywhere, to test that); the loader and the C library are those this program itself runs with (`lib/imphost.c`: `PT_INTERP`, `dladdr`), with the table of
the architectures as the fallback of a program that has none. The only program the `llvm` back end ever starts is `ld.lld` (`-c`, `-S` and `--run` start none).

LLVM is the only back end; a compiler built without the LLVM libraries says so and stops. `tools/ir2ll.c` is part of the compiler.

Tests: `tests/native/run.sh` and `runc.sh` compile and run the programs of `tests/native/` and
compare their output; `tests/oracle/run.sh` compares with what the original compiler printed (`docs/ORACLE.md`);
`tests/direct/run.sh` runs them, the IEEE fuzz and a differential fuzz through the compiler built by the seed, and
`tests/direct/stage2.sh` requires stage 2 to write the same IR as stage 1 for every program; `make -C stage0 check` runs the
seed's unit tests.

Platforms checked in CI (`.github/workflows/ci.yml`): Linux on amd64, i386, arm64, armv7, ppc64le, riscv64, s390x and
loongarch64 (Alpine, native or under QEMU), Debian amd64 (the package), macOS on Apple silicon, FreeBSD, NetBSD and
OpenBSD (the LLVM libraries are needed on each).

Packages: `packaging/README.md`.

## Continuous integration

`.github/workflows/` has four workflows, which run, cancel and restart on their own: `ci-linux.yml` (amd64, arm64, the Debian package), `ci-alpine.yml` (musl, every architecture Alpine has, under
QEMU where it is not the runner's), `ci-bsd.yml` (FreeBSD, NetBSD, OpenBSD) and `ci-macos.yml`. To say which a push runs:

- a commit message with tags runs only the groups it names: `[ci:bsd]` the BSDs, `[ci:bsd][ci:macos]` those two (the groups are `linux`, `alpine`, `bsd` and `macos`); without a tag all four run; the
  tags of the last commit of the push count;
- `[skip ci]` in the message skips every workflow;
- a push that changes only documents (`docs/`, `*.md`, `decus/`, `oracle/`, `LICENSE`) runs none;
- "Run workflow" in the Actions tab starts one workflow by hand, whatever the message says;
- a new push cancels the runs of the same workflow that are still going; one job can be run again from the page of its run.

`tools/ci-run.sh` (build and `make check`, with the failure in GitHub annotations) is what the BSD and Linux jobs run, and `tools/ci-arch.sh ARCH` the same in an Alpine container on one's own machine.

