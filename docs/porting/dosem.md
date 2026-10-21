# Porting dosem.imc (src/dosem.imp)

The eighth module of the compiler's own sources on the 64 bit model: the semantics array (`SEMS`, `PROSEM`) and its
interpreter. `DOSEM`, `SEMFIX`, `SEMFX1`, `DOCASE`, `ENPROD`, `MATCHP`, `NOTEPROD`, `PLIES`, `SNODEN`, `ENSTACK`, `RESETSEM`,
`SETSEM`, `SNARG`, `SEMGET`, `SEMED`, `SEMOR` and `CKSVAL` are IMP. `PAR`, the table entry of `STACK`
and `SEMPER` stay C (`stage0/dosem_imp.c`, which also has the by-value entry points); the state the C side reads
(`sems`, `nsems`, `prosem`, `nprosem`, `scratch`, `casep`) is `COMMON`.

- **The call of a semantic routine.** The original puts the routine's address in a `JSA` instruction and executes it
  (`JSA_SVAL OR JSA AND NOT 777777B; GO TO JSA`). Here the routines are C functions in the table of `DSEM`,
  and `DOSEM` calls `IMPSEMCALL(SVAL,PARS)` (`impglue.c`), which passes the array of up to ten parameters. `SETSEM`
  has `PAR` and `STACK` entered in the table through `IMPDSEMS2`.
- **36 bit words.** `SEMS`, `PROSEM` and the stack entries are 36 bit numbers (`SX36` to store, `U36` to take bits, `L18`,
  `R18`); the constants with a top bit are written with a leading zero when they are masks (`0776000777777B`) and
  without when they are the sign extension of a 30 bit field (`SVAL OR 770000000000B`, read as negative).
- **No `RS 18`.** The 18 bit halves are taken with `L18`/`R18`; the fields are made with `(X LS 9) LS 9`.
- **A glue call that may move free storage is not an argument** of `FADD` or `FREES`: its result goes into a temporary first,
  since the address of the store is computed before the call and `FB` changes (`TYPE2`).
- Recursion: all the variables are module variables, as in the original; `dosem.c` had automatic variables. No program
  of the tests re-enters `DOSEM`.

Found on the way: a sed that renamed `CHAR` to `CHR` in `entree.imp` and missed `CHAR_0` (an underscore is a word
character to the regex, an assignment to IMP) left the two names in use and wrecked free storage in a way that looked
like the new module. `IMP_SLOG=1` logs every semantic routine call (`sem-pdp10.c`), as `IMP_ELOG` does the tree builder's.
