# tape64: where the branch stands (checkpoint)

Branch `tape64`. The last commit that passes everything (`tests/direct/run.sh`, `tests/direct/stage2.sh`, the oracle goldens) is `3f6cde0`
(the driver program, `src/impm.imp`, is the tape's). The commit after it (`6ed28ea`, the free storage routines under the tape's names, `free_imp.c`
and `impglue.c` gone) builds, but the stage 1 compiler HANGS when it compiles `src/pccir/impsem.imp` (the step "direct3" of `tests/direct/run.sh`),
so it is not verified: start there (suspects: FRELOT's `LOC(A)` of the relocation registry, FNEW/FALLOT, the removed FFIXER call-after-move).

Done on the branch: words are 64 bit with fields of their own (code tree, semantics array, stack entries); names are eight characters to a word; the
seed folds the tape's constants in 64 bits (IMP72WIDE); the IMP modules call one another by the tape's names; the semantic routines are registered by
the tape (IMPSEM, CODEI, DSEM); the driver, LEXIN, PSTATS, ERROR, MSG are IMP; EXECUTE does an instruction of the pcc IR machine.
