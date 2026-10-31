# Bootstrap

The compiler is written in IMP, so building it takes a compiler. The route:

1. **The seed** (`stage0/`) is a port of the compiler to C99, and it stays C. It reads the `tape/syntax.imc` (the original, `decus/syntax.imc`, with `:=` for `_`)
   and compiles IMP to the IR (its code generator makes PDP-10 code chains that `trans.c` translates).
2. **Stage 1** is the compiler's own sources, `src/*.imp` and `src/llvm-ir/*.imp`, compiled by the seed
   (`tools/direct.sh`) and linked with the runtime library (the LLVM glue is `src/llvm.imp` and the IR writer `src/ir2ll.imp` and `src/ir2llx.imp`, modules like the rest; the seed has its own C copies); the main program is
   `src/driver.imp`. The seed folds constants in 64 bits, and compiles shifts, `AND` masks and bit-fields as 64 bit words (`IMP72NARROW=1` gives the tape's 36 bit rules back, for its own modules).
3. **Stage 2** is the same sources compiled by stage 1 (`tools/llvm-ir.sh`; the name is the IR's old one). `tests/direct/stage2.sh` requires stage 2 to
   write the same IR as stage 1 for the native tests, the oracle programs and every module of the sources. Stage 2 is
   installed.

Each stage is built in its own directory (`build/direct`, `build/llvm-ir1`, `build/llvm-ir2`), and nothing is installed from a
stage that has not passed its test.

What stage 1 and stage 2 still take from C: the glue to LLVM's C API (`stage0/llvmorc.c`) and the little C
of `lib/imp72sys.c` and `lib/imp72main.c`. Everything else is IMP, and these are to become IMP too
(`docs/porting/c-to-imp.md`); only the seed stays C.
