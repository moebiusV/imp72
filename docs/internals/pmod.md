# PMOD, the formatted print module

`IMPLIB.LIB` holds PMOD, the module that does the `PRINT`/`READ` of the manual (the
sources of PMOD are not on the tape). It was compiled by the IMP compiler itself, so its
code is readable: labels such as `%IF19#`, `%WH21#`, `%FOR54#` are the compiler's. The
disassembly, from DDT on the KI10 oracle, is in [pmod-disassembly.txt](pmod-disassembly.txt);
`lib/imp72io.imp` follows it where output must be the same.

## Items

`PRN.` reads a list of words, each `n<<30 + ...` plus the address of an operand: n = 0 a
value, 1 OCT, 2 IGR, 3 STG, 4 FILE, 5 `/`, 6 IMAGE MODE, 7 DEVICE, 8 TAB (code 9 here), 10
FILL, 11 FLT (code 13 octal in the format word). A format stays set (`FM.`/`FL.` of the output
channel). For FLT the format word holds `m` in the right half and `n` in the left; the
routine works with `G = m-1` and `D = n`.

## FLT

All of it is PDP-10 single precision, 27 bit mantissa, with the rounded instructions
(`FMPR`, `FADR`):

1. `z = |x|`; zero prints as `0` and nothing else.
2. Scale into `.1 <= z < 1`, counting `EXP`: while `z < 0.1` multiply by 1e10 (`EXP -= 10`);
   while `z >= 1e10` multiply by 1e-10 (`EXP += 10`); while `z >= 1` multiply by 0.1
   (`EXP += 1`). The constants are the words `0137667633766` (1e-10), `0175631463146`
   (0.1) and `0242452013710` (1e10): 0.1 and 1e-10 are not exact, which is why 0.001
   prints as `.99999"-3`.
3. `SCI = 1` (fixed notation). A negative `m` (its right half has the sign bit) makes
   `G = -m-1`, `D = n+1`, `SCI = 0`.
4. `J = EXP*SCI` if `EXP > 0` (the digits before the point), `I = J + (D ? D+1 : 0)`. If
   `G >= 0` and `I >= G` the number does not fit: `SCI = 0`, `D = G-6` (at least 2, and then
   `G = -1`: no padding), `I = D+1`. `D = 0` adds one to `I`.
5. A rounding quantity `0.5 * 0.1^(I-1)` is formed by repeated multiplication by 0.1, and
   reduced by 0.1 while `rnd + z >= 1`; `z = rnd + z`.
6. Digits: a `-` for a negative number; then for each digit before the point
   `z *= 10; digit = IFIX(z) mod 10` (z is never reduced, so rounding errors accumulate); a
   `.`; then `D` more digits the same way, with `0`s first when `EXP < 0` and notation is fixed.
7. Scientific form: the buffer then gets the exponent (`-` if negative) and the `"`
   written in front of the digits, which are printed after them. The characters are
   written from the highest buffer index down.
8. Output pads with the fill character while the width `G` exceeds the index of the last
   character, then writes the characters with a loop that runs at least once: with nothing
   to write (a fraction with no decimals, `I = -1`) it writes the word before the buffer, a
   blank.

### What this port does instead (IEEE 754, since 2026-10-22)

REAL is a binary64 double here, not 27 bit. `lib/imp72io.imp` keeps the layout of steps 3, 4, 7 and 8 (the field width
`G`, the fit test, the scientific form with `"` and the exponent, the padding) and replaces steps 1 to 2, 5 and 6, the
scaling and the digit loop with their accumulating errors, by correctly rounded digits of the exact decimal value, ties
away from zero (so `0.125` with two decimals is `.13`; the original's inexact arithmetic gave `.12` and `.99999"-3` for
0.001). The description above stays as the record of the original, which `tests/golden/oracle` (the tests that still
agree with it) and the golden outputs in `tests/golden/ieee` (the ones that differ) document.

Writing this down and porting it made 43 exact-tie rounding cases, the 14 value and format
rows of `tests/oracle/flt.imp`, `lits` and 18 random programs of real arithmetic agree with
the original.
