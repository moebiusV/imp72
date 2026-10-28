# IMP72 on the PDP-10 and IMP72 here

The language is the original's (`docs/manual/imp72.md` is its manual; `decus/syntax.imc` is its definition). What differs
comes from the machine, and from the runtime.

- **The word is 64 bits.** Integers are 64 bit two's complement, addresses are byte addresses, and `A[I]` scales by 8. Arithmetic
  that overflowed 36 bits does not here. A constant is folded as the program would compute it, in 36 bits for arithmetic
  except left shifts (64). The octal constant `777777777777B` is -1, as on the PDP-10.
- **Reals** are IEEE 754 binary64, not the PDP-10's 27 bit floating point: arithmetic is in double precision, a constant is
  the double nearest to it (`0.25` is exact; the original's was one unit low), and `FIX` and `FLOAT` convert exactly.
  `PRINT FLT m.n` keeps PMOD's layout (no 0 before the point, fixed notation unless it does not fit the field, then
  `.ddd"e`, a negative width for scientific always) with correctly rounded digits, ties away from zero. So programs with
  reals print the original's numbers except where its 27 bit arithmetic showed (`0.001` printed as `.99999"-3`).
  The word of a REAL is the double's own bits (`imp72`, the compiler written in IMP): the code generator compares reals with
  floating compares, negates them by flipping the sign bit and does arithmetic in doubles, as it makes the constants it
  once wrote as 36 bit all-ones into -1. The PDP-10 format sorts as a two's complement integer, so the original's code
  generator compared and negated a real with the integer instructions; `imp72stage0`, the seed, translates that code
  generator instruction by instruction and therefore keeps a word that behaves so (the double's sign and magnitude with the
  magnitude negated for a negative number, `lib/imp72real.h`). C code that shares a REAL converts with
  `imp_real_to_double` and `imp_double_to_real` (`lib/imp72.h`), which is the identity for `imp72`. Not a number and
  infinity print as `NAN` and `INF`; the original had neither.
- **Strings** are bytes: a string constant is a C string in a word array (not five 7 bit characters to a word), so its address
  can be passed to C. The characters of a name are packed eight to a word inside the compiler (`DNAME`), the first at the first address, not in programs.
- **Byte pointers** to characters (`BYTEP A<7,36>`) are byte addresses and a character is a byte (UTF-8); `<s,p>` fields and other `BYTEP` pointers work on the bits of the word.
- **Halves** (`HRRZ`, `A AND 777777B`, `RS 18`) are 32 bits.
- **Variables** that are not `COMMON` belong to their module (compilation unit); `COMMON` ones are global symbols that C can
  name. See `docs/FFI.md`.
- **Input and output** are the C library's. `PRINT`/`READ` keep their formats; `FILE`, `DEVICE` and `IMAGE MODE` map to files
  and standard I/O; the TOPS-10 UUOs (`CALLI`, `OUTSTR`, `INIT`/`LOOKUP`/`ENTER`) are the runtime's.
- **FFI.** `CCALL NAME(A,B)` calls a C function with values; a `SUBR` is a C function taking pointers.
- **Not in this port:** the PDP-10 code and the `.REL` output of the original (the code is the host's), `TRACE` (not in the
  original either), `XCT`-based tricks of the compiler's own sources (ported), the TOPS-10 command line.

`docs/LANGUAGE-NOTES.md` lists the things in the original that a new user trips on.
