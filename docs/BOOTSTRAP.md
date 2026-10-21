# Bootstrap

The goal is a compiler written in IMP72 that compiles itself. The route:

1. **Stage 0** is the compiler's kernel in C (`stage0/`), strict C99, buildable with pcc itself. It reads `decus/syntax.imc`
   and compiles IMP72 programs.
2. **The sources are ported** to the 64 bit model one module at a time (`src/*.imp`, notes in `docs/porting/`). A ported module is
   compiled by stage 0 (`imp72stage0`) and links in place of its C counterpart.
3. **Stage 1** (`tools/stage1.sh` builds `build/stage1/imp72s1`) is stage 0 with the ported modules replaced by their IMP
   versions compiled by stage 0: `stack`, `dir`, `graph`, `parse`, `ercor`, `entree`, `cotree`, `dosem`, `rsyn`, `lex`.
4. **The test** (`tests/self/stage1.sh`) requires stage 1 to write the same IR as stage 0 for every program: the native tests,
   every module of the tape, and the ported sources themselves. That the stage 1 compiler compiles its own modules to the
   IR stage 0 makes of them is the fixed point for the part that is ported (equal IR is equal objects: the back end is the
   same).
5. **What is not ported** stays in C and is the compiler's back end, as planned: the semantic routines of the PDP-10 code
   generator (`*-pdp10.c`), the translator to IR (`trans.c`, `ir.c`), the free storage manager, the debug printers, the driver.
   The IMP modules reach them through `stage0/impglue.c` (arguments by reference; `tools/glue-lint.py` checks it).

Each stage is built in its own directory (`build/stage1/`), and nothing is installed from a stage that has not passed its test.
A port that fails does not replace a module: `MODULES='stack dir' tools/stage1.sh` builds a stage 1 with only those.

To port another module: copy the C or IMP source, write it against the helpers of `src/impsyn64.imc`, give it a by-value C
interface (`stage0/NAME_imp.c`) and glue for what it calls, add it to `modules=` in `tools/stage1.sh`, and make
`tests/self/stage1.sh` pass. `docs/porting/` has what went wrong in each port.
