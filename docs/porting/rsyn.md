# Porting rsyn.imc (src/rsyn.imp)

The ninth module of the compiler's own sources on the 64 bit model: the bootstrap reader of the initial syntax
(`RSYN`) and the semantic routines that enter syntax statements in the graph and in the semantics array (`SYNT`,
`SYTRM`, `SYTERM`, `ADDCHAR`, `SYNTS`, `SEMANTICS`, `SYNARG`, `DEFCASE`, `CASE`, `QUOSEM`, `EQUOSE`, `SEMP`, `GETARG`,
`SEMR`, `SYNTAX`, `SEMBIT`, `REPVAL`, `VALU`). `stage0/rsyn_imp.c` has the by-value entry points that other C modules
call and the registration of the routines.

- **Thunks.** The table of semantic routines (`DSEM`) holds C functions that take the array of parameters; an IMP routine
  takes its parameters by reference. `rsyn_imp.c` has a thunk for each (`t_synt(p)` calls `impl_synt(&p[0], &p[1])`):
  a routine that reads more parameters than the call gives sees what the parameter array held before, as in the original.
- **`CASE` is `RCASE`**: `case` is a C keyword and the C back end stops on it.
- **Names.** `ADDCHAR` and `SYNTS` take apart and make names five characters to a word as the original does (`DNAME`,
  and `DIR` through `IMPDIRP`); the number sign is `43B LS 29`, a string constant being bytes here. `SYTERM` tests for a
  quoted single character with the masks of the packed word.
- **Everything that is in another module goes through the glue**, `SETSEM` as much as `GRAPH`: a call that bypasses it
  passes addresses to a by-value C function. One missed `SETSEM(0,0)` put a pointer in the first semantics word and
  looked like a fault in the case statements; the script in the log of this commit lists the calls of a module that are not
  its own or glue (`IMP` prefix).

The port needed the glue of `impglue.c` for `GRAPH`, `GMATR`, `ENTREE`, `ENTRCA`, `DOCASE`, `SETSEM`, `RESETSEM`, `SEMFX1`,
`SEMFIX`, `SEMOR`, `CKSVAL`, `GETLIST`, `ENSTACK`.
