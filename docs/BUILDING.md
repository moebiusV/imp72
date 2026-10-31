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

    tools/llvm-ir.sh                     # stage 1 in build/llvm-ir1, stage 2 in build/llvm-ir2/imp72d
    tools/direct.sh                    # the compiler built by the seed: build/direct/imp72d and imp72stage-llvm-ir
    build/direct/imp72stage-llvm-ir -o hello tests/native/hello.imp
    build/direct/imp72d -s TSX -o dump.txt -c ir.txt src/llvm-ir/syntax64.imc lib/ffi.imc prog.imp

(`llvm-ir` in these names is the directory of the code generator and the name of the IR.)

`imp72d` is the compiler proper (`src/impm.imp`): options `-i` (the tape's commands from the terminal; `imp72 -i` does the same), `-I IMAGE` (start from an image saved with `NAME_/U`), `-s LETTERS` (the original's switches), `-o FILE` (listing and
dumps), `-c FILE` (the IR), `-T TARGET`, then the syntax file and the files to compile. `imp72stage-llvm-ir` and
`imp72` are the same program with the driver around it: they find the syntax file and the library, make the object with LLVM in the process and link.

The seed alone: `cd stage0 && make`, then `./imp72s0 -s TSX -o dump.txt ../tape/syntax.imc`, or
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

Platforms checked in CI (`.github/workflows/ci-*.yml`): Linux on amd64, i386, arm64, armv7, ppc64le, riscv64 and s390x
(Alpine, native or under QEMU; loongarch64 is run in an imported minirootfs, as Alpine has no Docker image for it), Debian amd64 (the package), macOS on Apple silicon, FreeBSD, NetBSD and
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


### CI in stages

The Alpine job of CI (`.github/workflows/ci-alpine.yml`) runs `tools/ci-arch.sh ARCH STAGE` for the stages build, check and link, each a step of its own. The copy of the tree,
the build and the log of every stage are kept in a work directory (`tmp/ci-ARCH` here, `work` in CI) that is mounted into the container and saved in the Actions cache even when a
stage fails. A stage that finished leaves a stamp and is skipped when asked for again; one that failed leaves none and runs again, `make` going on from the objects it has. So
"Re-run failed jobs" on the same commit goes on from where the last run stopped. Each failed stage writes its own annotations, the tails of the logs are in the summary of the job, and
the logs are uploaded as an artifact (`logs-*`).

### The fuzzers of the arithmetic

`tests/fuzz64/run.sh` (in `make check`) compiles random programs with both code generators, the seed and imp72, and compares what they print with an answer worked out in Python from the rules of the 64 bit word, never from a compiler. Three generators:

- `tools/imp-fuzz64.py`: integers (`+ - * /`, `AND OR XOR NOT`, comparisons, shifts), array indexing with offsets, bit-fields read and stored, over values from around every width a word has had.
- `tools/imp-fuzz-bytes.py`: byte pointers (`BYTEP P[k]<S,Q>` and the pointer to characters `<7,36>`: `<X>`, `<X>_E`, `<+X>`, `<+X>_E`, the step to the next field and to the next word) and block moves (`MOVE A THROUGH N TO B`, overlapping, a word at a time upward as BLT does).
- `tools/imp-fuzz-real2.py`: REAL over the whole double (denormals to overflow, signs of zero, infinities), the bits of the result (`[LOC(V)]`), the six comparisons, `FLT` of integers around 2**53 and 2**63, `FIX`. No NaN is generated (the machines' default NaN differs), nor `FIX` past 2**63 (x86 and aarch64 differ).

`tools/fuzz64.sh DRIVER SEED COUNT` runs one (`GEN=imp-fuzz-bytes.py`), and `tools/fuzz64-reduce.py` boils a wrong integer program down to the smallest wrong sub-expression. `tools/ieee-fuzz.sh` checks the printed text of REAL values (`PRINT FLT`). These need Python 3 and are skipped where there is none.

### The command line of imp72

`imp72` takes the options of a C compiler (`tests/cli/run.sh`):

    imp72 hello.imp                  compile and link: a.out
    imp72 -o hello hello.imp         the program is named
    imp72 -c a.imp b.imp             an object for each, a.o and b.o, in the current directory (the TOPS-10 REL file is a .o: ELF, Mach-O, ... as the system is)
    imp72 -S hello.imp               the assembly of the target, hello.s
    imp72 -Sir hello.imp             LLVM IR text, hello.ll
    imp72 a.imp b.imp c.o -o prog    several sources are compiled one by one and linked with the objects named
    imp72 --run hello.imp            compile with ORC and run in the process
    imp72 --version | --help

A file of syntax and declarations (`.imc`) is read by each compilation before the source, as the original's `/C` files are. `-o` with `-c` or `-S` and more than one source is refused, as the C compiler does.

**Cross-compiling** is `--target=TRIPLE` (LLVM's names, the vendor may be left out: `aarch64-linux-gnu`, `riscv64-linux-musl`, `i686-linux-gnu`, `armv7-linux-gnueabihf`, `x86_64-apple-darwin`) with `-c`, `-S` or `-Sir`: the object is in the format of that system, with
32 or 64 bit pointers as it has. **Linking for another system** (Linux and macOS targets so far; the ELF and Mach-O drivers of lld are both in the compiler, wherever it runs, so a Linux build links macOS programs and the reverse; COFF and WebAssembly are not linked, the runtime library being POSIX) works when imp72 is told where that system's files are: `--sysroot=DIR` (the root of that system: its `crt1.o`, `crti.o`, `crtn.o` and C library under `DIR/usr/lib/...`, or for macOS the `MacOSX.sdk` directory; or `IMP72SYSROOT`) and `--runtime=FILE` (the `libimp72.a` built for it, or put next to its C library; or `IMP72RUNTIME`). lld links for any target, so the linker is not what is needed. Without them the message lists what is missing and how to give it. The `libimp72.a` of a system can be built by `imp72` itself for any target once the runtime's C is IMP (`lib/imp72sys.c`, `lib/imphost.c`; not yet); until then build it on that system (`make`). A target of the other byte order (s390x, big-endian PowerPC) is refused with a message.
