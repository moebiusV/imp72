# Things to know when writing IMP72

Found by writing test programs; all of it is how the original language works,
not something this port changed.

- **No precedence, right to left.** `3*N+1` is `3*(N+1)`; write `(3*N)+1`.
  `A-B-C` is `A-(B-C)`.
- **Variables are global to the compilation**, except formal arguments (which
  are references to the caller's argument). A subroutine's "locals" are shared
  by every subroutine that uses the same name, and an argument expression is
  evaluated into a call temporary that is also global. **So a subroutine
  cannot call itself** and expect its variables, or an argument expression of an
  outer call, to survive: `FIB(N-1)+FIB(N-2)` does not work. Recursion is done
  with an explicit stack (`PUSHMI`/`PULLYU` in the compiler's own sources).
- **Keywords are not reserved but the syntax is ambiguous with them:** a
  variable called `FOR` or `TO` will confuse `FOR` loops.
- **The main program is the statements before the first `SUBR`**; the last
  `SUBR` before `%%` has no `;` after it.
- **`FOR I TO N` includes N; `FOR I FROM N` counts down to 0.**
- **`IS 100 LONG` declares an array**; an undeclared subscripted name is a
  one word variable.
- **A condition that is two parenthesized tests needs one more pair of parentheses.** `WHILE (A=1) AND (B=1) DO S` runs zero times (the `AND` is taken as part of what follows the first
  parenthesis, there being no precedence); `WHILE ((A=1) AND (B=1)) DO S` is the loop. The same for `(A) AND (B) => S` and `A AND B => S`: parenthesize the whole condition. (Both compilers agree on this.)
- **`[P+8]` is the word 8 words after `P`**, that is the address `P+64`: a constant added inside the brackets is scaled by the word. For a byte offset write `[P+(8*I)]` for words, or put the
  byte address in a variable first and add to that. The same holds for `LOC(X)+N` with a bare constant `N` in the seed's old form; both compilers now count `N` as bytes when the address is in a
  variable or the offset is parenthesized.
- **`>=` and `<=` are not operators**: write `GE` and `LE`. An empty text constant `''` is a syntax error. A `#` comment that is not closed by a `#` swallows the rest of the file.
- **A C `int` result comes back in half a register**: mask it (`AND 4294967295`, or the `SCMASK` of the library) before comparing it with a negative number or a 64-bit value.
