# Building and running

    make                  # the seed, the runtime library, stage 1 and stage 2
    make check            # all the tests
    make install PREFIX=/usr DESTDIR=$PWD/tmp/root

## Requirements

- a C99 compiler and a C++17 compiler (the seed and `stage0/impld.cpp`; the
  compilers are also linked with `cc`), GNU make (`gmake` on the BSDs), `ar`, a POSIX shell;
- the LLVM C API: `llvm-dev` (Debian), `llvm-dev` (Alpine), Homebrew's `llvm`,
  and `llvm-config` in the `PATH` or named by `LLVM_CONFIG`;
- lld as a library to link in the process: `liblld-dev` (Debian), `lld-dev`
  (Alpine), Homebrew's `lld`, the same version as LLVM. Without it the driver
  runs `ld.lld` (`ld64.lld` on macOS) as a program, found in the `PATH` and then in
  the `bin` directory of the LLVM it was built with. OpenBSD's own linker is
  lld and it has no library;
- the C library's development files (`crt1.o`, `libc`): the driver links with
  them. It never runs `cc`;
- Python 3 for the fuzzers; the tests that need it are skipped without it.

The seed builds with `make CC=clang` or `make CC=gcc`. The make variables are
`CC`, `CFLAGS`, `LDFLAGS`, `DESTDIR` and `PREFIX` (default `/usr/local`).
`WITH_LLVM=no` and `WITH_LLD=no` (environment) leave LLVM or the lld library out
of the build; without LLVM the compilers say so and stop. `LLVM_CONFIG` names
another `llvm-config`.

## The steps

The compiler is built in three steps (`docs/BOOTSTRAP.md`). The seed compiles
`src/` into stage 1, stage 1 compiles `src/` again into stage 2, and `make
install` installs stage 2.

| target | what it makes |
|---|---|
| `stage0` | `stage0/imp72s0`, `stage0/imp72stage0` (the seed) |
| `build/lib/libimp72.a` | the runtime library, built by the seed (`tools/mklib.sh`); the compilers are linked with it |
| `llvm-ir` | stage 1 in `build/llvm-ir1`, stage 2 in `build/llvm-ir2` (`tools/llvm-ir.sh`); skipped where there are no LLVM libraries |
| `runtime` | `build/rt-host/libimp72.a`, built by stage 2 (`tools/mkrt.sh`) |

By hand:

    tools/llvm-ir.sh          # stage 1 and stage 2: build/llvm-ir2/imp72d and imp72stage-llvm-ir
    tools/direct.sh           # one stage, built by the seed: build/direct/imp72d, imp72stage-llvm-ir
    build/llvm-ir2/imp72stage-llvm-ir -o hello tests/native/hello.imp
    build/llvm-ir2/imp72d -s TSX -o dump.txt -c ir.txt src/llvm-ir/syntax64.imc lib/ffi.imc prog.imp

`imp72d` is the compiler proper (`src/impm.imp`). Its arguments are `-i` (the
original's commands from the terminal), `-I IMAGE` (start from an image saved
with `NAME_/U`), `-s LETTERS` (the original's switches), `-o FILE` (listing and
dumps), `-c FILE` (the IR file), `-T TARGET`, then the syntax file and the
files to compile. `imp72stage-llvm-ir` and `imp72` are the same program with the
driver around it: they find the syntax file and the library, make the object with
LLVM in the process, and link.

The seed alone: `make -C stage0`, then `stage0/imp72stage0 -o prog prog.imp`,
or `stage0/imp72s0 -s TSX -o dump.txt src/llvm-ir/syntax64.imc`.

## Installed layout

    PREFIX/bin/imp72                       stage 2
    PREFIX/bin/imp72stage0                 the seed
    PREFIX/lib/imp72/libimp72.a            the library stage 2 built
    PREFIX/share/imp72/syntax64.imc, ffi.imc
    PREFIX/include/imp72.h
    PREFIX/share/man/man1/imp72.1, imp72stage0.1
    PREFIX/share/man/man7/imp72.7          the language manual
    PREFIX/share/doc/imp72/                imp72.md, FFI.md, LANGUAGE-NOTES.md

Where stage 2 cannot be built (no LLVM libraries), `imp72` is a link to
`imp72stage0` and the library is the seed's. Both compilers find their parts
relative to their own place, and in the source tree they use the files of the
tree. `IMP72SYNTAX`, `IMP72FFI` and `IMP72LIB` name other files. The temporary
directory is `TMPDIR`, else `tmp/` of the tree, else the current directory.

## LLVM and lld in the process

`tools/llvm.sh` gives the flags (`cflags`, `libs`, `bindir`, `imp`). The seed
(`stage0/`) and the compilers `tools/direct.sh` builds link `libLLVM`. The back
end makes the object with the host's target machine in the process, so no
`clang` runs. `imp72 --run prog.imp` compiles with ORC (LLJIT) and runs in the
process, finding the runtime and the C library in the program itself.

The link is lld in the process (`stage0/impld.cpp`, built where `tools/llvm.sh`
finds `liblldELF` or `liblldMachO` and their headers), or lld as a program where
there is no library (`WITH_LLD=no` builds so on any system, to test it). The
loader and the C library are those the compiler itself runs with
(`lib/imphost*.imp`: `PT_INTERP`, `dl_iterate_phdr`), with a table of
architectures as the fallback. The only program the driver starts is `ld.lld`
(`ld64.lld`); `-c`, `-S` and `--run` start none.

## Cross-compiling

`--target=TRIPLE` (LLVM's names; the vendor may be left out:
`aarch64-linux-gnu`, `riscv64-linux-musl`, `i686-linux-gnu`,
`armv7-linux-gnueabihf`, `x86_64-apple-darwin`) with `-c`, `-S` or `-Sir`
writes the output for that system, in its object format and with its pointer
size. A target of the other byte order from the host's is refused.

Linking for another system (Linux and macOS targets; the ELF and Mach-O drivers
of lld are both in the compiler, so a Linux build links macOS programs and the
reverse) needs that system's files:

- `--sysroot=DIR` (or `IMP72SYSROOT`): the root of that system, with `crt1.o`,
  `crti.o`, `crtn.o` and the C library under `DIR/usr/lib/...`, or for macOS the
  `MacOSX.sdk` directory;
- `--runtime=FILE` (or `IMP72RUNTIME`): the `libimp72.a` built for it.

Without them the error message lists what is missing. Objects can be linked for ELF and Mach-O targets; for other formats (COFF, WebAssembly) the compiler writes objects only.

`sh tools/mkrt.sh TRIPLE` builds the runtime library for a target with `imp72`
and no C compiler, into `build/rt-TRIPLE/libimp72.a`; with no triple it builds
this machine's, `build/rt-host`. It takes the constants of the target's C
library from `tools/sysconst.sh`. A system that script cannot describe from the
build machine (the size of a `FILE` on NetBSD) is refused: build the library
on that system. For a 32-bit target it adds the target's compiler-rt members
from the LLVM at hand (`tools/llvm.sh builtins TRIPLE`) and says so when they are
missing.

## Tests

`make check` runs, under `tools/guard.sh`:

| test | checks |
|---|---|
| `make -C stage0 check` | the seed's unit tests (`tests/stage0`) and its syntax test |
| `tests/native/run.sh` | each `tests/native/NAME.imp` is compiled, linked with `NAME_main.c` where there is one, run, and compared with `NAME.out` |
| `tests/oracle/run.sh` | the programs of `tests/oracle/LIST` against the output the original compiler printed (`docs/ORACLE.md`) |
| `tests/ieee/run.sh` | random real arithmetic against an independent IEEE 754 reference |
| `tests/direct/run.sh` | the native and oracle tests, the IEEE fuzz and a differential fuzz through the compiler the seed builds |
| `tests/direct/stage2.sh` | a compiler built by the seed and one built by that compiler write the same IR for every program |
| `tests/direct/ir2ll.sh` | the IMP LLVM IR writer and `tools/ir2ll.c` write the same text |
| `tests/interactive/run.sh` | `imp72d -i`, the original's commands |
| `tests/image/run.sh` | saving the compiler with `/U` and starting from it with `-I` |
| `tests/orc/run.sh` | `--run`, and objects made in the process |
| `tests/llvm/gen.sh` | `tools/llvm.sh imp` in each of its shapes |
| `tests/link/run.sh` | the link with lld in the process, with no `cc` on the `PATH` |
| `tests/runtime/run.sh` | the runtime library built by `imp72`, for the host and other targets |
| `tests/seed/run.sh` | the seed and stage 2 write the same LLVM IR |
| `tests/cli/run.sh` | the command line of `imp72` |
| `tests/fuzz64/run.sh` | random integer, byte-pointer and real programs against 64-bit rules worked out in Python |
| `tests/manual/run.sh` | `docs/manual/imp72.md` and `imp72.7` are what the RUNOFF converters make of `decus/imp.rno` |

## Fuzzers

`tests/fuzz64/run.sh` compiles random programs with the seed and with `imp72`
and compares what they print with an answer worked out in Python from the
rules of the 64-bit word, never from a compiler. Three generators:

- `tools/imp-fuzz64.py`: integers (`+ - * /`, `AND OR XOR NOT`, comparisons,
  shifts), array indexing with offsets, bit-fields read and stored.
- `tools/imp-fuzz-bytes.py`: byte pointers (`BYTEP P[k]<S,Q>`, the pointer to
  characters `<7,36>`, `<X>`, `<X>:=E`, `<+X>`, `<+X>:=E`, the step to the next
  field and to the next word) and block moves (`MOVE A THROUGH N TO B`).
- `tools/imp-fuzz-real2.py`: REAL over the whole double (denormals to overflow,
  signs of zero, infinities), the bits of the result, the six comparisons,
  `FLT` of integers around 2**53 and 2**63, `FIX`. It generates no NaN and no
  `FIX` past 2**63, where machines differ.

`tools/fuzz64.sh DRIVER SEED COUNT` runs one (`GEN=imp-fuzz-bytes.py`), and
`tools/fuzz64-reduce.py` reduces a wrong integer program to the smallest wrong
sub-expression. `tools/ieee-fuzz.sh SEED COUNT` checks the printed text of REAL
values (`PRINT FLT`).

## Continuous integration

`.github/workflows/` has four workflows: `ci-linux.yml` (amd64, arm64, the Debian
package), `ci-alpine.yml` (musl: amd64, arm64, ppc64le, riscv64, s390x,
loong64, 386, armv7, under QEMU where the runner is another architecture),
`ci-bsd.yml` (FreeBSD, NetBSD, OpenBSD in virtual machines) and `ci-macos.yml`.
To say which a push runs:

- a commit message with tags runs only the groups it names: `[ci:linux]`,
  `[ci:alpine]`, `[ci:bsd]`, `[ci:macos]`, several together (`[ci:bsd][ci:macos]`);
  `[ci:loong64]` runs only the loong64 job of the Alpine workflow; without a tag
  all four run; the tags of the last commit of the push count;
- `[skip ci]` in the message skips every workflow;
- a push that changes only documents (`docs/`, `*.md`, `decus/`, `oracle/`,
  `LICENSE`, `.gitignore`) runs none;
- "Run workflow" in the Actions tab starts one workflow by hand;
- a later push cancels the runs of the same workflow that are still going; one job
  can be run again from the page of its run.

`tools/ci-run.sh NAME` (build and `make check`, failures as GitHub annotations,
log in `tmp/ci-NAME.log`) is what the BSD and Linux jobs run. The Debian job
builds the package with `dpkg-buildpackage`, installs it and compiles a program
with it.

`tools/ci-arch.sh ARCH STAGE` runs the same in an Alpine container on one's own
machine, one stage at a time: `build`, `check`, `link` (Docker's architecture
names: `arm64`, `riscv64`, `ppc64le`, `s390x`, `amd64`, `386`, `arm/v7`,
`loong64`). The copy of the tree, the build and the log of every stage are kept in a
work directory (`tmp/ci-ARCH` locally, `work` in CI) that is mounted into the
container and, in CI, saved in the Actions cache even when a stage fails. A
finished stage leaves a stamp and is skipped when asked for again; one that
failed leaves none and runs again, `make` going on from the objects it has.

## Runaway jobs

`make` and `make check` run under `tools/guard.sh`, which kills the whole job
(its process group) when `build/` and `tmp/` together pass 10 GB (`-g GB`,
`-m MB`; `GUARD_OFF=1` turns it off; `tmp/vm`, the images of `tools/bsd-vm`, does
not count). The scripts source `tools/limits.sh` (2 GB for a file one process
writes, 40 minutes of CPU for one process, best effort;
`IMP72_NOLIMITS=1` leaves it out), and a log that may grow goes through
`tools/capped.sh FILE CMD...` (at most 200 MB, the beginning, which says why).
The scratch directory is `tmp/` of the tree (not in git).

## The BSDs on one's own machine

`tools/bsd-vm/bsd-vm.sh` runs the machines the BSD jobs use, OpenBSD 8.0 and
NetBSD 11.0 (amd64), under QEMU with KVM in a container (docker and `/dev/kvm`
are all it needs), so that a failure of those jobs can be examined with a shell
and a debugger.

    sh tools/bsd-vm/bsd-vm.sh fetch openbsd     # the image of the cross-platform-actions project, once
    sh tools/bsd-vm/bsd-vm.sh start openbsd     # a fresh machine on a copy-on-write overlay: the image is never written to
    sh tools/bsd-vm/bsd-vm.sh ci openbsd        # copy the tree (committed and not) and run tools/ci-run.sh there; `ci openbsd check` runs only gmake check
    sh tools/bsd-vm/bsd-vm.sh ssh openbsd [CMD] # a shell as runner
    sh tools/bsd-vm/bsd-vm.sh stop openbsd      # halt the guest from inside, then the container

`status` and `clean` complete the commands. Everything is in `tmp/vm/OS`. Stop
a machine with `stop`, not by killing it: an OpenBSD killed while it relinks
its kernel at boot comes up with files missing and needs `clean` and `start`
again. `VM_CPUS` (default `6-9`) and `VM_MEM` (default `5G`) set the CPUs and
the memory. The OpenBSD image comes up without a carrier on its network card and
has no login on the serial port; the tool logs in on the video console through
the QEMU monitor (`tools/bsd-vm/type.imp` writes the `sendkey` commands) and
brings the interface up.

## Packages

`packaging/README.md`.
