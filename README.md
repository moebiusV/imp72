# imp72

IMP72 on modern Unix machines: the extensible systems language of Ned Irons, as
implemented by Walter Bilofsky for the PDP-10 (DECUS 10-198a, 1973). The
compiler is the original's design and source, translated to IMP for 64-bit
words and IEEE 754 doubles. It compiles to native code through LLVM, in its own
process, and calls C and ELF or Mach-O objects through the C ABI.

Almost none of the language is built in. A small kernel (a lexer, Irons's
multi-track parser, a syntax graph, a code-tree matcher, a semantic stack)
starts empty. A bootstrap reader, `RSYN`, reads a notation for syntax
statements, and `src/llvm-ir/syntax64.imc` then defines the language in
statements of the form

    <CLASS> ::= elements ::= semantics ;

each carrying the semantics that generates its code. A language extension is a
file of such statements; `lib/ffi.imc` is one (`CCALL`, `EXTERNAL`).

## Quick start

    make                                  # the seed, the runtime library, stage 1 and stage 2
    make check
    make install PREFIX=/usr/local

    imp72 hello.imp && ./a.out            # compile, link, run
    imp72 --run hello.imp                 # compile with ORC and run in the process
    imp72 -c a.imp b.imp                  # a.o and b.o
    imp72 -S hello.imp                    # assembly, hello.s
    imp72 -Sir hello.imp                  # LLVM IR text, hello.ll
    imp72 -c --target=aarch64-linux-gnu hello.imp    # an object for another system

The build needs a C99 compiler, a C++ compiler, GNU make, the LLVM C API
(`llvm-dev`) and, to link in the process, lld as a library (`liblld-dev`).
`docs/BUILDING.md` has the details. The options are in `docs/imp72.1`
(`man imp72` after installing).

A program is the statements before its first `SUBR`; it ends at `%%`:

    PRINT STG 0,'HELLO, WORLD',/;
    X:=6*7; PRINT STG 0,'THE ANSWER IS ',IGR 0,X,/;
    (PRINT IGR 3,K,STG 3,' SQ',IGR 6,K*K,/) FOR K TO 4;
    FINI(0)
    %%

IMP expressions have no operator precedence and group right to left:
`A+B*C` is `A+(B*C)`. `FOR K TO N` runs K from 0 through N.
`docs/LANGUAGE-NOTES.md` lists the other points where programs go wrong.

## How the compiler is built

The compiler is IMP, apart from the seed and a short C++ file.

- **The seed** (`stage0/`, C99) is a port of the kernel and of the code
  generator to C. It builds stage 1 and is installed as `imp72stage0`.
- **Stage 1** is `src/` (the kernel, the driver, the LLVM glue, the IR writer)
  and `src/llvm-ir/` (the code generator), compiled by the seed.
- **Stage 2** is the same sources compiled by stage 1. It is installed as
  `imp72`.
- **`stage0/impld.cpp`** (41 lines of C++) gives lld's driver a C name, because
  lld has no C interface.

The seed and stage 2 write byte-identical LLVM IR for the same program;
`tests/seed` checks it over the native tests, the oracle programs, the runtime
library and the code generator's own modules. Stage 1 and stage 2 also write
the same IR (`tests/direct/stage2.sh`). `docs/BOOTSTRAP.md` describes the route.

LLVM is the back end, in the compiler's process: `--run` compiles with ORC and
runs, the other modes write an object file, assembly or LLVM IR. The driver links
executables with lld in the process where lld is installed as a library, and by
running `ld.lld` (`ld64.lld` on macOS) where it is not. It never runs `cc`.

The runtime library `libimp72.a` (`PRINT`, `READ`, `FINI`, the entry `main`,
the helpers that generated code calls) is IMP too, in `lib/*.imp`.
`tools/mklib.sh` builds it with the seed, for the compilers themselves.
`tools/mkrt.sh` builds it with `imp72` for any target; `make install` installs
that one.

## Platforms

`make check` runs in CI on Linux (amd64 and arm64, and the Debian package),
macOS arm64, FreeBSD, OpenBSD, NetBSD, and Alpine containers (musl) for amd64,
386, arm64, armv7, riscv64, ppc64le, s390x and loong64. Hosts are little- and
big-endian, 32 and 64 bit; a 32-bit host holds the 64-bit word in a
`long long`. `--target` writes objects for every target the installed LLVM
has; linking for another system needs that system's C library files
(`--sysroot`, `--runtime`).

## Repository layout

    decus/          the DECUS 10-198 tape, unchanged (hashes, PROVENANCE.md)
    src/            the compiler in 64-bit IMP: the kernel, the driver, llvm.imp, ir2ll*.imp
    src/llvm-ir/    the code generator, and syntax64.imc, the language's syntax
    stage0/         the seed (C99), the glue to the LLVM C API, impld.cpp
    lib/            the runtime library (IMP), ffi.imc, imp72.h
    tools/          build scripts, tools written in IMP (run by imptool.sh), fuzzers (Python), CI scripts
    tests/          native programs, goldens, the bootstrap and seed tests
    oracle/         the setup for the original compiler on simulated TOPS-10
    docs/           guides, the language manual, internals, porting notes
    debian/ packaging/   Debian and Alpine packages

## Design

**The word.** The IMP word is 64 bits, a half is 32 bits, pointers are byte
addresses, `A[I]` scales by 8, so IMP data and C data are the same data.
Strings are NUL-terminated bytes. The name directory packs eight characters to
a word.

**Reals** are IEEE 754 doubles; a `REAL` variable holds the double's bits.

**Calls.** A `SUBR` is a C function that takes pointers to its arguments and
returns a word. `CCALL F(A,B)` calls a C function with arguments by value, and
`CALLBACK(F)` is the address of a C entry of a `SUBR`, for `qsort`, `signal` and
the like. `docs/FFI.md` has all of it.

**Code generation.** The code generator builds chains of records whose
operations are those of an IR machine (`docs/porting/llvm-ir-machine.md`).
`src/llvm-ir/demit.imp` turns the chains into IR trees, `src/ir2ll.imp` writes
them as LLVM IR text, and LLVM makes the object. `docs/ARCHITECTURE.md` is the
map.

**The oracle.** The original compiler runs on TOPS-10 6.03 on a simulated KA10
or KI10 (`docs/ORACLE.md`). The output of the programs in `tests/oracle/LIST`
on it is in `tests/golden/oracle/`, and `make check` compares the compiler's
output with it.

## Documentation

- [docs/manual/imp72.md](docs/manual/imp72.md), [docs/manual/imp72.7](docs/manual/imp72.7): the language manual (the 1973 RUNOFF source rendered as Markdown and as an mdoc page)
- [docs/imp72.1](docs/imp72.1), [docs/imp72stage0.1](docs/imp72stage0.1): the manual pages of the compiler and of the seed
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): the map; [docs/BOOTSTRAP.md](docs/BOOTSTRAP.md): the build route
- [docs/BUILDING.md](docs/BUILDING.md): building, cross-compiling, tests, CI
- [docs/LANGUAGE-DIFFERENCES.md](docs/LANGUAGE-DIFFERENCES.md): IMP72 here and on the PDP-10; [docs/LANGUAGE-NOTES.md](docs/LANGUAGE-NOTES.md): pitfalls
- [docs/FFI.md](docs/FFI.md): IMP72 from C and C from IMP72
- [docs/ROADMAP.md](docs/ROADMAP.md): planned language and interop work
- [docs/ORACLE.md](docs/ORACLE.md): the original compiler as a reference
- [docs/internals/](docs/internals/): the kernel module by module; [docs/porting/](docs/porting/): the code generator's machine, layouts, modules, routine names compared with the tape
- [packaging/README.md](packaging/README.md): Debian and Alpine packages

## Provenance and licence

The sources in `decus/` are DECUS program 10-198a, *IMP - PDP-10 IMP72 Compiler,
Version 1.5*, October 3, 1973, by Walter Bilofsky, Yale University, Department of
Computer Science, submitted by Sonya Shapiro, Bolt Beranek and Newman, Inc.
IMP is by Edgar T. (Ned) Irons. They come from the PDP-10 archive's listing of
the DECUS library tape 10-LIB-3,
<https://pdp-10.trailing-edge.com/decuslib10-03/index.html>, directory
`[43,50306]`, which states of the DECUS software that it was put into the public
domain at the time of donation. The hashes of every file are in `decus/`; see
`decus/PROVENANCE.md`.

The language is described in Bilofsky's *IMP72 Reference Manual* (DECUS 10-198,
1973; `decus/imp.rno` is its RUNOFF source) and in "Syntax Extension and the
IMP72 Programming Language" (SIGPLAN Notices, 1974).

The code in this repository outside `decus/` is under the ISC licence, in
`LICENSE`. `decus/` is the original DECUS program (public domain, as above).
