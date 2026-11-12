# imp

Parent: [All Projects](https://moebiusv.github.io/) | Git Repo: [github.com/moebiusV/imp72](https://github.com/moebiusV/imp72) | `git clone https://github.com/moebiusV/imp72`

IMP72 on modern Unix machines: the extensible systems language of Ned Irons, as
implemented by Walter Bilofsky for the PDP-10 (DECUS 10-198a, 1973). The
compiler is the original's design and source, translated to IMP for 64-bit
words and IEEE 754 doubles. It compiles to native code through LLVM, in its own
process, and calls C and ELF or Mach-O objects through the C ABI.

IMP stands for (Ned) Irons Meta Processor: it is named for Ned Irons, who invented it, as LISP is named
for its List Processor. It follows up the Meta II project and is based on his earlier BNF-driven ALGOL 60
compiler.

Almost none of the language is built in. A small kernel (a lexer, Irons's
multi-track parser, a syntax graph, a code-tree matcher, a semantic stack)
starts empty. A bootstrap reader, `RSYN`, reads a notation for syntax
statements, and `src/syntax.imc` then defines the language in
statements of the form

    <CLASS> ::= elements ::= semantics ;

each carrying the semantics that generates its code. A language extension is a
file of such statements; `lib/ffi.imc` is one (`CCALL`, `EXTERNAL`).

## Quick start

    make                                  # the seed, the runtime library, stage 1 and stage 2
    make check
    make install PREFIX=/usr/local

    imp hello.imp && ./a.out            # compile, link, run
    imp -c a.imp b.imp                  # a.o and b.o
    imp -O2 hello.imp                   # optimization level (-O0 -O1 -O2 -O3 -Os -Oz; -Os is the default)
    imp -S hello.imp                    # assembly, hello.s
    imp -Sir hello.imp                  # LLVM IR text, hello.ll
    imp -c --target=aarch64-linux-gnu hello.imp    # an object for another system
    imp-cimport -n qsort -o decls.imp stdlib.h   # IMP72 declarations from a C header

The build needs a C99 compiler, a C++ compiler, GNU make, the LLVM C API
(`llvm-dev`) and, to link in the process, lld as a library (`liblld-dev`).
`docs/BUILDING.md` has the details. The options are in `docs/imp.1`
(`man imp` after installing).

A program is the statements before its first `SUBR`; it ends at `%%`:

    PRINT STG 0,'HELLO, WORLD',/;
    X:=6*7; PRINT STG 0,'THE ANSWER IS ',IGR 0,X,/;
    (PRINT IGR 3,K,STG 3,' SQ',IGR 6,K*K,/) FOR K TO 4;
    FINI(0)
    %%

IMP expressions have no operator precedence and group right to left:
`A+B*C` is `A+(B*C)`. `FOR K TO N` runs K from 0 through N.
`docs/LANGUAGE-NOTES.md` lists the other points where programs go wrong.

A C function is called with `CCALL`. A typed declaration gives its types once:

    EXTERNAL INT puts(PTR);
    CCALL puts('hello from C');

An undeclared C function is called with a fixed number of word arguments. A C
function that takes a variable number of arguments (`printf`, `open`) must be
declared with `...`: `EXTERNAL INT printf(PTR, ...);`.

## How the compiler is built

The compiler is IMP, apart from the seed and two short C++ files.

- **The seed** (`stage0/`, C99) is a port of the kernel and of the code
  generator to C. It builds stage 1 and is installed as `impstage0`.
- **Stage 1** is `src/` (the kernel, the driver, the LLVM glue, the IR writer)
  and `src/` (the code generator), compiled by the seed.
- **Stage 2** is the same sources compiled by stage 1. It is installed as
  `imp`.
- **`stage0/impld.cpp`** (47 lines of C++) gives lld's driver a C name, because
  lld has no C interface.
- **`stage0/lvtriple.cpp`** asks LLVM's own triple parser whether a `--target`
  triple is one it knows, because the C API cannot be asked.

The seed and stage 2 write byte-identical LLVM IR for the same program;
`tests/seed` checks it over the native tests, the oracle programs, the runtime
library and the code generator's own modules. Stage 1 and stage 2 also write
the same IR (`tests/direct/stage2.sh`). `docs/BOOTSTRAP.md` describes the route.

LLVM is the back end, in the compiler's process: it writes an object file,
assembly or LLVM IR. The driver links
executables with lld in the process where lld is installed as a library, and by
running `ld.lld` (`ld64.lld` on macOS) where it is not. It never runs `cc`.

The runtime library `libimp.a` (`PRINT`, `READ`, `FINI`, the entry `main`,
the helpers that generated code calls) is IMP too, in `lib/*.iml`.
`tools/mklib.sh` builds it with the seed, for the compilers themselves.
`tools/mkrt.sh` builds it with `imp` for any target; `make install` installs
that one.

## Platforms

`make check` runs in CI on Linux (amd64 and arm64, and the Debian package),
macOS arm64, FreeBSD, OpenBSD, NetBSD, and Alpine containers (musl) for amd64,
386, arm64, armv7, riscv64, ppc64le, s390x and loong64. Hosts are little- and
big-endian, 32 and 64 bit; a 32-bit host holds the 64-bit word in a
`long long`.

**Cross-compiling.** `--target=TRIPLE` writes objects, assembly or IR for every
target the installed LLVM has, with the byte order and pointer size of the
target. A triple LLVM does not know by name is refused. The driver links for
Linux (glibc and musl, about 25 architectures), FreeBSD, NetBSD, OpenBSD, macOS,
and mingw-w64 and Cygwin on Windows, given that system's C library files
(`--sysroot`) and a runtime library built for it (`--runtime`, made by
`sh tools/mkrt.sh TRIPLE`). Native linking needs no table of architectures. There
is no runtime library for Windows or Cygwin: objects, and programs that need no
runtime, work there. `docs/BUILDING.md` has the linker order and the flags per
target.

## Repository layout

    decus/          the DECUS 10-198 tape, unchanged (hashes, PROVENANCE.md)
    src/            the compiler in 64-bit IMP (.imc, as on the tape): the kernel, the code
                    generator, the driver, llvm.imc, ir2ll*.imc, and syntax.imc, the language's syntax
    stage0/         the seed (C99), the glue to the LLVM C API, impld.cpp
    lib/            the runtime library (IMP), ffi.imc, imp.h
    tools/          build scripts, tools written in IMP (run by imptool.sh, among them imp-cimport), fuzzers (Python), CI scripts, tools/bsd-vm
    tests/          native programs, goldens, the bootstrap, seed, cross-compiling and cimport tests
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
returns a word. `CCALL F(A,B)` calls a C function with arguments by value;
`EXTERNAL INT puts(PTR);` declares its types, and `...` marks a variadic one.
`CALLBACK INT CMP(PTR, PTR);` gives a `SUBR` a C signature, `CALLBACK(CMP)` is
the address of its C entry (for `qsort`, `signal` and the like), and `FNPTR`
declares a call through a function pointer. `STRUCT` declares the layout of a C
structure, `SIZEOF` and `OFFSETOF` give its size and offsets, and `FIELD(P, S.F)`
reads and writes a field of the structure at an address. `imp-cimport` writes
the `EXTERNAL`, `STRUCT` and `LET` declarations of the functions, structures and
constants of a C header. `docs/FFI.md` has all of it.

**Code generation.** The code generator builds chains of records whose
operations are those of an IR machine (`docs/porting/llvm-ir-machine.md`).
`src/demit.imc` turns the chains into IR trees, `src/ir2ll.imc` writes
them as LLVM IR text, and LLVM makes the object. `docs/ARCHITECTURE.md` is the
map.

**The oracle.** The original compiler runs on TOPS-10 6.03 on a simulated KA10
or KI10 (`docs/ORACLE.md`). The output of the programs in `tests/oracle/LIST`
on it is in `tests/golden/oracle/`, and `make check` compares the compiler's
output with it.

## Documentation

- [docs/manual/imp.md](docs/manual/imp.md), [docs/manual/imp.7](docs/manual/imp.7): the language manual (the 1973 RUNOFF source rendered as Markdown and as an mdoc page)
- [docs/imp.1](docs/imp.1), [docs/impstage0.1](docs/impstage0.1), [docs/cimport.1](docs/cimport.1): the manual pages of the compiler, of the seed and of `imp-cimport`
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): the map; [docs/BOOTSTRAP.md](docs/BOOTSTRAP.md): the build route
- [docs/BUILDING.md](docs/BUILDING.md): building, cross-compiling, tests, CI
- [docs/LANGUAGE-DIFFERENCES.md](docs/LANGUAGE-DIFFERENCES.md): IMP72 here and on the PDP-10; [docs/LANGUAGE-NOTES.md](docs/LANGUAGE-NOTES.md): pitfalls
- [docs/FFI.md](docs/FFI.md): IMP72 from C and C from IMP72, declarations, callbacks, structures, `imp-cimport`
- [docs/ROADMAP.md](docs/ROADMAP.md): what is not done
- [docs/decisions-and-status.md](docs/decisions-and-status.md): each design and implementation choice with its rationale and precedents, and whether it is done, left to do, rejected or left in the air The author's messages it cites are rows [R<n>] of the [decision register](docs/decision-register.md).
- [The tape and the port, side by side](https://moebiusv.github.io/imp72/docs/tape-vs-port/index.html): every module of the DECUS tape against its file in `src/` and `lib/`, token by token, old on the left in red and new on the right in green, as GitHub shows a split difference (`sh tools/token-diff.sh docs/tape-vs-port` makes the pages; `docs/porting/tape-diff.md` says how)
- [docs/ORACLE.md](docs/ORACLE.md): the original compiler as a reference
- [docs/internals/](docs/internals/): the kernel module by module; [docs/porting/](docs/porting/): the code generator's machine, layouts, modules, routine names compared with the tape
- [packaging/README.md](packaging/README.md): Debian and Alpine packages

## Provenance and licence

The sources in `decus/` are DECUS program 10-198a, *IMP - PDP-10 IMP72 Compiler,
Version 1.5*, October 3, 1973, by Walter Bilofsky, Yale University, Department of
Computer Science, submitted by Sonya Shapiro, Bolt Beranek and Newman, Inc.
IMP ((Ned) Irons Meta Processor) is by Edgar T. (Ned) Irons, who invented it. They come from the PDP-10 archive's listing of
the DECUS library tape 10-LIB-3,
<https://pdp-10.trailing-edge.com/decuslib10-03/index.html>, directory
`[43,50306]`, which states of the DECUS software that it was put into the public
domain at the time of donation. The hashes of every file are in `decus/`; see
`decus/PROVENANCE.md`.

The language is described in Bilofsky's *IMP72 Reference Manual* (DECUS 10-198,
1973; `decus/imp.rno` is its RUNOFF source) and in "Syntax Extension and the
IMP72 Programming Language" (SIGPLAN Notices, 1974).

The code in this repository outside `decus/` is under the ISC licence, in
`LICENSE`. `stage0/strlcpy.c` and `stage0/strlcat.c` are OpenBSD's `strlcpy`
and `strlcat` (ISC, Todd C. Miller; each file has its notice), compiled by
`stage0/strl.c` only where the C library has neither. `decus/` is the original
DECUS program (public domain, as above).
