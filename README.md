# imp72

IMP72 on modern Unix: Ned Irons's extensible systems language, as implemented
by Walter Bilofsky for the PDP-10 (DECUS 10-198, 1973), compiled to native code
for Linux, the BSDs and macOS through LLVM (or C, or [pcc](https://github.com/moebiusv/pcc)),
and calling C and ELF objects through the C ABI.

Almost none of the language is built in. A small kernel (a lexer, Irons's
multi-track parser, a syntax graph, a code-tree matcher, a semantic stack)
starts empty; a bootstrap reader, `RSYN`, reads a tiny notation for syntax
statements; and `syntax.imc` then defines the whole language in statements of
the form

    <CLASS> ::= elements ::= semantics ;

each carrying the semantics that generates its code. The language is extended by
adding such statements.

## Status

**Where the project is.** The compiler is the original's source, translated to
64-bit IMP in `src/` (the tape itself is kept untouched in `decus/`). It
compiles IMP to pcc's intermediate representation, which a back end turns
into machine code: LLVM IR (the default; in the compiler's own process through
the LLVM C API: there is no clang in it, and it is built where the LLVM libraries are installed), C for any C compiler, or
pcc's pass 2 where pcc is built (`make WITH_PCC=yes`, `--backend=pcc`).
`imp72 --run prog.imp` compiles a program with ORC and runs it in the process.
The compiler builds and compiles itself on little- and big-endian hosts, 32 and 64 bit
(tried on amd64, i386, arm64, s390x and a 32-bit big-endian PowerPC under emulation). The language, `PRINT` and `READ`, reals
(IEEE 754), `CCALL` to C functions, and the programs of the test suite work, and
their output matches what the original compiler's programs print on a
simulated TOPS-10 (`docs/ORACLE.md`), apart from the word size and IEEE 754.
The build has three steps. The seed, a C port of the compiler in `stage0/`,
compiles `src/` into stage 1. Stage 1 compiles `src/` again into stage 2, and
stage 2 writes the same IR as stage 1 for every test program. Stage 2 is the
compiler that is installed.

Translated to IMP so far: free storage, the directory, the lexer, the syntax
graph, the parser, error correction, the code tree, the semantic stack
reducer, the semantics interpreter, `RSYN`, the register and declaration
routines, the code generator (`src/pccir/`), the diagnostic print-outs, and the
driver program. Its words are 64 bits wide, names are eight characters to a
word, and `EXECUTE` runs an instruction of the pcc IR machine.

`make check` passes on amd64 and i386, and the build and tests ran clean on
arm64, armv7 and, through the C back end, riscv64, ppc64le and s390x. loong64,
mips64le and the BSDs and macOS have not been run since the translation of
the tables and the driver program.

**What is next.**

1. Run the remaining platforms (loong64, mips64le, the BSDs, macOS) and get CI green everywhere.
2. `imp72` is IMP now but for the seed (`stage0/`, the trusted stage 0), the glue to LLVM ORC (`stage0/llvmorc.c`,
   `tools/ir2ll.c`), the little C that says what only C can (`lib/imp72sys.c`, `lib/imp72main.c`) and the C back end
   `tools/ir2c.c`: the driver, the system part, the image, the table of semantic routines, the IR builders and the names,
   and now the runtime library (PRINT, READ, FINI and the rest) are IMP over `CCALL` (`docs/porting/c-to-imp.md`).
3. Bring the code generator in `src/pccir/` closer to the tape's names and text.
   Restore the tape where a departure is marked provisional in
   `docs/porting/modules.md` (`COMPIL`, inline in the tape's main program).
4. Compare the compiler's dumps (directory, code tree, semantics array, graph)
   with the original's, and the `/A` listings.
5. Make the seed-to-stage-2 build the only way the compiler is built and
   installed, and bring the Makefile, CI and docs in line.
6. Release the current set of changes as v0.9.0. Then, before v1.0.0, study in
   depth (do not implement) six things, and write up the recommendations and
   the impact on the code: UTF-8 symbols for operators; C's syntax for strings
   and characters; the tape's channel I/O (`INIT`, `LOOKUP`, `INPUT`, the buffer
   header) kept as it is with `fopen` and `putc` underneath; the type that a
   routine returns, `int64_t` for an IMP routine against `long` for a C function
   called with `CCALL`, and whether C's return types should be declared;
   NUL-terminated C strings in IMP against strings that carry their length (the
   preference is for the latter); and file names that match Unix's limits (the
   tape's six characters and three of the extension, where `MKNAME` allows 15 and 7). The syntax
   of IMP72 does not change until the study is done and decided.
7. Later: a function pointer from the compiler for a single definition
   (`libimp72`, over ORC); a build for the 36-bit PDP-10 itself.

## Contents

- [From the PDP-10 to amd64](#from-the-pdp-10-to-amd64)
- [How IMP72 works](#how-imp72-works)
- [Repository layout](#repository-layout)
- [Building and running](#building-and-running)
- [Design decisions](#design-decisions)
- [Documentation](#documentation)
- [Provenance and licence](#provenance-and-licence)

## From the PDP-10 to amd64

The compiler was written in IMP for the PDP-10 and compiles IMP for the PDP-10.
Its source is a tape from 1973: 36-bit words, 18-bit halves, five 7-bit
characters to a word, floating point with a 27-bit fraction, code generated as
PDP-10 instructions and assembled by routines written in IMP too.

The first step was to get the original running as it was. It runs under TOPS-10
on a simulated KA10, and everything after that is checked against what it
prints.

The second was a seed: a port of the kernel to C, which reads the original
`syntax.imc` unchanged. Its code generator still makes PDP-10 instructions, but
a translator turns them into pcc's intermediate representation, and pcc makes
the machine code. That compiler can compile IMP programs for a modern machine,
and it can compile the tape's own modules.

The third step is the one still under way: translating the tape itself, module
by module, into IMP for a 64-bit machine. The word becomes 64 bits and the
halves 32; names are eight characters to a word, as a C string is; reals are
IEEE 754; the code generator emits pcc IR instead of PDP-10 instructions. The
seed compiles the translated modules into a first compiler, the first compiler
compiles them again, and when the two agree the C seed is no longer needed to
build the compiler.

## How IMP72 works

The pieces, in the order the language manual (section 4) gives them:

| module | role |
|---|---|
| `free`, `dir`, `stack` | relocatable free storage; the name directory (hash coded); the semantic stack, a pushdown stack, bit matrices |
| `lex` | input and lexical analysis |
| `rsyn` | the bootstrap reader for the initial syntax, and the routines that enter productions |
| `graph`, `parse`, `ercor` | the syntax graph and its connectivity matrices; Irons's parser; error correction and ambiguity resolution |
| `entree`, `cotree`, `dosem` | the code-generation tree; the semantic stack reducer; the semantics interpreter |
| `code*`, `impsem` | the semantic routines that generate code (pcc IR) and the register and declaration routines |
| `debug` | diagnostic dumps (graph, tree, semantics, parser trace) |
| `impm` | the driver program |

A compiler is made in stages. `RSYN` reads the first part of the syntax file
(the syntax for syntax) with the lexer alone. The parser, now able to read
syntax statements, reads the rest, which defines the base language and its
semantics; quoted semantics (`"A LS 32" => DEWOP(...)`) become patterns in the
code tree that are matched against the code-generation stack, most specific
first.

IMP expressions have **no operator precedence** (`A+B*C` is `A+(B*C)`), and
`FOR I TO N` includes `N`.

## Repository layout

    decus/          the DECUS 10-198 tape, unchanged, with hashes
                    (SHA256SUMS, SHA256SUMS.bin) and PROVENANCE.md
    src/            the compiler's sources in 64-bit IMP: the kernel modules,
                    impsyn64.imc (the syntax they are written in)
    src/pccir/      the code generator, the syntax file syntax64.imc, demit.imp
    stage0/         the seed: a C port of the compiler that builds the first
                    stage, and the C the compiler still needs (ir.c, the pcc IR
                    builder; sys.c; semtab.c)
    lib/            the runtime library (PRINT, READ, FINI, main), ffi.imc
    tools/          the build scripts, the oracle drivers, the fuzzers
    tests/          native programs, oracle goldens, the fixed-point tests
    oracle/         the TOPS-10 simulator setup
    docs/           the manual, internals, the porting notes
    debian/ packaging/  Debian and Alpine packages
    third_party/pcc the pcc back end (a submodule of github moebiusv/pcc)

## Building and running

    make                                  # the seed, the runtime, pcc's pass 2, then stage 1 and stage 2
    make check
    make install PREFIX=/usr/local
    imp72 -o hello hello.imp && ./hello

The seed is C99 with no dependencies beyond libc. See
[docs/BUILDING.md](docs/BUILDING.md), and
[docs/porting/pcc-notes.md](docs/porting/pcc-notes.md) for building pcc (also
with musl).

The compiler takes the original's switches as letters (`-s TSX`): `A` assembly
listing, `G` trace the parser, `I` print the directory, `N` print the
connectivity matrices, `P` trace semantics execution, `M` trace tree matching,
`S` print the semantics array, `T` print the code tree, `X` print the syntax
graph, `L` list the source, `E` trace productions entered, `Z` statistics. Dumps
go to the file given with `-o`. `-c FILE` writes the pcc IR instead of making
an object. The exit status is 1 if an error (not an advisory) was reported.

## Design decisions

**The word.** The IMP word is 64 bits, halves are 32 bits, pointers are byte
addresses and `A[i]` scales by 8, so IMP data and C data are the same data.
Strings are NUL-terminated bytes; the name directory keeps eight characters to
a word, the first in the low byte. The tables of the compiler (code tree,
semantics array, stack entries) have fields laid out for 64 bits.

**Reals** are IEEE 754 doubles.

**Calls.** An IMP `SUBR` is a C function taking and returning words, with
arguments by reference; `CCALL F(A,B)` calls a C function with arguments by
value (`docs/FFI.md`).

**Code generation.** The code generator builds pcc IR trees directly: `DEWOP`
and the routines around it take the operations of a machine whose instructions
are those of the IR (`docs/porting/pccir-machine.md`), and `src/pccir/demit.imp`
hands the chains to pcc's pass 2 through `stage0/ir.c`. `EXECUTE` runs one
instruction of that machine from a word, with registers `0R` to `15R` as its
accumulators. A `GO TO` to a tag of another routine is a `longjmp`.

**Free storage** is one growable array of words addressed by index; the arrays
in it are relocatable and doubly linked as in the original.

**The oracle.** The original compiler runs on TOPS-10 6.03 on a simulated KA10
(open-simh), set up by `tools/oracle-build.sh` and driven by `tools/oracle-*.py`
([docs/ORACLE.md](docs/ORACLE.md)). Its output for the programs in
`tests/oracle/LIST` is in `tests/golden/oracle/`, and `make check` compares the
compiler's with it. The three binaries in the archive lose a bit of every word;
the exact words are in `decus/exact/`.

## Documentation

- [docs/manual/imp72.md](docs/manual/imp72.md), [imp72.7](docs/manual/imp72.7): the language manual (the 1973 RUNOFF source rendered as Markdown and as an mdoc page)
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): the map; [docs/BOOTSTRAP.md](docs/BOOTSTRAP.md): the route to a compiler that compiles itself
- [docs/LANGUAGE-DIFFERENCES.md](docs/LANGUAGE-DIFFERENCES.md): IMP72 here and on the PDP-10; [docs/LANGUAGE-NOTES.md](docs/LANGUAGE-NOTES.md)
- [docs/FFI.md](docs/FFI.md): IMP72 from C and C from IMP72
- [docs/BUILDING.md](docs/BUILDING.md), [docs/ORACLE.md](docs/ORACLE.md)
- [docs/porting/pccir-path.md](docs/porting/pccir-path.md), [pccir-machine.md](docs/porting/pccir-machine.md): the code generator and its machine
- [docs/porting/modules.md](docs/porting/modules.md): the modules and what differs from the tape; [layouts.md](docs/porting/layouts.md): the layout of the compiler's words
- [docs/internals/](docs/internals/): the kernel module by module; the C back end
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

The code in this repository outside `decus/` and `third_party/pcc` is under the
ISC licence, in `LICENSE`. `decus/` is the original DECUS program (public
domain, as above); `third_party/pcc` has its own licence.
