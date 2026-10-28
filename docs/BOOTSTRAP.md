# Bootstrap

The compiler is written in IMP, so building it takes a compiler. The route:

1. **The seed** (`stage0/`) is a port of the compiler to C99, buildable with pcc itself. It reads the original `decus/syntax.imc`
   and compiles IMP to pcc IR (its code generator makes PDP-10 code chains that `trans.c` translates).
2. **Stage 1** is the compiler's own sources, `src/*.imp` and `src/pccir/*.imp`, compiled by the seed
   (`tools/direct.sh`) and linked with the LLVM glue (`stage0/llvmorc.c`, `tools/ir2ll.c`) and the runtime library; the main program is
   `src/driver.imp`. Constants of the sources are folded in 64 bits by the seed (`IMP72WIDE=1`).
3. **Stage 2** is the same sources compiled by stage 1 (`tools/pccir.sh`). `tests/direct/stage2.sh` requires stage 2 to
   write the same IR as stage 1 for the native tests, the oracle programs and every module of the sources. Stage 2 is
   installed.

Each stage is built in its own directory (`build/direct`, `build/pccir1`, `build/pccir2`), and nothing is installed from a
stage that has not passed its test.

What the compiler still takes from C: the builder of the pcc IR (`ir.c`), the table of semantic routines (it holds
addresses), `FCON`, `LETSYN`, `CVAL`, the `/A` listing, and the operating system's part (`sys.c`: arguments, the listing
file, the terminal). The rest is IMP.
