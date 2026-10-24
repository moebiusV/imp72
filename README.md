# imp72

IMP72 on modern Unix: Ned Irons's extensible systems language, as implemented
by Walter Bilofsky for the PDP-10 (DECUS 10-198, 1973), compiled to native code
for Linux, the BSDs and macOS through [pcc](https://github.com/moebiusv/pcc),
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
compiles IMP to pcc's intermediate representation, and pcc's pass 2 makes
machine code for amd64, i386 and aarch64. The language, `PRINT` and `READ`, reals
(IEEE 754), `CCALL` to C functions, and the programs of the test suite work, and
their output matches what the original compiler's programs print on a
simulated TOPS-10 (`docs/ORACLE.md`), apart from the word size and IEEE 754.
The seed, a port of the compiler to C in `stage0/`, builds the first stage from `src/`; that stage compiles `src/` again into the
second, which writes the same IR as the first for every test program; the
second is what is installed.

Translated to IMP so far: free storage, the directory, the lexer, the syntax
graph, the parser, error correction, the code tree, the semantic stack
reducer, the semantics interpreter, `RSYN`, the register and declaration
routines, the code generator (`src/pccir/`), the diagnostic print-outs, and the
driver program. Its words are 64 bits wide, names are eight characters to a
word, and `EXECUTE` runs an instruction of the pcc IR machine.

The work is on the branch `tape64`, which is not merged yet. Its last fully
tested commit is `3f6cde0`; the next one renames the free storage routines to
the tape's names and the compiler built from it hangs on one of its own
modules (`docs/porting/tape64-status.md`).

**What is next.**

1. Find that hang, then merge `tape64` and get CI green on every architecture.
2. Translate what is still C: `LETSYN`, `CVAL`, `FCON`, the `/A` code listing
   and `HELPD`. The table of semantic routines stays C (it holds addresses).
3. Bring the code generator in `src/pccir/` closer to the tape's names and text.
4. Compare the compiler's dumps (directory, code tree, semantics array, graph)
   with the original's, and the `/A` listings.
5. Make the seed-to-stage-2 build the only way the compiler is built and
   installed, and bring the Makefile, CI and docs in line.
6. Later: pcc as a library inside the compiler (IR in, a pointer to the compiled
   function out); an LLVM IR back end if pcc proves too limited; a build for
   the 36-bit PDP-10 itself.

## Contents

- [How IMP72 works](#how-imp72-works)
- [Repository layout](#repository-layout)
- [Building and running](#building-and-running)
- [Design decisions](#design-decisions)
- [Documentation](#documentation)
- [Provenance and licence](#provenance-and-licence)

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
- [docs/BUILDING.md](docs/BUILDING.md), [docs/PLAN.md](docs/PLAN.md), [docs/ORACLE.md](docs/ORACLE.md)
- [docs/porting/pccir-path.md](docs/porting/pccir-path.md), [pccir-machine.md](docs/porting/pccir-machine.md): the code generator and its machine
- [docs/porting/tape64-status.md](docs/porting/tape64-status.md): the state of the `tape64` branch
- [docs/porting/](docs/porting/): one note per module (`free.md`, `dir.md`, `lex.md`, `graph.md`, `parse.md`, `ercor.md`, `entree.md`, `cotree.md`, `dosem.md`, `rsyn.md`, `impsem.md`, `debug.md`, `stack.md`)
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
