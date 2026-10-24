# Architecture

IMP72 is a small kernel plus a language defined in data. This page is the map; the details are in `docs/internals/`
(what each part does) and `docs/porting/` (how the compiler's own sources were moved to the 64 bit model).

## The compiler

```
 source.imp ──lex──► tokens ──parse (Irons multi-track)──► parser output (production numbers, names)
                                  ▲                              │
                    syntax graph (graph)                         ▼
                    built from the syntax file by rsyn     cotree: the semantic stack reducer
                                                                 │  matches the code tree (entree)
                                                                 ▼
                                              dosem: the semantic routines of the syntax file run
                                                                 │  (impsem, code, code2, code3: chains of IR records)
                                                                 ▼
                                   demit: chains ──► pcc IR ──► pcc pass 2 (cc1) ──► assembly ──► executable
```

- **Kernel** (`decus/` is the original, `src/` its translation to 64-bit IMP, `stage0/` the seed, a port to C): the lexer
  (`lex`), the Irons parser and its error corrector (`parse`, `ercor`), the syntax graph and its connectivity matrices
  (`graph`), the directory of names (`dir`), the free storage and the stacks (`free`, `stack`), the code tree and the
  semantic stack reducer (`entree`, `cotree`), the semantics array and its interpreter (`dosem`), the bootstrap
  reader of the initial syntax (`rsyn`), the diagnostic print-outs (`debug`) and the driver program (`impm`).
- **The language** is the syntax file: `src/pccir/syntax64.imc`, the translation of `decus/syntax.imc`, statements
  `<CLASS> ::= elements ::= semantics` that `rsyn` reads first with a small notation of its own and then the parser itself
  reads, each semantics a call of the semantic routines.
- **The semantic routines** (`src/impsem`, `src/pccir/`) keep their names from the original. They build chains of records
  in free array `CO`, whose operations are those of the pcc IR machine (`docs/porting/pccir-machine.md`); registers are
  temporaries. `demit.imp` turns a chain into IR trees through `stage0/ir.c` one function at a time.
- **Back ends.** `cc1` (pcc's pass 2, built in this tree from the `moebiusv/pcc` fork) compiles the IR for amd64, aarch64
  and i386; `tools/ir2c.c` turns the seed's IR into C for any other host.
- **Runtime** (`lib/`): `PRINT` and `READ` with their formats, `FINI`, the C entry `main`, the library routines the
  generated code calls (`imp_lsh`, `imp_xct`, `imp_calli`, ...) and the FFI of `lib/ffi.imc`.

## The word

A word is 64 bits, a half is 32, addresses are byte addresses, and a string is bytes. The layouts of the compiler's own
tables are in `docs/porting/layouts.md`.

## The test oracle

The original compiler runs on TOPS-10 under SIMH (`docs/ORACLE.md`); its output is the golden output of the tests in
`tests/oracle`, and fuzzers compare programs it and this compiler run.

## Bootstrap

See `docs/BOOTSTRAP.md`.
