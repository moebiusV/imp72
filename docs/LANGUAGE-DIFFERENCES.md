# IMP72 on the PDP-10 and IMP72 here

The language is the original's (`docs/manual/imp72.md` is its manual; `decus/syntax.imc` is its definition). What differs
comes from the machine, and from the runtime.

- **The word is 64 bits.** Integers are 64 bit two's complement, addresses are byte addresses, and `A[I]` scales by 8. Arithmetic
  that overflowed 36 bits does not here. A constant is folded as the program would compute it, in 36 bits for arithmetic
  except left shifts (64). The octal constant `777777777777B` is -1, as on the PDP-10.
- **Reals** are doubles rounded to the PDP-10's 27 bit mantissa after every operation, with the original's inexact constant
  conversion and `PRINT FLT` (a port of its print module), so that programs print what the original printed.
- **Strings** are bytes: a string constant is a C string in a word array (not five 7 bit characters to a word), so its address
  can be passed to C. The characters of a name are packed five to a word inside the compiler (`DNAME`), not in programs.
- **Byte pointers** are byte addresses; `<s,p>` fields and `BYTEP`, `<+P>` work on 8 bit bytes of words.
- **Halves** (`HRRZ`, `A AND 777777B`, `RS 18`) are 32 bits.
- **Variables** that are not `COMMON` belong to their module (compilation unit); `COMMON` ones are global symbols that C can
  name. See `docs/FFI.md`.
- **Input and output** are the C library's. `PRINT`/`READ` keep their formats; `FILE`, `DEVICE` and `IMAGE MODE` map to files
  and standard I/O; the TOPS-10 UUOs (`CALLI`, `OUTSTR`, `INIT`/`LOOKUP`/`ENTER`) are the runtime's.
- **FFI.** `CCALL NAME(A,B)` calls a C function with values; a `SUBR` is a C function taking pointers.
- **Not in this port:** the PDP-10 code and the `.REL` output of the original (the code is the host's), `TRACE` (not in the
  original either), `XCT`-based tricks of the compiler's own sources (ported), the TOPS-10 command line.

`docs/LANGUAGE-NOTES.md` lists the things in the original that a new user trips on.
