# Before v1.0.0: eight questions, studied and not implemented

The README (step 6 of the road) asks for a study of these, with a recommendation and the effect on the code of each. Nothing here is implemented and the syntax of IMP72 does not change until you have
decided. The recommendations are mine. "Looked at" means I read the code named; "not measured" means I have not counted the places a change would touch.

## 1. UTF-8 symbols for operators (`≤ ≥ ≠ ← ∧ ∨ ¬`)

*Now.* `src/lex.imp` makes every byte of 128 and up a letter of names (so that a name may be UTF-8 text). A symbol in an operator position would therefore be read as the start of a name.
*Recommendation.* Not as new syntax: as spellings of the tape's operators, in a syntax extension file loaded after `syntax64.imc` (`<ATOM> ::= '≤' ::= ...` repeating the rule of `LE`), the way `ffi.imc` extends the language. Names keep all the bytes of 128 and up except those a loaded extension makes into symbols.
*Effect.* The lexer must test the table of symbols before it takes a high byte for a letter (one routine, `lex.imp`); the rest is a file of rules. The risk is a name that used one of those characters.

## 2. C's syntax for strings and characters (`"text\n"`, `'a'`)

*Now.* `'text'` is the string, two apostrophes are one inside it, and `!name!` is an exact identifier.
*Recommendation.* Add `"..."` with C's escapes (`\n \t \\ \" \0 \xHH`, and `\u` as UTF-8 bytes) as a second spelling of the same string constant; keep `'...'`. A one-character `'a'` stays a string of one character: giving it the value of a character would break `STG 0,'A'` and every program of the tape.
*Effect.* The quoting code of `lex.imp` and the rule for the constant in `syntax64.imc`; no change in the code generators, which see a constant either way.

## 3. The tape's channel I/O (`INIT`, `LOOKUP`, `ENTER`, `INPUT`, `OUTPUT`, the buffer header)

*(Rule from the user: what comes back from the tape comes back working, in the new idiom, not as a stub. `RLEAS`, which released a channel, would be the `fclose` of this study.)*

*Now.* `PRINT`, `READ` and `PRINT FILE` are in `lib/imp72io.imp`. The channel operations (`INIT`, `LOOKUP`, …) are not in the tape's `syntax.imc` or `impsyn.imc` (I searched for them and found none): the original reaches them through its libraries, which are not ported. So "keeping them" means a new library part and new declarations, not enabling something that is there.
*Recommendation.* Keep the tape's statements with their meaning, over `fopen`/`getc`/`putc`: a channel is an index into a table of `FILE *`, the buffer header is a small record the program may read (count, pointer) kept in step by the library. Programs of the tape that call them would then compile; the buffer is a fiction kept for them.
*Effect.* A new part of the library (about the size of `imp72io.imp`'s file part) and the statements in an extension file. Not measured: how many routines of `decus/` use the header words directly, which is what decides whether the fiction is cheap.

## 4. The type a routine returns, and C's return types

*Now.* An IMP routine returns a 64-bit word; `CCALL` takes the result as a C long (widened on i386, `CRET` in `ix.imp`); a pointer from C is sign extended on 32-bit machines, which the library masks by hand (`SCMASK`).
*Recommendation.* Declare it: `EXTERNAL !name! RETURNS INT | LONG | PTR | DOUBLE | VOID` in `ffi.imc`, with `LONG` the default (today's behaviour). `INT` sign extends, `PTR` zero extends, `DOUBLE` is read as a `REAL`. The hand-written masks of `imp72sys.imp` go away.
*Effect.* `ffi.imc`, `IXCALL` and the call lowering in `src/ir2ll.imp`; the existing declarations keep working. This is the study with the clearest gain: the sign-extended pointers and the widened results that had to be fixed by hand in the library on the 32-bit machines this session came from here.

## 5. NUL-terminated strings against strings that carry their length

*Now.* String constants are NUL-terminated bytes, so they pass to C unchanged; `STG` and `RTLPUT` count to the NUL.
*Decision (the user, 2026-11-03): strings carry their length, and they are ALSO NUL-terminated, always.* The invariant, for every string, a constant or one built at run time:
one word before the first byte holds the length in bytes (the NUL not counted), the bytes follow, and a zero byte follows them; one address (that of the first byte) names the string and serves C
unchanged. Any routine that makes or changes a string keeps both: the length and the final zero. The cost is accepted.
*Effect.* The constant emitter (the length word and the zero), `STG`/`PRINT` (use the length, not a scan), the library's text routines (`RTLPUT`, the file items, `MKNAME`, the lexer's token buffers where they
are strings), the `LOC('...')` of a constant (the address of the first byte, as now), and every place that builds a string by `snprintf` or by hand (the length must be set after). `CCALL` of C strings is
unchanged: the zero is there. A C string coming back from C has no length word: it needs a way to be wrapped (a library routine that reads `strlen` and builds the header in a buffer), and a routine that
takes a string must say whether it takes one with a length or a plain C one (this goes with study 4's declarations). It is the largest change of the eight; after 2 and 4.

## 6. File names that match Unix's limits

*Now.* `MKNAME` (`src/impm.imp`) builds a file name from two words (15 characters) and an extension of one word (7); the tape had six and three.
*Recommendation.* Keep the tape's form for `PRINT FILE 'NAME'.'EXT'` but let the name be as long as the string given (limit: `NAME_MAX`), since the library takes it from the string, not from a packed word. The compiler's own `MKNAME` is for names on its command line, which a shell has already limited: lift it to `PATH_MAX`.
*Effect.* Small: `MKNAME` and the buffer `NMB`; the `%.7s` of the library's file items (`CM6` in `imp72io.imp`) becomes the length of the buffer. Test with a name of 200 characters.

## 7. The empty string `''`

*Now.* A syntax error; a program that needs an empty C string passes the address of a zero byte.
*Recommendation.* Yes, a literal: `''` is the empty string, and within a string two apostrophes remain one apostrophe, so `''''` is one apostrophe. The ambiguity is only in the lexer (a pair of apostrophes with nothing between them), settled by "an empty string is two apostrophes not followed by a third".
*Effect.* One case in `lex.imp`, one test; with study 5 an empty string is a header of length 0 and a zero byte.

## 8. The count

The README says eight and names seven things in a list; the fourth (the type that a routine returns, and whether C's return types are declared) is two questions, and that makes eight. Assignment `:=` and the underscore as a letter are done.

## Decided (the user, 2026-11-03)

All the recommendations above are accepted, with the decision on strings in study 5. The order: 4, 7, 2 and 1 (the lexer, together), 6, 3, and 5 last. The syntax of IMP72 may now change in these ways, one study at a time, each with its tests and with the seed and stage 2 still writing the same IR (`tests/seed`).
