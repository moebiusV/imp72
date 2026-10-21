# Stage 0, part 3: semantics, the code tree, RSYN and the driver

Ports of `dosem.imc`, `entree.imc`, `cotree.imc` and `rsyn.imc` (and the bits
of `impsem.imc` and `code2.imc` that reading syntax needs) to `stage0/dosem.c`,
`entree.c`, `cotree.c`, `rsyn.c` and `sem-pdp10.c`, with `debug.c` extended with
`TPRIN1`, `SEMPR1` and `PSTAC1`, and a driver, `imp72s0`.

**What runs:** `stage0/imp72s0 decus/syntax.imc` reads the whole syntax file of
the IMP72 compiler: RSYN reads the syntax for syntax with the lexer alone,
then the parser, the code tree and the semantic stack, driven by the productions
just defined, read the remaining 150-odd syntax statements. It ends with the
syntax graph, the semantics array and the code tree built, and 177
productions entered (the number of syntax statements in the file). `make
check` runs it (`tests/stage0/test_syntax.sh`).

## The data

`SEMS`, `PROSEM`, the code tree's stuff words, the code stack and the semantic
stack keep the **36 bit formats of the original** (`imp.h`: `u36`, `sx36`,
`L18`, `R18`, `W18`, `ls36`), held sign-extended in a 64 bit `word`, so that
`debug.c` can print the same octal as the original compiler. The syntax graph
(see part 2) and the code tree's links word do not, since nothing prints
them raw.

## Semantic routines

The original calls a routine by its address, taken from the 'SEM' property of
its name. Here the property is the routine's id (1 + its slot in `sem-pdp10.c`'s
table). A routine is a function `word fn(const word *p)`; `p` is the array of
arguments (`PARS` of the original, so that arguments beyond those passed are
left over from earlier calls, as there).

`sem-pdp10.c` registers every routine the PDP-10 compiler registers with `DSEM`, so
that `SEMR` accepts the names in `syntax.imc`; the ones that generate code
(`code.imc`, `code2.imc`, `code3.imc`, most of `impsem.imc`) are *not ported*
and stop with `SEMANTIC ROUTINE X IS NOT YET PORTED` if called. Reading syntax
statements calls only `HOOK`, `ASSEMC`, `NEWLIST`, `ENLIST`, `GETLIST` among
them, for entries without code attached (`HOOK` and `ASSEMC` stop if code is
attached). They are replaced when the compiler switches to pcc IR.

## Differences and decisions

- `CODEID` (taking identifiers like `Q0REG0115` apart) is a literal port of the
  state machine, including that it takes any name that starts with a
  three letter indicator for that indicator (`REGISTER` would be `REG`
  followed by garbage); the original loops forever on such a name.
- `MATCH` does not reject a register outside the limits of the pattern: the
  original calls `RMATCH` and then `MEX` turns its 0 into a match. The call is
  dropped.
- `ONEWORD` is 0 for an entry without code, so a `WRD` pattern matches only
  entries with code, as in the original.
- `MAXWELL(N)` makes N+1 phony entries (`FOR J FROM N`).
- In `TPRIN1` the width of the bit matrix is that of the original's 36 bit
  words for the same columns, at least 5 words.

## Not yet checked against the oracle

The 177 productions and the dumps have not been compared with the original
compiler's. The two `PRODUCTION DUPLICATES` advisories on `syntax.imc` are
from the production check in `GRAPH`; whether the original prints them is part
of that comparison.

## Declarations (`impsem-pdp10.c`)

`impsyn.imc` runs IMP declarations (`ALF IS COMMON;`), so the declaration
routines of `impsem.imc` are ported: `NAME`, `TSTRN`, `AREG`, `AREG1`, `REG2`,
`DECL`, `DECLARE`, `VAL`, `SVAL`, `STACKUP`, `SWITCH`, `NSUBP`. `imp72s0 syntax.imc
impsyn.imc` loads (215 productions); `tests/stage0/test_decl.c` checks the
routines directly. Register declarations call `DEWFUN`, which generates
code and is still a stub.

## Next

The semantic routines that generate code (`code*.imc`, the rest of
`impsem.imc`), which the switch to pcc IR replaces.
