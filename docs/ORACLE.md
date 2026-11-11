# The oracle: the original IMP72 compiler, running

The reference for what IMP72 does is the original: Bilofsky's IMP 1.5
(20-Aug-73) from the DECUS tape, running on TOPS-10 6.03 on a simulated KA10 or
KI10 (open-simh and Richard Cornwell's simulators), the same monitor and kind
of machine it was made for. Programs are compiled by `RUN IMP`, linked with
`IMPLIB.LIB` and run; their output is what `tests/golden/oracle/` holds.

## Using it

    tools/oracle-build.sh        # once: prebsd, the KA10 simulator, disks, the DECUS tape, the exact binaries (about 400 MB; oracle/PROVENANCE.md)
    tools/oracle-ki-build.sh     # once, for the KI10: its simulator and its disks, restored from Cornwell's system tape
    tools/oracle-boot.sh [ka|ki] # start TOPS-10 in the background through prebsd (1 to 3 minutes)
    tools/oracle-boot.sh stop
    sh tools/imptool.sh oracle-put decus/exact/imp.low.words=IMP.LOW decus/exact/imp.shr.words=IMP.SHR \
                        decus/exact/implib.lib.words=IMPLIB.LIB     # once per fresh disk
    sh tools/imptool.sh oracle-run -d DIR prog.imp...      # compile, link, run; DIR/prog.oracle (and .tty, .compile)
    tools/oracle-update.sh       # make tests/golden/oracle from tests/oracle/LIST
    sh tools/imptool.sh oracle "DIR" "TYPE X.IMP"  # any TOPS-10 commands, over the simulated terminal line

`make check` does not need the oracle: `tests/oracle/run.sh` compiles the
programs of `tests/oracle/LIST` (19 programs) with the seed and compares their
output with the stored goldens.

## prebsd

[prebsd](https://github.com/moebiusV/prebsd) builds the simulators (pinned,
with the tape patch), fetches the disks, makes the KI10's from Cornwell's tape,
and boots (`prebsd -k` leaves the system running; its console stays held by
prebsd, the DC lines are free). `PREBSD=/path` names it (default `../prebsd`,
cloned from the repository if absent). This repository does what is specific to
the oracle: the transfer tapes (`tools/pdp10tap.imp`), the terminal sessions
(`tools/oracle.imp`), the exact binaries of the compiler and the goldens.

## How it works

- **The terminal line.** The simulator's DC device listens on telnet port 10603.
  `tools/oracle.imp` logs in as `[100,100]` (password `demo1`) on the KA10 disk,
  or as the `ORACLE_USER` and `ORACLE_PASS` of the environment (`1,2` and
  `FAILSA` on the KI10 disk), and types slowly because the line drops characters
  sent in a burst. A job left over from a dropped session is interrupted with
  `^C` first, and `KJOB` needs the `F` option; the tools do both on exit.
- **Getting files in.** Files are written as a SIMH `.tap` (`tools/pdp10tap.imp`):
  one record of 128 words (the size PIP reads), five frames to a word (bits
  35-28, 27-20, 19-12, 11-4, then 3-0 in the low nibble: the drive's core-dump
  mode), then a tape mark. `PIP DSK:NAME=TAPE:/B` copies it. Text goes in as
  five 7-bit characters to a word with CR LF line ends. The tape file is padded
  to 8 MiB because the simulator takes the tape's length from the file when it
  is attached, and the simulator is patched (prebsd's `simh/patch-sim-tape.py`)
  to drop buffered data on a seek: without that it serves the first block of
  the previous contents when the host rewrites the file.
- **Source text.** A program is sent as the tape wrote it, with `_` for the
  assignment (`tools/imp-assign.imp -T` converts `:=`).
- **Output.** A program's `PRINT` goes to the file `PO` unless it says
  `DEVICE 'TTY:'`; it is written only if the program ends with `FINI(0)` (the
  original does not flush on exit). The runner types the file and expands the
  terminal's tabs. `TTY WIDTH 200` keeps the monitor from folding long lines at
  72 columns.

## The binaries on the archive

`decus/imp.low`, `imp.shr` and `implib.lib` (the archive's "5 bytes to a word"
form) keep only 35 of the 36 bits of every word: every byte is below 0x80 and
the last bit is gone (about 40% of the words had it set). They load but do not
run correctly. The DECUS tape image has all the bits; `tools/decus-exact.imp`
finds each word in the tape (the extracted file says which words) and takes its
last bit from there. The exact files are `decus/exact/*.words`, five frames to
a word as on tape: 12090, 20436 and 4655 words.

The link needs the Fortran library for `IFIX`:
`EX X.REL,/LIBRARY,IMPLIB.LIB,SYS:FORLIB/LIBRARY`.

## Differential fuzzing

`tools/oracle-fuzz.sh SEED COUNT` writes random programs (`tools/imp-fuzz.py`:
integer expressions; `GEN=imp-fuzz2.py`: subroutines, arrays, loops,
conditionals), runs them on the original and on the seed, and reports the
programs whose output differs. The programs keep their values small and shift
only non-negative values, so the 36-bit original and the 64-bit port agree.
`sh tools/imptool.sh imp-trace IN.imp OUT.imp` adds a `PRINT` of the variables
after every statement of every `SUBR`, to find the statement where the two part.

Behaviour of the original that the fuzzers avoid:

- A constant passed as an argument is a word of the literal pool, shared with
  every other use of that value, and a `0` is address 0: assigning to such a
  parameter changes the constant for the whole program, or, for 0, overwrites
  the first instruction and the program stops with `?Illegal UUO`.
- Output is lost when a program stops without `FINI(0)`.

## The KI10 oracle and REAL

The KA10 has no floating conversion instructions, so programs that convert
reals stop with `?Illegal UUO` there. The KI10 oracle
(`tools/oracle-ki-build.sh`, `tools/oracle-boot.sh ki`) is TOPS-10 6.03
restored from Richard Cornwell's system tape by prebsd's `pdp10/install-ki.sh`.

REAL is IEEE 754 here and the original's is a 27-bit floating point, so reals
do not match the original bit for bit. The original converts `0.25` with its own
`FCON`, in 27-bit arithmetic with an inexact 1/10, so `0.25`, `0.125`, `0.75` and
`0.0625` come out one unit in the last place low, and every result is rounded
to 27 bits. The oracle tests that expose this (`lits`, `ties`) have reviewed
IEEE goldens in `tests/golden/ieee/`, which `tests/oracle/run.sh` uses in place
of the original's; the other oracle tests (including `flt`, `fmt`, `reals`)
agree with the original. Where 64-bit words give other numbers than 36-bit ones,
the golden is in `tests/golden/llvm-ir/`. The real fuzzer `tools/imp-fuzz-real.py`
(27-bit mantissa on both sides) differs from the original by design wherever its
rounding shows. The reference for REAL is `tools/imp-fuzz-ieee.py`, which
evaluates the programs it writes in Python floats and writes the output they
must give, with `FLT` formatting implemented independently from the rules;
`tools/ieee-fuzz.sh SEED COUNT` runs it, and `tests/ieee` runs a few programs in
`make check`.

The original prints zero as `0` in any `FLT` format, except that it never
returns from the scientific form of zero (`FLT -12.4` of 0.0), and a program that
overflows 2^127 stops the same way.

## Limits

- The KA10 oracle has no floating conversion instructions: use the KI10 for REAL.
- The boot sometimes hangs after `%DPA7 IS OFF-LINE`; prebsd starts it again.
- Programs for the oracle must be self-contained (no C main) and end with `FINI(0)`.

## Dumps and listings of the original

`tests/golden/oracle-dumps` and `tests/golden/oracle-lst` hold captures of the
original's debugging dumps (the semantics array `S`, the code tree and bit
matrix `T`, the syntax graph `X`, the directory `I`, of the program `dr.imp`) and
of its `/A` assembly listings (the 19 programs of `tests/oracle/LIST`). Capture
with `ORACLE_SW=/S sh tools/imptool.sh oracle-run --lst -d DIR prog.imp`
(`/T`, `/X`, `/I`, `/A`).

The captures are in 36-bit octal with halves of 18 bits and names of five
characters to a word, and the PDP-10 instructions of the original's listing.
The seed and `imp` print 64-bit words and the instructions of the IR machine,
so these captures are not compared with their output. `tools/dump-compare.imp
KIND ORACLE.LST OURS.TXT` and `tools/lst-compare.imp ORACLE.LST OURS.TXT` are the
comparison tools for a 36-bit path. What is compared with the real machine
is the output of the programs (`tests/oracle/run.sh`); the dumps of the seed and
of the stage compilers are compared with each other (`tests/seed`).
