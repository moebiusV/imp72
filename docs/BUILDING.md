# Building and running

    make                  # the seed, the runtime, pcc's pass 2 (cc1), then stage 1 and stage 2
    make check            # all the tests (a few minutes)
    make install PREFIX=/usr DESTDIR=/tmp/root

The seed (`stage0/`) is C99 with no dependencies beyond libc, and builds with any C99 compiler
(`make CC=pcc`, `make CC=clang`; with pcc see `docs/porting/pcc-notes.md`). The compiler is built in three steps
(`docs/BOOTSTRAP.md`): the seed compiles `src/` into stage 1, stage 1 compiles `src/` again into stage 2, and stage 2
is installed as `bin/imp72` with `share/imp72/syntax64.imc` and `lib/imp72/libimp72raw.a`. The seed stays as
`bin/imp72stage0`. The steps need pcc's pass 2 (built on x86_64, i386 and aarch64), an ELF `objcopy` and a C compiler for
the assembler and the link; where one is missing `imp72` is the seed.

Make variables: `CC`, `CFLAGS`, `LDFLAGS`, `DESTDIR`, `PREFIX`, and `WITH_PCC=yes|no|auto` (auto: build pcc's cc1 where
its back end works). The submodule `third_party/pcc` must be checked out (`git submodule update --init`).

By hand:

    tools/pccir.sh                     # stage 1 in build/pccir1, stage 2 in build/pccir2/imp72d
    tools/direct.sh                    # the compiler built by the seed: build/direct/imp72d and imp72stage-pccir
    build/direct/imp72stage-pccir -o hello tests/native/hello.imp
    build/direct/imp72d -s TSX -o dump.txt -c ir.txt src/pccir/syntax64.imc lib/ffi.imc prog.imp

`imp72d` is the compiler proper (`src/impm.imp`): options `-i` (the tape's commands from the terminal; `imp72 -i` does the same), `-s LETTERS` (the original's switches), `-o FILE` (listing and
dumps), `-c FILE` (the pcc IR), `-T TARGET`, then the syntax file and the files to compile. `imp72stage-pccir` and
`imp72` are the same program with the driver around it (`-S`, `-c`, `-o`): they find the syntax file and the library,
run pcc's pass 2, assemble and link.

The seed alone: `cd stage0 && make`, then `./imp72s0 -s TSX -o dump.txt ../decus/syntax.imc`, or
`stage0/imp72stage0 -o prog prog.imp` (pcc, or `-C` for the C back end).

Tests: `tests/native/run.sh` and `runc.sh` compile and run the programs of `tests/native/` (pcc and the C back end) and
compare their output; `tests/oracle/run.sh` compares with what the original compiler printed (`docs/ORACLE.md`);
`tests/direct/run.sh` runs them, the IEEE fuzz and a differential fuzz through the compiler built by the seed, and
`tests/direct/stage2.sh` requires stage 2 to write the same IR as stage 1 for every program; `make -C stage0 check` runs the
seed's unit tests.

Platforms checked in CI (`.github/workflows/ci.yml`): Linux on amd64, i386, arm64, armv7, ppc64le, riscv64, s390x and
loongarch64 (Alpine, native or under QEMU), Debian amd64 (the package), macOS on Apple silicon, FreeBSD, NetBSD and
OpenBSD. Where pcc's back end or an ELF `objcopy` is missing the seed's C back end is used.

Packages: `packaging/README.md`.
