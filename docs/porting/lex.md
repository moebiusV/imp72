# Porting lex.imc (src/lex.imp)

The tenth module of the compiler's own sources on the 64 bit model: the lexical analyser (`LEX`, `NEXTCHAR`, `PLINE1`,
`PLINE` and the state of the line). The original reads through TOPS-10 buffers (`OPENIN`, `INPUT`, `BHEAD`) and keeps the line and the
token as packed byte strings; here the file is read whole by C (`stage0/lex_imp.c`: `lexin` loads it; `lex` and `pline1`
are the by-value entry points) and the IMP module asks for the next byte through `IMPSRCGETC`. The line and the token are
free arrays with one character in a word (`FADD` fills them), and are turned into names and into printed lines by
`IMPDIRW`, `IMPPLINEW` and `IMPPLINELW` of `impglue.c`.

- **The glue gets the index of an array**, not an address: an array is a free storage index, and the argument by reference
  is the variable that holds it. The first version passed it as if it were a pointer and read garbage as the first token.
- The logic is that of `lex.c` (and of the original `LEX`): letters and digits, a quoted text with `''` for an apostrophe, `!text!`,
  `#comment#`, and one other character; the lookahead (`SKIP`, `CSV`) and the paren depth of the line (`PCT`).
- `GETTY`, `READCMD`, `PUTN`, `PUTO`, `PUTSW` and `LEXSAV`/`LEXRES` (the TOPS-10 command line) are not ported, as in `lex.c`.
- A comment of an IMP source must not hold `#` or an apostrophe.
