# Bootstrap

The compiler is written in IMP, so building it takes a compiler. The route:

1. **The seed** (`stage0/`) is a port of the compiler to C99, and it stays C. It reads `src/llvm-ir/syntax64.imc` (the tape's syntax with words of 64 bits and no rules of
   halves of 18 bits) and compiles IMP to the IR with `stage0/gen_*.c`, a C port of the code generator of `src/llvm-ir/*.imp`: the seed and the stage compilers are one generator in
   two languages, and write the same IR byte for byte (the test is in `docs/porting/seed-direct.md`). The seed has no PDP-10 in it: no 36-bit words, no instruction records.
2. **Stage 1** is the compiler's own sources, `src/*.imp` and `src/llvm-ir/*.imp`, compiled by the seed
   (`tools/direct.sh`) and linked with the runtime library (the LLVM glue is `src/llvm.imp` and the IR writer `src/ir2ll.imp` and `src/ir2llx.imp`, modules like the rest; the seed has its own C copies); the main program is
   `src/driver.imp`.
3. **Stage 2** is the same sources compiled by stage 1 (`tools/llvm-ir.sh`; the name is the IR's old one). `tests/direct/stage2.sh` requires stage 2 to
   write the same IR as stage 1 for the native tests, the oracle programs and every module of the sources. Stage 2 is
   installed.

Each stage is built in its own directory (`build/direct`, `build/llvm-ir1`, `build/llvm-ir2`), and nothing is installed from a
stage that has not passed its test.

What stage 1 and stage 2 still take from C: the glue to LLVM's C API (`stage0/llvmorc.c`) and the little C
of `lib/imp72sys.c` and `lib/imp72main.c`. Everything else is IMP, and these are to become IMP too
(`docs/porting/c-to-imp.md`); only the seed stays C.
