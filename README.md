# imp72

IMP72 on modern Unix: a bootstrap of Ned Irons's extensible systems language,
as implemented by Walter Bilofsky for the PDP-10 (DECUS 10-198, 1973), onto
Linux, the BSDs and macOS, generating native code through
[pcc](https://github.com/moebiusv/pcc) and interoperating with C and ELF
objects through the C ABI.

The compiler is unusual. Almost none of the language is built in. A small
kernel (a lexer, Irons's multi-track parser, a syntax graph, a code-tree
matcher, a semantic stack) starts empty; a bootstrap reader, `RSYN`, reads a
tiny notation for syntax statements; and `syntax.imc` then defines the whole
language in statements of the form

    <CLASS> ::= elements ::= semantics ;

each carrying the semantics that generates its code. The language can be
extended by adding such statements. This project keeps that design.

## Status

IMP72 programs compile to native code and run. There are two back ends. pcc's
code generator (the moebiusv fork) is used where its back end works, which
today is amd64 and aarch64; the C back end, which turns the same IR into C for
the host's compiler, works wherever there is a C compiler and is the only one
elsewhere. The whole test suite passes on x86_64 and aarch64 with both back
ends and, with the C back end, on riscv64, ppc64le, s390x and loongarch64
(Alpine/musl containers under QEMU, as in `docs/porting/pcc-targets.md`):

    make                                     # stage 0, runtime, ir2c, cc1 on amd64 and aarch64
    stage0/imp72stage0 -o hello tests/native/hello.imp && ./hello
    stage0/imp72stage0 -C -o hello tests/native/hello.imp   # the C back end
    make check
    make install PREFIX=/usr/local

`imp72stage0` runs stage 0 (a C port of the compiler's kernel, with the original
syntax file `decus/syntax.imc` unmodified and the original code generator
ported to produce PDP-10 code chains), translates the chains to pcc IR, and
turns that into machine code with pcc's pass 2 (`cc1`, where pcc works) or into C
(`tools/ir2c.c`) for the host's compiler. What works: subroutines, expressions,
loops, conditionals, arrays, calls (any number of arguments), byte fields and
pointers, `BLT`, computed `GO TO`, `DATA`, reals (`FLOAT`, `FIX`, arithmetic,
constants), `PRINT` and `READ` with their formats (`FLT`, `FILE`, `DEVICE`,
`IMAGE MODE`), and `CCALL`, which calls C functions with arguments by value
(`docs/FFI.md`). What does not, and the survey of the compiler's own sources
(18 of the 19 modules translate, 14 compile to objects), is in
`docs/internals/codegen-and-translation.md`.

Self-hosting is under way: `tools/stage1.sh` builds `imp72s1`, stage 0 with five of its modules
(`stack`, `dir`, `graph`, `parse`, `ercor`: the stack and bit matrices, the directory, the syntax graph,
the Irons parser and the error corrector) replaced by the compiler's own sources in `src/`, ported to 64 bit
words and compiled by `imp72stage0`. `tests/self/stage1.sh` checks that it writes the same IR as stage 0
for the native tests, every module of the tape and the ported sources themselves (the stage 1
compiler compiling its own modules). `free`, `dosem` and `impsem` use executed instructions or jumps
out of a subroutine that need porting, `lex` reads files, `impm` is the TOPS-10 interface, and the
modules with the 36 bit formats of the semantics (`entree`, `cotree`, `codegen`) are next. See
`docs/porting/` (`stack.md`, `dir.md`, `graph.md`, `parse.md`, `ercor.md`).

Packages: `debian/` and `packaging/alpine/APKBUILD` (`packaging/README.md`; only
the amd64 Debian package has been built here). `.github/workflows/ci.yml`
builds and tests on every architecture above and on macOS arm64; its results
have not been seen yet.

Nothing has been compared with output from the original compiler yet; that is
what the oracle (Roadmap, phase 1) is for. Where this file says the port is
"faithful", it means it follows the source, not that it has been verified
against the running original.

## Contents

- [How IMP72 works](#how-imp72-works)
- [Repository layout](#repository-layout)
- [Building and running](#building-and-running)
- [Design decisions](#design-decisions)
- [The plan](#the-plan)
- [Findings so far](#findings-so-far)
- [Documentation](#documentation)
- [Provenance and licence](#provenance-and-licence)

## How IMP72 works

The pieces, in the order the original compiler's manual (section 4) gives them:

| module | role |
|---|---|
| `free`, `dir`, `stack` | relocatable free storage; the name directory (hash coded); the semantic stack, a pushdown stack, bit matrices |
| `lex` | input and lexical analysis |
| `rsyn` | the bootstrap reader for the initial syntax, and the routines that enter productions |
| `graph`, `parse`, `ercor` | the syntax graph and its connectivity matrices; Irons's parser; error correction and ambiguity resolution |
| `entree`, `cotree`, `dosem` | the code-generation tree; the semantic stack reducer; the semantics interpreter |
| `code*`, `impsem`, `amod`, `rmod`, `omod` | semantic routines that generate PDP-10 code, assemble it, allocate registers, write relocatable output |
| `debug` | diagnostic dumps (graph, tree, semantics, parser trace) |

A compiler is made in stages. `RSYN` reads the first part of `syntax.imc` (the
syntax for syntax) with the lexer alone. The parser, now able to read syntax
statements, reads the rest, which defines the base language and its semantics;
quoted semantics (`"A LS 18" => DEWOP(...)`) become patterns in the code tree
that are matched against the code-generation stack, most specific first.
Finally `impsyn.imc` adds the syntax the compiler's own modules are written in.

IMP expressions have **no operator precedence** (`A+B*C` is `A+(B*C)`), and
`FOR I TO N` includes `N`; every port in this repository spells the grouping
out.

## Repository layout

    decus/          the DECUS 10-198 tape directory, unchanged, with hashes
                    (SHA256SUMS, SHA256SUMS.bin) and PROVENANCE.md
    stage0/         the hand port of the compiler to C99 (kernel, code generator,
                    translator to pcc IR), and its driver imp72s0
    lib/            the runtime library (PRINT, READ, FINI, main) and ffi.imc
    src/            the compiler's own sources ported to 64 bits (stack so far)
    tools/          imp72stage0 (the compiler driver), ir2c (the C back end),
                    stage1.sh, survey.sh, modules.sh
    debian/ packaging/  Debian and Alpine packages
    tests/stage0/   unit tests, and the syntax-file integration test
    tests/native/ self/  programs run against expected output; the ported
                    modules and the stage 1 compiler
    docs/           PLAN.md, BUILDING.md, FFI.md, internals/, porting/
    third_party/pcc the pcc back end (a submodule of github moebiusv/pcc)
    backend/ oracle/ reserved for later phases (the PDP-10 oracle)

The `.imc` files in `decus/` are the original sources, in upper case with DEC-10
CRLF line ends. They are never edited: ported sources will live in `src/`.

## Building and running

Stage 0 is C99 with no dependencies beyond libc: `make` builds `libstage0.a`
and the driver `imp72s0`; `make check` runs the tests; `make CC=pcc` or
`CC=clang` works. See [docs/BUILDING.md](docs/BUILDING.md), and
[docs/porting/pcc-notes.md](docs/porting/pcc-notes.md) for building and using pcc
itself (including with musl).

The driver takes the original's compiler switches as letters (`-s TSX`):
`G` trace the parser, `I` print the directory, `N` print the connectivity
matrices, `P` trace semantics execution, `M` trace tree matching, `S` print
the semantics array, `T` print the code tree, `X` print the syntax graph, `L`
list the source, `E` trace productions entered, `Z` statistics. Dumps go to the
file given with `-o`, else standard output. The exit status is 1 if an error
(not an advisory) was reported.

## Design decisions

These were made deliberately; the reasons are here so they can be argued with.

### The target

**Native code through pcc, on modern Linux, the BSDs and macOS, with the C ABI
as the foreign-function interface.** IMP72 `SUBR`s are C functions taking and
returning machine words (`REAL` as `double`), so IMP calls libc and C calls IMP
directly, and IMP data (`COMMON`) can be linked from C. There is no wrapper
layer and no separate FFI syntax beyond declarations of foreign signatures
(varargs, `REAL` arguments) that the ABI needs.

### The word

**A 64-bit word and byte addresses.** The original is built on 36-bit words, 18-bit
halfwords, five 7-bit characters to a word and word addressing (`LOC(X)+1` is
the next word). For native code that interoperates with C, the IMP word is
`int64_t`, pointers are real byte addresses (so `A[i]` scales by 8), and
strings are NUL-terminated bytes. The compiler's own sources therefore need a
documented port that removes the 36-bit idioms (name packing, `LS 28`/`RS 7`,
`CALLI`, `TWOSEG`), done module by module. Emulating 36 bits was rejected:
it would make FFI need marshalling and make the generated code slow.

### Stage 0 in C99, and it must compile with pcc

**Stage 0 is C99, with GNU extensions allowed only where pcc compiles them.**
It links in-process with pcc's `mip/` and `arch/` objects (as pcc's own f77
does), so the bootstrap must be something pcc builds. Everything in `stage0/`
compiles and passes its tests with the host compiler, under
AddressSanitizer/UndefinedBehaviorSanitizer, and with pcc (also against musl,
as static executables), and a pcc build gives byte-identical dumps to the host
build.

### Faithful ports, with the original's tables

**Port the algorithms, not the idea.** Each C file names the `.imc` it ports
and each function the routine; data structures keep the original's shape.

- **Free storage is one array of words, addressed by index**, with the same
  doubly linked relocatable arrays, best-fit gaps and packing, so every table
  is an index into `fmem` as in the original (`FORG`). `FREE`/`FREES` are
  inline functions, not macros: AddressSanitizer found that `FREES(a, f())`
  stores through a stale pointer when `f` grows storage.
- **The tables whose raw words the original prints keep their 36-bit formats**
  (`SEMS`, `PROSEM`, the code tree's stuff words, the code stack, the semantic
  stack), as sign-extended values in a 64-bit `word`, so `debug.c`'s octal
  dumps can be compared digit for digit with the original's. The syntax graph
  and the tree's links word, which nothing prints raw, use wider fields.
- **The directory keeps the original's name packing and hash** (five 7-bit
  characters to a word; hash of the first five), so a name's directory index is
  the number the PDP-10 compiler computes; everything above the directory sees C
  strings.
- **Quirks are reproduced, and recorded** (see [Findings](#findings-so-far)),
  not fixed, until the oracle says what the original does. Where the original
  reads past an array or its own data (the unchecked `BIT` macro), the port
  reads zero instead, and says so.

### The back end: IR, not AST

**pcc's pass-2 interface, entered at the IR level.** The front end hands pcc
`NODE` trees in `struct interpass` and calls `pass2_compile()`, the way pcc's
f77 front end does. pcc's pass-1 trees (`P1ND`) were rejected: they carry C
symbol tables, C types and `buildtree`'s C rules, need most of ccom linked in,
and fight a word-oriented, essentially untyped language; they would also
duplicate what IMP's own code tree already does (pattern-matched semantic
selection, constant folding, special cases). pcc also has a textual form of the
same IR (`--enable-twopass`), which we emit too, for golden tests and debugging.
The cost is a thin per-target layer for call lowering, function labels, data
and constants; with one word type and `REAL`, it is small.

The front end produces a *pre-ABI* IR (calls as `CALL(name, args)`, parameters
as `PARAM n`, locals as temporaries), then either lowers it for pcc or prints
portable C from it:

### Targets, and the C fallback

pcc's code generator is used wherever the moebiusv fork of pcc has a back end
that works: amd64 and aarch64 (the fork replaced the 32 bit ARM port that
aarch64 had been with an AAPCS64 one). Testing it on the machines we can run
is in [docs/porting/pcc-targets.md](docs/porting/pcc-targets.md). The
**C-emitting back end from the same IR covers every other host**, macOS arm64
included until the fork's back end has Mach-O, and is tested on riscv64,
ppc64le, s390x and loongarch64 as well as aarch64 and x86_64. Each pcc target
needs only a small table in the translator (its argument registers, result
register, frame and stack pointers). The aarch64 back end is still missing
structures-in-IMP-calls, Darwin deltas (Apple's `@PAGE` relocations, all
variadic arguments on the stack, x18, the SDK headers), described in
[docs/PLAN.md](docs/PLAN.md).

### The oracle

**Differential testing against the original compiler.** The original IMP 1.5 runs
on TOPS-10 6.03 on a simulated KA10 (open-simh), set up by `tools/oracle-build.sh`
and driven by `tools/oracle-*.py`; [docs/ORACLE.md](docs/ORACLE.md) says how. Its output
for the programs in `tests/oracle/LIST` is stored in `tests/golden/oracle/`, and
`make check` compares imp72stage0's with it: `hello`, `algo` and `features` are
identical. The oracle also showed that the archive's copies of the three binaries
lose a bit of every word; the exact words (from the DECUS tape image) are in
`decus/exact/`. Still to do with it: the dumps (`GPRINT`/`TPRINT`/`SEMPER`), the
`/A` listings, PMOD, and programs with `REAL` (they need a KI10 or later monitor).

### Semantic routines

**C functions registered by name**, called by the semantics interpreter with up
to ten words, with the routine's id (a slot in a table) as the `SEM` property of
its name, where the original stores the routine's address. All routines the
PDP-10 compiler registers are registered, so `syntax.imc` loads unchanged; the
ones that generate PDP-10 code are stubs that stop with an error until the IR
back end replaces them.

### Documentation as part of the work

Every module has an internals note recording what it does and where the port
differs; `docs/PLAN.md` is the plan; the original manual (`imp.rno`) is to be
rendered to Markdown. Every difference from the original is written down where
it is made.

## The plan

Full detail in [docs/PLAN.md](docs/PLAN.md). In outline:

0. Repository and provenance. **Done.**
1. **The oracle** (SIMH + TOPS-10): golden dumps and listings from the original
   compiler; recover PMOD. *Running; run output compared; dumps and listings to do.*
2. Stage 0 kernel port: housekeeping (`free`, `dir`, `stack`, `lex`), parsing
   (`graph`, `parse`, `ercor`), semantics (`dosem`, `entree`, `cotree`, `rsyn`),
   `debug`, and the declaration routines. **Done**; `syntax.imc` and `impsyn.imc`
   load.
3. Switch from PDP-10 code to pcc IR: the machine-dependent semantic routines
   (`DEWOP`, `ADDOP`, `HOOK`, `SUBCALL`, ...) become IR builders; `AMOD`, `RMOD`,
   `OMOD` go away; the C ABI is made explicit. Compile real programs.
4. Runtime library: the formatted `PRINT`, I/O, POSIX replacements for the
   TOPS-10 services.
5. Self-hosting: port the compiler's own sources (`src/`) to the 64-bit
   word; stage 0 compiles them into stage 1, which compiles them into stage 2,
   and so on until two stages are identical (each in its own directory; no stage
   overwrites another).
6. Platforms: pcc aarch64 and darwin/arm64; the C fallback; CI on every OS.

## Findings so far

Things in the original that look like bugs or surprises, reproduced here and to
be confirmed against the oracle (details in `docs/internals/`):

- Output is put out twice after a syntax error is corrected: the round in which
  the parses died has already added output to the surviving parses.
- `ERCOR` continues past a circled node without checking that the goal can
  still be reached (the parser itself does check). With a production that begins
  with its own nonterminal it can "correct" a symbol to itself forever.
- The parser's output chain has its value and link halves the other way round
  from its header comment; the code is authoritative.
- `GRAPH` and `GMATR` read the bit matrices one row past the last node. The
  checking version of `BIT` is commented out; the version in use reads whatever
  follows.
- `MATCH` calls `RMATCH` for registers and then ignores its answer, so the
  register limits of a pattern never reject a match.
- `CODEID` takes any identifier that begins with a three-letter indicator
  (`REG`, `VAR`, ...) for that indicator and loops on the rest.
- `PARTR1` indexes free storage with a value it has just loaded from free
  storage; the intended value is printed instead.

## Documentation

- [docs/PLAN.md](docs/PLAN.md): the plan, decisions, and the pcc audit
- [docs/BUILDING.md](docs/BUILDING.md): building and running
- [docs/FFI.md](docs/FFI.md): IMP72 from C and C from IMP72
- [docs/internals/c-backend.md](docs/internals/c-backend.md): the C back end
- [docs/porting/stack.md](docs/porting/stack.md): the first ported module, stage 1; then `dir.md`, `graph.md`,
  `parse.md`, `ercor.md`
- [packaging/README.md](packaging/README.md): Debian and Alpine packages
- [docs/internals/stage0-foundation.md](docs/internals/stage0-foundation.md):
  free storage, stack, directory, lexer
- [docs/internals/stage0-parser.md](docs/internals/stage0-parser.md): the syntax
  graph, parser, error corrector
- [docs/internals/stage0-semantics.md](docs/internals/stage0-semantics.md):
  semantics, code tree, RSYN, the driver
- [docs/internals/codegen-and-translation.md](docs/internals/codegen-and-translation.md):
  the code generator, the translation to pcc IR, the runtime
- [docs/porting/pcc-notes.md](docs/porting/pcc-notes.md): building and using pcc

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

The new code in this repository (`stage0/`, `tests/`, `docs/`, and what follows
from them) is under the ISC licence, in `LICENSE`. That does not cover
`decus/`, which is the original DECUS program (public domain, as above), or
`third_party/pcc`, which has its own licence.
