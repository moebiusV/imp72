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
