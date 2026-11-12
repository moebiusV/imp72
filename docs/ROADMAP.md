# Roadmap: what is not done

Each item says what it is, what is proposed, and the effect on the code. None
is implemented; `docs/LANGUAGE-DIFFERENCES.md`, `docs/FFI.md` and
`docs/BUILDING.md` describe what exists. Every change to code generation is
made in both languages of the generator (`src/*.imc` and `stage0/gen_*.c`) and in both IR writers
(`src/ir2ll.imc`, `src/ir2llx.imc` and `tools/ir2ll.c`), and `tests/seed`
requires the seed and stage 2 to keep writing the same IR.

A call to C stays visibly different from a call to IMP, whose arguments are
by reference: `CCALL` is always written.

## Language

### 5. What is left of the original's channel I/O

`INIT`, `LOOKUP`, `ENTER`, `INPUT`, `OUTPUT`, `CLOSE`, `RENAME` and `RELEAS` are in `lib/impchan.iml`: the monitor calls of TOPS-10 as routines over
`FILE *`, one buffer of 1024 bytes to a channel, a header of three words (the buffer, the byte pointer, the count). Added since: `USETI` and `USETO`
(an `fseek` to a block of 1024 bytes), the status of a channel (`GETSTS`, `SETSTS`, `STATZ`, `STATO`, with the bits of TOPS-10 for the end of the
file and a failed transfer), and dump mode (mode `17B`: `INPUT` and `OUTPUT` take a list of pairs, count and address, ended by a count of 0, and
move that many words of 8 bytes). `RLEAS` is back and the other `CORE` is in `free.imc`. The test is `tests/native/chan.imp` and `chan2.imp`.

Not provided: binary mode and its checksums (a block of 36 bit words with a count and a checksum word has no file format on a machine with 64
bit words that a program of the tape could read), the ring of buffers (its links are 18 bit addresses), the devices but `DSK` and `TTY`, and the
date, protection and project-programmer words of `LOOKUP`, `ENTER` and `RENAME` (a Unix file has none of them that a program could set).
None of the tape's own programs uses any of these.

Decided (2026-11-11): the rest as far as a program of the tape needs it; the programs of the tape (the compiler's modules) need none of what is
not provided, so it stays so until a program that does is found.

### 11. The compiler on Windows

The runtime library for Windows and the programs it links run (`docs/BUILDING.md`, `tests/winrun`, under Wine). The compiler itself does not: `lib/imphost.iml` forks, execs and waits (the
driver starts lld and the children of a compilation of several files), the temporary files of `src/driver.imc` are POSIX, and the images need the table of the compiler's variables that COFF has no
section for. Decided (2026-11-05): Windows is supported where LLVM and mingw can do it; what remains is `CreateProcess` and `WaitForSingleObject` where the host layer forks, a Windows
temporary directory, a table of the variables for COFF, and a run on a Windows machine, where Wine runs the programs and not yet the compiler's own build.

Cygwin: the runtime library is built for it and `imp-sysroot` fetches its packages, but the link stops at `__data_start__`, `__data_end__`, `__bss_start__` and `__bss_end__`, which GNU ld's script defines and lld does not (`docs/BUILDING.md`); nothing was run on Cygwin.

*Effect on the code.* A `lib/imphostx` for Windows and the link tables in `src/driver.imc` where a run shows them wrong.

## Open question: tape routines

`docs/porting/tape-names.md` compares the routine names of the tape with the
port's. Two of the tape's routines are not present: `CORE(N)`, a monitor call
to grow the core (the arena is reserved once and `free.imc` grows its usable
part, so `CORE` could return as "make N words usable"), and `RLEAS(CH)`,
which released a channel. Whether either is restored is to be examined
together with item 5, since `RLEAS` is the `fclose` of a channel and `CORE`
may depend on it.
