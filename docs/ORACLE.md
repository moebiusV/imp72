# The oracle: the original IMP72 compiler, running

The reference for what IMP72 does is the original: Bilofsky's IMP 1.5 (20-Aug-73) from the DECUS
tape, running on **TOPS-10 6.03 on a simulated KA10** (open-simh, Richard Cornwell's KA10/KI10/KL10
simulators), the same monitor and kind of machine it was made for. Programs are compiled by
`RUN IMP`, linked with `IMPLIB.LIB` and run; their output is what `tests/golden/oracle/` holds.

## Using it

```
tools/oracle-build.sh        # once: prebsd, the simulator, disks, tape, exact binaries (about 400 MB; see oracle/PROVENANCE.md)
tools/oracle-boot.sh [ka|ki] # start TOPS-10 in the background through prebsd (1 to 3 minutes; prebsd restarts it if the boot hangs)
tools/oracle-boot.sh stop
tools/oracle-put.py decus/exact/imp.low.words=IMP.LOW decus/exact/imp.shr.words=IMP.SHR \
                    decus/exact/implib.lib.words=IMPLIB.LIB     # once per fresh disk
tools/oracle-run.py -d DIR prog.imp...      # compile, link, run; DIR/prog.oracle (and .tty, .compile)
tools/oracle-update.sh       # make tests/golden/oracle from tests/oracle/LIST
tools/oracle.py "DIR" "TYPE X.IMP"          # any TOPS-10 commands, over the simulated terminal line
```

`make check` does not need the oracle: `tests/oracle/run.sh` compiles the listed programs with
imp72stage0 and compares with the stored goldens.

## prebsd

What can be done by [prebsd](https://github.com/moebiusV/prebsd) is: building the simulators (pinned,
with the tape patch), fetching the disks, making the KI10's from Cornwell's tape, and booting (`prebsd -k` leaves the system running; its console stays held
by prebsd, the DC lines are ours). imp72 sets `PREBSD=/path` (default `../prebsd`, cloned from the
repository if absent) and does only what is specific to the oracle: the transfer tapes
(`tools/pdp10tap.py`), the terminal sessions (`tools/oracle.py`), the exact binaries of the
compiler and the goldens.

## How it works

- **The terminal line.** The simulator's DC device listens on telnet port 10603; `tools/oracle.py`
  logs in as `[100,100]` (password `demo1`) and types slowly (the line drops characters sent
  in a burst). A job left over from a dropped session must be `^C`ed first, and KJOB needs the `F`
  option; `Session.close()` does both.
- **Getting files in.** Files are written as a SIMH `.tap` (`tools/pdp10tap.py`): one record of
  128 words (the size PIP reads), five frames to a word (bits 35-28, 27-20, 19-12, 11-4, then 3-0
  in the low nibble: the drive's core-dump mode), then a tape mark. `PIP DSK:NAME=TAPE:/B`
  copies it. Text goes in as five 7-bit characters to a word with CR LF line ends. The tape file
  is padded to 8 MB because the simulator takes the tape's length from the file when it is
  attached, and it is patched (`oracle/simh-tape-fflush.patch`) to drop buffered data on a seek:
  without that it keeps serving the first block of the previous contents when the host rewrites the
  file.
- **Output.** A program's `PRINT` goes to the file `PO` unless it says `DEVICE 'TTY:'`; it is
  written only if the program ends with `FINI(0)` (the original does not flush on exit). The
  runner types the file and expands the terminal's tabs.

## What the oracle showed

- **The extracted files on the archive are damaged.** `decus/imp.low`, `imp.shr` and `implib.lib`
  (the archive's "5 bytes to a word" form) keep only 35 of the 36 bits of every word: every byte
  is below 0x80 and the last bit is gone (about 40% of the words had it set). They load but
  do not run correctly. The DECUS tape image has all the bits; `tools/decus-exact.py` finds each
  word in the tape (the extracted file says which words) and takes its last bit from there. The
  exact files are `decus/exact/*.words`, five frames to a word as on tape. All 12090, 20436 and
  4655 words were found.
- **Link with FORLIB.** `IMPLIB.LIB` leaves `IFIX` undefined; the link is
  `EX X.REL,/LIBRARY,IMPLIB.LIB,SYS:FORLIB/LIBRARY`.
- **hello, algo and features give the same output as imp72stage0**, byte for byte.

## Differential fuzzing

`tools/oracle-fuzz.sh SEED COUNT` writes random programs (`tools/imp-fuzz.py`: integer
expressions; `GEN=imp-fuzz2.py`: subroutines, arrays, loops, conditionals), runs them on the
original and on imp72stage0, and reports the programs whose output differs. The programs keep
their values small and shift only non-negative values, so the 36 bit original and the 64 bit
port must agree. `tools/imp-trace.py` adds a PRINT of the variables after every statement of
every SUBR, to find the statement where the two part. It found, in the port:

- constant-folded comparisons gave 1 for true (IMP's true is -1);
- the conditional case of `CONOP` (a constant condition) copied two of the three words of the
  chosen arm, and a one-armed conditional on a constant took the wrong branch;
- `PRINT` of an element such as `V[I]`, whose address an `HRRM` patches into the argument
  word, printed nothing.

Things in the original that the fuzzer taught us to leave alone:

- A constant passed as an argument is a word of the literal pool (shared with every other use
  of that value), and a `0` is address 0: assigning to such a parameter changes the
  constant for the whole program, or, for 0, overwrites the first instruction and the program
  stops with `?Illegal UUO`. The fuzzer never assigns to a parameter.
- The monitor folds a line longer than the terminal width (72) when it is typed; the runner
  sets `TTY WIDTH 200`.
- Output is lost when a program stops without `FINI(0)`.

## The KI10 oracle and REAL

The KA10 has no floating conversion instructions, so programs that convert reals stop with
`?Illegal UUO` there. The KI10 oracle (`tools/oracle-ki-build.sh`, `tools/oracle-boot.sh ki`;
login `[1,2]` password `FAILSA`, set `ORACLE_USER` and `ORACLE_PASS`) is TOPS-10 6.03 restored from
Richard Cornwell's system tape by [prebsd](https://github.com/moebiusV/prebsd)'s
`pdp10/install-ki.sh`. With it:

- **REAL is IEEE 754 here (since 2026-10-22), not the original's 27 bit floating point**, so reals no longer match the
  original bit for bit. Until then `fcon` and `fromd` reproduced its arithmetic (it converts `0.25` with its own `FCON`,
  in 27 bit arithmetic with an inexact 1/10, so `0.25`, `0.125`, `0.75` and `0.0625` came out one unit in the last place
  low, and every result was rounded to 27 bits), and `PRINT FLT` its digits, ties and all. Now: the oracle tests that
  expose those artifacts (`lits`, `ties`) have reviewed IEEE goldens in `tests/golden/ieee/`, which `tests/oracle/run.sh`
  uses in place of the original's; the other oracle tests (including `flt`, `fmt`, `reals`) still agree with the original;
  and the real fuzzer (`tools/imp-fuzz-real.py`) differs from the original by design wherever its 27 bit rounding showed
  (integer and structured fuzzers are unaffected). For REAL the reference is now `tools/imp-fuzz-ieee.py`, which
  evaluates the programs it writes in Python floats and writes the output they must give (FLT formatting
  re-implemented independently from the rules); `tools/ieee-fuzz.sh SEED COUNT` runs it, and `tests/ieee` runs a few
  programs in `make check`.
- **Zero** prints as `0` in any FLT format. The original never returns from the scientific
  form of zero (`FLT -12.4` of 0.0), and a program that overflows 2^127 stops the same way.

## Limits

- The KA10 oracle has no floating conversion instructions (see above): use the KI10 for REAL.
- The boot sometimes hangs after `%DPA7 IS OFF-LINE`; `oracle-boot.sh` starts again.
- Programs for the oracle must be self-contained (no C main) and end with `FINI(0)`.

## Dumps and listings against the original

`tests/oracle/dumps.sh` (in `make check`, no oracle needed) compares the debugging dumps and the `/A` listings of the seed with what the original printed, stored in `tests/golden/oracle-dumps/` (the semantics array S, the code tree and bit matrix T, the syntax graph X and the directory I, of `dr.imp`) and `tests/golden/oracle-lst/` (the 19 programs of `tests/oracle/LIST`). Capture with `ORACLE_SW=/S tools/oracle-run.py --lst -d DIR prog.imp` (`/T`, `/X`, `/I`, `/A`).

- **S, T, X are the original's, word for word**, with these differences, which `tools/dump-compare.py` counts and does not hide: the words that name a routine (403 in S, 82 distinct) or a constant by its address in `imp.shr` pair one to one with the slots of ours; the cell one past the bit matrix, which the tape's loop prints and which holds whatever follows the matrix in free storage (324 cells); the TERMINAL lines of X are compared as a set, as they come in the order of the directory.
- **The directory (I)** has the same names and hash codes, except: the original's alone (its program, and `\`, `IMP`, `IFIX`, `%TMP2`, `U`, and `_`, which is `:=` here: the assignment is one token of two characters, and tape/ is decus/ translated), ours alone (`CVAL`, added by the port), ten `#` names (`#ELIST`...) whose hash differs (ADDCHAR shifts a name and leaves a stray bit in the word of the original; ours has a byte to a character), and 18 runs where colliding names were entered in another order. `\` and `IMP` are the TOPS-10 command line (`decus/lex.imc` 92-111, `decus/impm.imc`); `%TMP2` is a spill temporary of RMOD; `IFIX` is entered at the start by the code generator, by the seed when a FIX needs it, and not used by imp72 (an operation of the IR does it); **`U` is not explained**: it is there before any program is read, in no production and no `DIR` call.
- **The listings** are the same sequence of instructions and labels as the original's after named rules (`tools/lst-compare.py` has them and the evidence): redundant reloads that the original's AMOD suppresses (it does not take a store through a pointer to change a variable), register copies its allocator removes, spill code of RMOD (`%TMPn`), byte pointers it builds in registers where ours names a descriptor, and the half-word float immediates of the PDP-10 where ours has whole-word IEEE constants. The comparison is of mnemonics and labels, not of operands; the values of REAL constants are covered by the run-output goldens.
- Found and fixed by it: the matrix of the code-tree dump had a stray `0` in its column labels (the tape's `IGR 6` has no value and prints nothing) and a width of our own (the original's is 36 bits times the words its highest column needs, at least 5).
