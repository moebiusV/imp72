# PMOD, the formatted print module

`IMPLIB.LIB` of the original holds PMOD, the module that does the `PRINT` and `READ` of the manual; its sources are not
on the tape. The IMP compiler compiled it, so its code is readable: the labels `%IF19#`, `%WH21#`, `%FOR54#` are the
compiler's. The disassembly, from DDT on the KI10 oracle, is in [pmod-disassembly.txt](pmod-disassembly.txt).
`lib/impio.imp` implements `PRINT` and `READ` for this implementation and follows the disassembly where the output has
to be the same.

## Items

`PRN.` reads a list of words, one for each item; the item code `n` is the word shifted right by 30, and the word holds
the address of the operand. A format stays set between items and statements. In this implementation `PRINPAR`
(`src/code2.imc`) numbers the items 0 value, 1 `OCT`, 2 `IGR`, 3 `STG`, 5 `/`, 6 `FILE`, 7 `IMAGE MODE`,
8 `DEVICE`, 9 `TAB`, 10 `FILL`, 11 `FLT`, and each item becomes a call of `imppr(n,p)` (`imprd` for `READ`) in
`lib/impio.imp`; see `code-generator.md`. For `FLT` the format word holds `m` in the right half and `n` in the left.
The original works with `G = m-1` and `D = n`.

## FLT in the original

PMOD works in PDP-10 single precision (27 bit mantissa) with the rounded instructions `FMPR` and `FADR`:

1. `z = |x|`; zero prints as `0` and nothing else.
2. Scale into `.1 <= z < 1`, counting `EXP`: while `z < 0.1` multiply by 1e10 (`EXP -= 10`); while `z >= 1e10` multiply by
   1e-10 (`EXP += 10`); while `z >= 1` multiply by 0.1 (`EXP += 1`). The constants are the words `0137667633766`
   (1e-10), `0175631463146` (0.1) and `0242452013710` (1e10). 0.1 and 1e-10 are not exact, which is why 0.001 prints as
   `.99999"-3`.
3. `SCI = 1` (fixed notation). A negative `m` makes `G = -m-1`, `D = n+1`, `SCI = 0`.
4. `J = EXP*SCI` if `EXP > 0` (the digits before the point), `I = J + (D ? D+1 : 0)`. If `G >= 0` and `I >= G` the
   number does not fit: `SCI = 0`, `D = G-6` (at least 2, and then `G = -1`: no padding), `I = D+1`. `D = 0` adds one to
   `I`.
5. A rounding quantity `0.5 * 0.1^(I-1)` is formed by repeated multiplication by 0.1 and reduced by 0.1 while
   `rnd + z >= 1`; `z = rnd + z`.
6. Digits: a `-` for a negative number; then for each digit before the point `z *= 10; digit = IFIX(z) mod 10` (z is
   never reduced, so rounding errors accumulate); a `.`; then `D` more digits the same way, with `0`s first when
   `EXP < 0` and the notation is fixed.
7. Scientific form: the buffer gets the exponent (`-` if negative) and the `"` written in front of the digits, which are
   printed after them. The characters are written from the highest buffer index down.
8. Output pads with the fill character while the width `G` exceeds the index of the last character, then writes the
   characters with a loop that runs at least once. With nothing to write (a fraction with no decimals, `I = -1`) it
   writes the word before the buffer, a blank.

## FLT in this implementation

A `REAL` is an IEEE 754 binary64. `RTLPRINTFLT` in `lib/impio.imp` keeps the layout of steps 3, 4, 7 and 8: the field
width `G`, the test whether the number fits, the scientific form with `"` and the exponent, the padding. It replaces
steps 1, 2, 5 and 6 (the scaling and the digit loop with their accumulating errors) by the correctly rounded digits of
the exact decimal value, ties away from zero. `0.125` with two decimals is `.13`; the original's inexact arithmetic gives
`.12`, and `.99999"-3` for 0.001. A number that is not finite prints as `INF`, `-INF` or `NAN`.

The programs in `tests/oracle/` (`flt.imp`, `lits.imp` among them) are run on the original (`docs/ORACLE.md`);
`tests/golden/oracle` holds the outputs that agree with it and `tests/golden/ieee` the outputs that differ because of
the arithmetic.
