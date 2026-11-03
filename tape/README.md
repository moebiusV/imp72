# The tape, as IMP72 reads it

`decus/` is the DECUS 10-198 tape as received, and stays so (its checksums are in `decus/SHA256SUMS`). The 22 `.imc` files here are those files translated for IMP72 by `tools/tape.sh`
(`tools/imp-assign.imp`, an IMP program the compiler builds (`tools/imp-assign.sh`)), and nothing else is different: **assignment is `:=` and not `_`**, so that `_` is a letter of names and every name is a valid C name. Every `_` that is code
became `:=`; in a syntax statement the token is quoted (`':='`; the lexer reads `:=` as one token); comments, text constants and
the quoted character `'_'` were left alone.

Do not edit these files: change `decus/` (never) or the translator, and run `tools/tape.sh`. `tests/tape/run.sh`, in `make check`, fails if they are not what the translation gives.
No compiler reads these files any more: they are the tape's own text (words of 36 bits, halves of 18, the PDP-10 code generator) with the one change above, kept as the reference for the audit
(`docs/porting/tape-names.md`) and for restoring routines. The 64 bit translation is `src/` (`src/*.imp`, `src/llvm-ir/*.imp`), with the syntax in `src/impsyn64.imc` and `src/llvm-ir/syntax64.imc`; the seed and
stage 2 both read `src/llvm-ir/syntax64.imc`.
