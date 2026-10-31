# The tape, as IMP72 reads it

`decus/` is the DECUS 10-198 tape as received, and stays so (its checksums are in `decus/SHA256SUMS`). The 22 `.imc` files here are those files translated for IMP72 by `tools/tape.sh`
(`tools/imp-assign.py`), and nothing else is different: **assignment is `:=` and not `_`**, so that `_` is a letter of names and every name is a valid C name. Every `_` that is code
became `:=`; in a syntax statement the token is quoted (`':='`; the lexer reads `:=` as one token); comments, text constants and
the quoted character `'_'` were left alone.

Do not edit these files: change `decus/` (never) or the translator, and run `tools/tape.sh`. `tests/tape/run.sh`, in `make check`, fails if they are not what the translation gives.
The seed reads `tape/syntax.imc`; `src/pccir/syntax64.imc` is the 64 bit language, a translation of it.
