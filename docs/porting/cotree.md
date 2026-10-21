# Porting cotree.imc (src/cotree.imp)

The seventh module of the compiler's own sources on the 64 bit model: `COTREE`, the semantic stack reducer. The rest of
`cotree.imc` (`CFUDGE`, `CODENT`, `CKDONE`, `MAXWELL`, `MAXEND`, `SETPRI`, `SAMEAS`, `PSTACK`) stays C in
`stage0/cotree_imp.c`: the three semantic routines are entered in the table of `DSEM` by address, `SAMEAS` is called from
the code tree, and the others are small. The state they share with `COTREE` (`cs`, `ncs`, `entries`, `prio`, `demon`, `maxie`,
`nmaxie`, `nc`) is declared `COMMON` in the IMP module. `COTREE` registers the three routines through `impglue.c`.

The structure follows `cotree.c` (the labels and jumps of the original, with the 36 bit stack entries):

- an entry is three 36 bit words, written with `SX36` and read with `L18`/`R18`; the node saved when a path is followed is
  `SX36(NODE)` with bit 35 as the "read as the original production" mark, and read back with `R18`;
- the masks of the conditions of a semantic routine (`PLIES`) are 36 bit, so the test of its bits uses `LS36(1,N)`, which is 0
  for `N` of 36 or more, and `U36(I) RS 1`;
- the moves done by `BLT` in the original are loops; the items inserted by quoted semantics are copied forward, which is
  safe because the source ends where the destination begins;
- the timing code (`CALLI(27B,0)` around `DOSEM`) is left out, as in `cotree.c`.

Two things came up compiling it. `((A)=0)=>` in an `=>` chain is "syntactic ambiguity" (write `A=0=>`), as the
original does; and a variable whose lower case name is a C keyword (`CONST`, `CHAR`) stops the C back end (`ir2c` says so
and stops), so `entree.imp` calls them `KONV` and `CHR`.
