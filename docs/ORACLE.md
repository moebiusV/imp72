# The oracle: the original IMP72 compiler, running

The reference for what IMP72 does is the original: Bilofsky's IMP 1.5 (20-Aug-73) from the DECUS
tape, running on **TOPS-10 6.03 on a simulated KA10** (open-simh, Richard Cornwell's KA10/KI10/KL10
simulators), the same monitor and kind of machine it was made for. Programs are compiled by
`RUN IMP`, linked with `IMPLIB.LIB` and run; their output is what `tests/golden/oracle/` holds.

## Using it

```
tools/oracle-build.sh        # once: simulator, disks, tape, exact binaries (about 400 MB; see oracle/PROVENANCE.md)
tools/oracle-boot.sh         # start TOPS-10 in the background (1 to 3 minutes; it restarts itself if the boot hangs)
tools/oracle-put.py decus/exact/imp.low.words=IMP.LOW decus/exact/imp.shr.words=IMP.SHR \
                    decus/exact/implib.lib.words=IMPLIB.LIB     # once per fresh disk
tools/oracle-run.py -d DIR prog.imp...      # compile, link, run; DIR/prog.oracle (and .tty, .compile)
tools/oracle-update.sh       # make tests/golden/oracle from tests/oracle/LIST
tools/oracle.py "DIR" "TYPE X.IMP"          # any TOPS-10 commands, over the simulated terminal line
```

`make check` does not need the oracle: `tests/oracle/run.sh` compiles the listed programs with
imp72stage0 and compares with the stored goldens.

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

## Limits

- **KA10 has no floating conversion instructions** (`FLTR` and friends came with the KI10), so
  programs with `REAL` conversions stop at `?Illegal UUO`. `io.imp` (reals) is not in the golden
  list; a KI10 or KL10 monitor is needed for those, as is a KS10 for the later monitors.
- The boot sometimes hangs after `%DPA7 IS OFF-LINE`; `oracle-boot.sh` starts again.
- Programs for the oracle must be self-contained (no C main) and end with `FINI(0)`.
