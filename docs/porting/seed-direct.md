# The seed without the PDP-10: plan

*Status: in progress (started 2026-11-03). Decided with the user: the seed (`stage0/`, C) stops emulating the 36-bit PDP-10. It reads the 64-bit syntax of the stage compilers
(`src/llvm-ir/syntax64.imc`), and its code generator is a C port of the stage's (`src/llvm-ir/*.imp`) that builds IR directly, so there is one generator in two languages and no PDP-10
instruction records, no `trans.c`, no `wide_syntax`, no `sx36`/`u36`/`L18`/`R18`.*

## Why

The seed is the trusted bootstrap. It used to run the tape's own code generator (a C port, `codegen.c`) and translate the PDP-10 instructions it made into IR (`trans.c`). The stage
compilers (`src/llvm-ir`) have a generator of their own. Two implementations of every behaviour had to agree and did not: `LOC(X)+N` (words in the seed, bytes in the stage), variable
shifts, byte pointers (`IMP72CHARPTR`), a register entry after a call (`SUBSCRIPT`), the half width. After this change the seed is the stage compiler's twin and the two are compared by
the IR they write.

## The target

```
syntax64.imc + ffi.imc  --RSYN-->  tables  --parser-->  semantic routines (C port of code*.imp, impsem.imp)
   --> record chain (CO) --> demit (C port of demit.imp) --> IR trees (stage0/ir.c) --> text --> ir2ll --> LLVM
```

* **Words are 64 bits, halves 32** (`HL`, `HR`, `HW` of `imp.h`, already so). Every word format of the kernel is that of the IMP port in `src/*.imp` and of `src/impsyn64.imc` (the
  syntax that defines `HL`, `DPROP`, `NT1`, `PLIM`, ... for those modules): that file is the contract, the C is made to match it.
* **Names in the directory**: 8 characters to a word, the first in the LOW byte, as `src/dir.imp` (not 5 seven-bit characters left justified in 36 bits). Its hash too.
* **IR**: `stage0/ir.c` builds the trees; its operator and type codes are those of `src/llvm-ir/irsyn.imc` (checked: they are equal). `demit.imp`'s helpers (`IRSYMNAME`, `IRKIND`,
  `IRSTRVAL`, `IRLABNAME`, `IRPOOLEMIT`, `IRFNKIND`, `IRRESET`, ...) are what `trans.c` did; they go into the port of `demit`.

## The test that decides: the two compilers write the same IR

`stage0/imp72stage0 -Sir` and `build/llvm-ir2/imp72stage-llvm-ir -Sir` (the LLVM IR text) are compared byte for byte over: every `src/*.imp` and `src/llvm-ir/*.imp` module,
`lib/*.imp`, `tests/native/*.imp`, `tests/oracle/*.imp`. A difference is a bug in the port (or, once understood, in the stage compiler, and then it is fixed and tested there too).
This replaces the comparisons with the real PDP-10 (see below).

## What goes, what stays

Goes: `codegen.c`, `impsem.c`, `trans.c`, `pcode.c` and the 36-bit side of `sem.c`/`semtab.c`; `wide_syntax` and `IMP72NARROW`; `sx36`, `u36`, `L18`, `R18`, `W18`, `ls36`, `MASK36`,
`MASK18`; the `/A` listing of PDP-10 instructions; `tests/stage0/test_codegen.sh` and its golden.

**Lost, and the user was told**: the comparison of the seed's dumps (`-s S|T|X|I`) and `/A` listings with the captures of the real TOPS-10 compiler (`tests/oracle/dumps.sh`,
`tools/dump-compare.py`, `tools/lst-compare.py`). They are 36 bit octal with halves of 18 bits and names of 5 characters to a word; the seed no longer makes them. The captures stay in
`tests/golden/oracle-dumps` and `oracle-lst`, with `oracle/` (the SIMH setup), for whoever wants them again; the run outputs of the original (`tests/golden/oracle/*.out`, programs run
on the real machine) are still compared (`tests/oracle/run.sh`).

## Steps (each leaves `make check` passing, or says what it retires)

1. **Kernel layouts** (files: `dir.c`, `dosem.c`, `entree.c`, `cotree.c`, `rsyn.c`, `debug.c`, `graph.c`, `ercor.c`, `free.c` as needed): every 36-bit format becomes the 64-bit format
   of the IMP port. Check: the seed loads `src/llvm-ir/syntax64.imc` + `lib/ffi.imc` and its dumps (`-s X`, `-s T`, `-s S`, `-s I`) equal those of the stage compiler for the same input.
2. **The generator** (new files `stage0/gen_*.c`, no edits of the old ones): `code.imp`, `code2.imp`, `code3.imp`, `impsem.imp`, `demit.imp` in C, registered with `dsem` under the
   names the syntax calls. Check: the IR comparison above.
3. **Switch**: `imp72s0.c` reads `syntax64.imc`; remove the old files, the 36-bit helpers, `tools/direct.sh`'s `stage0/*.o` exclusions; install `syntax64.imc`; retire the tests above.
4. **Docs and CI**: `docs/BOOTSTRAP.md`, `BUILDING.md`, `imp72stage0.1`, `README`.
