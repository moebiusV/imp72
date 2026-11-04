# Things to know when writing IMP72

All of this is how the original language works, and both compilers (the seed
and `imp72`) agree on it.

## Expressions

- **No precedence, right to left.** `3*N+1` is `3*(N+1)`; write `(3*N)+1`.
  `A-B-C` is `A-(B-C)`.
- **A single relation in parentheses at the start of a statement is a
  syntactic ambiguity**: `(A=1) => S` is rejected; write `A=1 => S`. Several
  relations joined by `AND` or `OR`, each in parentheses, are a condition:
  `(A=1) AND (B=1) => S`, `WHILE (A=1) AND (B=1) DO S`. In `X AND Y => S` and
  `X OR Y => S` without parentheses around `X` and `Y` the `=>` takes only `Y`.
- **`>=` and `<=` are not operators**: write `GE` and `LE`. A relation used as
  a value is -1 for true.
- **A constant added inside brackets counts words, a variable counts bytes.**
  `[P+8]` is the word 8 words after `P`, the address `P+64`, and so is
  `[P+(2*4)]`, since a constant expression is folded first. `[P+K]` is the
  word at the byte address `P+K`. To reach word `K` write `[P+(8*K)]`. Outside
  brackets, `LOC(X)+N` and `P+K` add `N` or `K` bytes.
- **A C `int` result comes back in half a register**: mask it (`AND 4294967295`)
  before comparing it with a negative number or a 64-bit value.

## Statements and declarations

- **The main program is the statements before the first `SUBR`**; subroutines
  are separated by `;`, the last `SUBR` before `%%` has no `;` after it, and
  `%%` ends the file.
- **`FOR I TO N` runs I from 0 through N; `FOR I FROM N` counts down from N to
  0.** The body runs once even when the end is below the start, because the
  test is at the end of the loop.
- **`IS 100 LONG` declares an array**; an undeclared subscripted name is a
  one-word variable. A name in capital letters declared `COMMON` is a global
  symbol.
- **`ELSE` after a chain of conditions is a syntactic ambiguity**: parenthesize the
  inner test, as in `A=1 => (B=2 => C:=1 ELSE C:=2) ELSE C:=3`.
- **A `#` comment that is not closed by a `#` swallows the rest of the file.**
- **Keywords are not reserved but the syntax is ambiguous with them:** a
  variable called `FOR` or `TO` confuses `FOR` loops.

## Variables, labels and calls

- **Variables are static and shared within the compilation unit**, except formal
  arguments (references to the caller's argument). A subroutine's locals are
  shared by every call of it, and an argument expression is evaluated into a
  call temporary that is also static. **So a subroutine cannot call itself** and
  expect its variables, or an argument expression of an outer call, to
  survive: `FIB(N-1)+FIB(N-2)` returns a wrong number. Recursion is done with
  an explicit stack (`PUSHMI` and `PULLYU` in the compiler's own sources): a
  recursive routine pushes what it needs, copies its arguments into variables
  at once, and takes the values back after each call.
- **Arguments of a call are evaluated right to left.** Two arguments with side
  effects run in that order.
- **Labels are global to the compilation, not to the routine.** The same label
  in two routines is not an error, and a `GO TO` can land in the other one.
- **A block from `calloc` is addressed `[P+(8*K)]`**, not `P[K]`: an array in IMP
  is the variable's own storage.
- **A compilation can name 2047 registers.** The compiler stops with a message
  past that: split the file into modules.
