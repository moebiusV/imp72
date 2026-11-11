# Bootstrap

The compiler is written in IMP, so building it takes a compiler. The route has
three steps.

1. **The seed** (`stage0/`) is a port of the kernel and of the code generator to
   C99. It reads `src/syntax.imc` and `lib/ffi.imc` and compiles IMP to
   LLVM IR with `stage0/gen_*.c`, a C port of the code generator in
   `src/*.imc`. It is the one part of the compiler that is not IMP,
   apart from `stage0/impld.cpp`, and it is installed as `impstage0`. It
   makes a program with its own driver (`stage0/imps0.c`): `-S` writes LLVM IR
   text, `-c` an object, `-o` names the output, `-s` gives the switches, `-C` names
   a file of syntax. It has no `--target`, `-Sir`,
   `-I` or `-i` (`docs/BUILDING.md`, The seed and `imp`).
2. **Stage 1** is `src/*.imc` compiled by the seed
   (`tools/direct.sh`, called by `tools/llvm-ir.sh` with `DIRECT_OUT=build/llvm-ir1`),
   linked with the runtime library that the seed makes (`tools/mklib.sh`) and
   with LLVM and lld. The main program is `src/driver.imc`. The LLVM glue
   (`src/llvm.imc`) and the IR writer (`src/ir2ll.imc`, `src/ir2llx.imc`) are
   modules like the others; the seed has its own C copies (`stage0/llvm.c`,
   `tools/ir2ll.c`).
3. **Stage 2** is the same sources compiled by stage 1
   (`DIRECT_OUT=build/llvm-ir2`). It is the compiler that `make install`
   installs as `imp`, with the runtime library that stage 2 builds
   (`tools/mkrt.sh`).

Each stage is built in its own directory (`build/llvm-ir1`, `build/llvm-ir2`)
and the install takes only stage 2. `tools/direct.sh` compiles each module
separately (`IMPMODULE` names it), renames the symbols of all modules to
`impl_NAME` so that they call one another by name (`tools/imprename.imp`, which
reads and rewrites ELF and Mach-O objects), and links the objects with `cc`.

## What the tests require

- **`tests/seed/run.sh`**: the seed and stage 2 are one code generator in two
  languages, and they write byte-identical LLVM IR (`-Sir` of stage 2 against
  `-S` of the seed) for every program of `tests/native` and `tests/oracle`, for
  the runtime library as `tools/mklib.sh` compiles it, and for the modules of
  the code generator itself (`impsem`, `code`, `code2`, `code3`, `demit`, `ix`).
  A difference is a bug in one of the two generators.
- **`tests/direct/stage2.sh`**: a compiler built by the seed and one built by
  that compiler (`build/direct`, `build/direct2`) write the same IR for the
  native tests, the oracle programs and every module of `src/`.
- **`tests/direct/ir2ll.sh`**: the IMP IR writer and `tools/ir2ll.c` write the
  same LLVM IR, errors included, for the IR of those programs and for random IR
  from `tools/ir2ll-fuzz.py`, for 32- and 64-bit pointers.
- **`tests/fuzz64/run.sh`**: the seed and `imp` compile random programs and
  print what an independent reference computes.

## What stays outside IMP

- the seed (`stage0/*.c`, and its unit tests in `tests/stage0`), which stays C
  so that a compiler exists before any IMP compiler does;
- `stage0/impld.cpp`, 47 lines of C++ that give lld's drivers C names, and
  `stage0/lvtriple.cpp`, which asks `llvm::Triple` about a `--target` triple;
- the C programs of `tests/native/*_main.c`, which are C calling IMP.

Everything else, the runtime library included, is IMP.
