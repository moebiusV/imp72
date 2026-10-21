# Architecture

IMP72 is a small kernel plus a language defined in data. This page is the map; the details are in `docs/internals/`
(what each part does) and `docs/porting/` (how the compiler's own sources were moved to the 64 bit model).

## The compiler

```
 source.imp ──lex──► tokens ──parse (Irons multi-track)──► parser output (production numbers, names)
                                  ▲                              │
                    syntax graph (graph)                         ▼
                    built from syntax.imc by rsyn          cotree: the semantic stack reducer
                                                                 │  matches the code tree (entree)
                                                                 ▼
                                              dosem: the semantic routines of syntax.imc run
                                                                 │  (sem, impsem, codegen: PDP-10 code chains)
                                                                 ▼
                                   trans: chains ──► IR ──► pcc pass 2 (cc1) ──► assembly ──► executable
                                                       └───► ir2c ──► C ──► the host's cc
```

- **Kernel** (`decus/` is the original, `stage0/` the C port, `src/` the 64 bit IMP port of part of it): the lexer
  (`lex`), the Irons parser and its error corrector (`parse`, `ercor`), the syntax graph and its connectivity matrices
  (`graph`), the directory of names (`dir`), the free storage and the stacks (`free`, `stack`), the code tree and the
  semantic stack reducer (`entree`, `cotree`), the semantics array and its interpreter (`dosem`) and the bootstrap
  reader of the initial syntax (`rsyn`).
- **The language** is `decus/syntax.imc`, unmodified: statements `<CLASS> ::= elements ::= semantics` that `rsyn` reads
  first with a small notation of its own and then the parser itself reads, each semantics a call of the semantic routines.
- **The semantic routines** (`sem-pdp10.c`, `impsem-pdp10.c`, `codegen-pdp10.c`) are the original's, ported: they make PDP-10
  code in chains of words, the original's format. Registers are virtual (`R65`); `trans.c` translates the chains to pcc IR
  text, one function at a time, taking the registers as temporaries. `pcode.c` lists a chain as PDP-10 assembly (`-s A`).
- **Back ends.** `cc1` (pcc's pass 2, built in this tree from the `moebiusv/pcc` fork with `--enable-twopass`) compiles the
  IR for amd64, aarch64, i386 and armv7; `tools/ir2c.c` turns the same IR into C for any other host.
- **Runtime** (`lib/`): `PRINT` and `READ` with their formats (a port of the original's PMOD, read off its code on the
  oracle), `FINI`, the C entry `main`, and the FFI of `lib/ffi.imc`.
- **Driver** (`stage0/imp72s0.c`): `imp72` (also `imp72stage0`) loads the syntax, compiles the files, and runs the back end.

## The word

A word is 64 bits and addresses are byte addresses (`docs/README` "The word"); the halves of a word in the compiler's
tables are 32 bits; the semantics (`SEMS`, the code tree, the stacks) keep the original's 36 bit formats in a 64 bit word,
sign extended (`docs/porting/helpers.md`).

## The test oracle

The original compiler runs on TOPS-10 under SIMH (`docs/ORACLE.md`); its output is the golden output of the tests in
`tests/oracle`, and fuzzers compare programs it and this compiler run.

## Bootstrap

See `docs/BOOTSTRAP.md`.
