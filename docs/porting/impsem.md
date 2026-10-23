# impsem

`src/impsem.imp` is the machine-independent semantic routines of `decus/impsem.imc` for the 64 bit port: `NAME`, `AREG`, `AREG1`,
`REG2`, `TSTRN`, `VAL`, `SVAL`, `STACKUP`, `SWITCH`, `DECL`, `DECLARE`, the register tests the code generator uses (`HISREG`,
`PAIRFIRST`, `SUBREG`, `TMPREG`, `SAMEREG`, `REGEQ`, `REGSIM`, `REG0`, `RMATCH`), the subroutine routines (`SUBPR0`, `SUBPRM`, `SUBTMPF`,
`SUBBEG`, `SUBEND`, `NSUBP`), `REMOT`, `CALLME` and `REG2S`. `stage0/impsem_imp.c` is the C interface (`areg1`, `tmpreg`, … for
`codegen.c` and `trans.c`) and the registration of the routines by name; `stage0/impsem.c` is the all-C version stage 0 uses.

## What stays in C, and why

- **The table of semantic routines** (`sem.c`, `dsem`): a table of C function pointers, which IMP cannot call. The routines are
  registered from `impsem_imp.c` with wrappers that call the IMP ones.
- **`FCON`** needs the C library's `strtod` (a floating constant is the nearest IEEE double, `docs/LANGUAGE-DIFFERENCES.md`);
  **`LETSYN`** drives the syntax reader's C functions; the **table of names called with values** (`CCALL`, `ccall_mark`/`is_cval`)
  is read by the translator.
- **Glue** (`stage0/impglue.c`): the IMP module calls the code generator and RSYN by reference through `IMPADDCODE`, `IMPHOOK`,
  `IMPFREEZE`, `IMPANYCO`, `IMPDEWFUN`, `IMPENSTACK`, `IMPGETLIST`, `IMPNEWNAMEG`, `IMPDNAME`, `IMPCONVC`, plus three new small
  ones for string work (`IMPNEWPAR`, `IMPIMPM`, `IMPSUBTAG`). These go away as `codegen.c` is ported.

## What the port found

- **A bug in the C version**: the register flags were initialised with the decimal constant `10000L`, where the original has
  `10000B` (octal, bit 12: "not a temporary"). The IMP port took the number from the original and so disagreed with the C one. Fixed in
  `impsem.c`.
- **`J AND (NOT 777777B)` is a halfword move.** `syntax.imc` compiles `AND NOT 777777B` to `HLLZ` (keep the left half), and halves are
  32 bits here, so the register field of a `X IS REGISTER` name came out as 0 and every use of the register went through memory.
  It is the same trap as `X AND 777777B` (use `R18`); clear a field with `XOR` of its value instead (`J XOR R18(J)`). Found by
  comparing the PDP-10 code listings of the two stages (`-s A`) for a three line program.
- **A subroutine named `SWITCH` is refused by the C back end** (`ir2c: symbol is a C keyword`), which the stage 1 test (it uses the
  object code back end) does not exercise and the stage 2 test (it uses the C one) does. It is `SWTCH` in IMP.
- **How to find a difference between two stages**: link both binaries with `-Wl,--wrap=` for the routines of the module and for
  `semcall` (`sem.c`), log name, arguments and result (and the words of the stack entry the result names) from a wrapper, and diff the
  two logs with the stack addresses normalised. The first differing line is the routine whose result differs. (The stages allocate
  their first arrays in another order, so absolute free storage indices differ and must be masked.)

## Tests

The stage 1 test (IR identical for the native tests, every module of the tape and the ported sources), the stage 2 test (the C and the
objects identical, and the whole fixed point), the dumps test, and the whole suite.
