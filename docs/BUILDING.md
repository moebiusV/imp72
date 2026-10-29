# Building and running

    make                  # the seed, the runtime, then stage 1 and stage 2
    make check            # all the tests (a few minutes)
    make install PREFIX=/usr DESTDIR=/tmp/root

The seed (`stage0/`) is C99 and needs libc and the LLVM C API (below); it builds with any C99 compiler
(`make CC=clang`, `make CC=gcc`). The compiler is built in three steps
(`docs/BOOTSTRAP.md`): the seed compiles `src/` into stage 1, stage 1 compiles `src/` again into stage 2, and stage 2
is installed as `bin/imp72` with `share/imp72/syntax64.imc` and `lib/imp72/libimp72.a`. The seed stays as
`bin/imp72stage0`. The steps need the LLVM libraries (linked into the compilers) and an ELF `objcopy`; the driver runs the
system's `cc` to link an executable.

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
library in the program itself. The only program the `llvm` back end starts is the system's `cc`, to link an executable (`-c` and `-S` start none).

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
