# Architecture

IMP72 is a small kernel plus a language defined in data. (IMP stands for Irons
Meta Processor, named for Ned Irons as LISP is for its List Processor.) This page is the map.
`docs/internals/` describes what each kernel module does, and `docs/porting/`
describes the code generator's machine, the layouts of the compiler's tables
and the modules compared with the original.

## The compiler

```
 source.imp --lex--> tokens --parse (Irons multi-track)--> production numbers, names
                                 ^                              |
                   syntax graph (graph)                         v
                   built from the syntax file by rsyn     cotree: the semantic stack reducer
                                                                |  matches the code tree (entree)
                                                                v
                                             dosem: the semantic routines named in the syntax file
                                                                |  (impsem, code, code2, code3: chains of records)
                                                                v
              demit: chains --> IR trees (ix) --> IR file --> ir2ll --> LLVM IR --> LLVM, in the process
                                                                          |--> object file --> lld --> executable
                                                                          '--> assembly, LLVM IR text (-S, -Sir)
```

The kernel modules are `lex`, `parse`, `ercor`, `graph`, `dir`, `free`, `stack`,
`entree`, `cotree`, `dosem`, `rsyn` and `debug`. `impm` is the kernel's driver
(`IMPK`), `image` saves and restores a compiler together with the language it
has learned, and `driver` is the command line of `imp`. All are `src/*.imc`,
the 64-bit IMP translation of the original modules in `decus/`.

- **The language** is the syntax file `src/syntax.imc` (the
  translation of `decus/syntax.imc`) and the extension `lib/ffi.imc`. Both
  compilers read them at start. The part up to the first `%` is read by `rsyn`
  in its own notation; the parser reads the rest. Each statement is
  `<CLASS> ::= elements ::= semantics`, and the semantics are calls of semantic
  routines by name.
- **The semantic routines** are the code generator. They keep the original's
  names (`DEWOP`, `FETCH`, `STORE`, `SUBCALL`, ...) and are registered by name
  with `DSEM`. They build chains of records in free array `CO`. The operations
  of a record are those of the IR machine
  (`docs/porting/llvm-ir-machine.md`); registers are IR temporaries.
- **`demit`** reads the chains and writes IR trees. In the stage compilers the
  tree builders and the IR file writer are `src/ix.imc`; in the seed they
  are `stage0/ir.c`. The IR file is text, one tree node a line.
- **`ir2ll`** (`src/ir2ll.imc`, `src/ir2llx.imc`; the seed's copy is
  `tools/ir2ll.c`) reads the IR file and writes a module of LLVM IR as text.
  The IR is lowered for one machine model: the arguments of a call are pointers
  to the values and the word is 64 bits. The pointer size (32 or 64 bits) is the
  writer's only parameter, so the output does not depend on the host otherwise.
- **`llvm`** (`src/llvm.imc`; the seed's copy is `stage0/llvm.c`) calls the
  LLVM C API through `CCALL`. It parses the module, optimizes it at `default<O1>`
  and emits an object file, assembly or IR text. The module `llvmgen`, written by `tools/llvm.sh imp` for the LLVM at hand, holds what the LLVM headers decide.
- **`driver`** links an executable from the object, the objects named on the
  command line, the C library's start files and `libimp.a`, with lld
  (`impld`, `stage0/impld.cpp`), `ld.lld` or the system's `ld`. The loader and
  C library of this machine are those the compiler itself runs with
  (`lib/imphost*.iml`), for any architecture; a target's come from the tables
  of `linktab` (docs/BUILDING.md, The linker and its flags).

## The runtime library

`lib/*.iml` is `libimp.a`:

| file | contents |
|---|---|
| `impio.iml` | `PRINT`, `READ`, files, `FINI` |
| `imprt.iml` | `CALLI`, `BLT`, `IFIX`, shifts and rotations by a variable count, byte pointers, `EXECUTE` |
| `impsys.iml` | the standard files, the arena's address space, argument vector access |
| `impmain.iml` | the C `main`, a member of its own so that a program with a `main` of its own does not link it |
| `imphost.iml`, `imphostx-*.iml` | what the link needs to know of the host |
| `impvars.iml`, `impvarsx-*.iml` | the table of the compiler's own variables, for images |

`tools/sysconst.sh` writes `sysgen.iml`, the constants of the target system's
headers (`mmap` flags, directory entry layout, names of the standard files).
`lib/imp.h` is the C header for code that shares data with IMP programs.

## The word

A word is 64 bits, a half is 32, addresses are byte addresses, and a string is
bytes. `docs/porting/layouts.md` has the layouts of the compiler's tables.

## The code generator in two languages

The seed (`stage0/`, C) and the stage compilers (IMP) contain one code
generator twice:

| IMP | C | contents |
|---|---|---|
| `src/impsem.imc` | `stage0/gen_impsem.c` | registers, subroutines, declarations, lists, names |
| `src/code.imc` | `stage0/gen_code.c` | `UNPCK`, `FETCH`, `DEWOP`, `STORE`, `COPYCO`, `SUBARG`, ... |
| `src/code2.imc` | `stage0/gen_code2.c` | `ADDCODE`, `HOOK`, `CONOP`, `SUBRCALL`, `FLOAT`, `PRINPAR`, ... |
| `src/code3.imc` | `stage0/gen_code3.c` | `SUBCALL`, `SUBSCRIPT`, `BYTEP`, `PCODE` |
| `src/demit.imc`, `ix.imc` | `stage0/gen_demit.c`, `stage0/ir.c` | chains to IR trees, and the tree builders |
| `src/ir2ll.imc`, `src/ir2llx.imc` | `tools/ir2ll.c` | IR file to LLVM IR |
| `src/llvm.imc` | `stage0/llvm.c` | the LLVM C API glue |

The two write the same LLVM IR for the same program, and `tests/seed` compares
them. To add or change code generation, change both:

1. Write the semantic routine in the right `src/*.imc` file and
   register it with `DSEM('NAME',ROUTINE)` in that file's initializer
   (`IMPSEM`, `CODEI`, `CODE2I`, `CODE3I`).
2. Write the same routine in the matching `stage0/gen_*.c` file, with a
   wrapper `static word w_name(const word *p)` and a `dsem("NAME", w_name)`
   line in that file's `gen_*_dsems`. `gen.h` lists the IMP name to C name
   mapping.
3. Reference it from a rule of `src/syntax.imc` (or `lib/ffi.imc`).
4. An IR operator or type that the generator needs goes into `src/irsyn.imc`, `stage0/ir.h`,
   `src/ir2ll.imc` or `src/ir2llx.imc`, and `tools/ir2ll.c`.
5. Run `sh tests/seed/run.sh`, `sh tests/direct/stage2.sh` and
   `sh tests/direct/ir2ll.sh`.

Things that make the C port differ in form from the IMP, and that it has to
reproduce: the arguments of a call are evaluated right to left; a `FOR` body
runs once even when the end is below the start; locals are static; a
relation used as a value is -1 for true.

Both compilers also read `IMPCHARPTR` (a byte pointer held in a variable is
a pointer to characters; the runtime library is compiled that way) and
`IMPMODULE` (the name of the compiler module being compiled, so that its
variables are listed for images).

## Images

`NAME_/U` (or `/V`) in the commands of `imp -i` saves the compiler with the
language it has learned to `NAME.img`; `-I NAME.img` starts from it.
`src/image.imc` writes the free storage (directory, syntax graph, code tree,
semantics array), the variables of the compiler's modules, listed in the
section `impvars` of the program, and the table of semantic routines by name.
An image is valid for the compiler build that made it. Its format is version 6.
The free storage is restored at any address: the arena is asked for at the
address it was saved at, and when the system gives another, the loader adds the
difference of the bases to the five byte pointers of the lexer that point into it
(`docs/BUILDING.md`, Diagnostics of the free storage and the images).

## Tools and tests

The tools of `tools/` that process text are IMP programs, built on first use by
the compiler of the tree and run by `tools/imptool.sh NAME ARGS`. The fuzzers
(`tools/imp-fuzz*.py`, `tools/ir2ll-fuzz.py`, `tools/fuzz64-reduce.py`) are
Python, so that their reference does not come from the compiler under test.
`docs/BUILDING.md` lists the tests that `make check` runs.

## The oracle

The original compiler runs on TOPS-10 under SIMH (`docs/ORACLE.md`). Its
output for the programs of `tests/oracle/LIST` is in `tests/golden/oracle`, and
fuzzers compare programs it and this compiler run.

## Bootstrap

See `docs/BOOTSTRAP.md`.
