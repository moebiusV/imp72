# Handoff: Windows 10 and Cygwin

Written 2026-11-13 at `63d1bb3` (main, all CI green: Linux amd64 and arm64, the Debian package, macOS arm64,
FreeBSD, NetBSD, OpenBSD and Alpine on eight architectures). For whoever picks up the Windows 10 and Cygwin
work on another machine. What was run and what was only read is said for each item; nothing here is guessed
without saying so.

## Where things are

| Thing | State |
|---|---|
| Programs **for** Windows (x86_64-w64-mingw32, msvcrt) | work: 38 programs of `tests/native` and the oracle compile, link and print the goldens **under Wine 10.0** (`tests/winrun`, in `make check`) |
| The same on **real Windows 10** | not tried |
| 32-bit Windows (i686-w64-mingw32) | the sysroot is fetched and a hello.exe links; never run (Wine here is 64-bit only as set up) |
| Programs for **Cygwin** (x86_64-pc-cygwin) | the runtime library builds; the link **stops** at four symbols (below); nothing was ever run |
| The **compiler itself** on Windows or Cygwin | not built, not tried |
| UCRT (x86_64-w64-mingw32-ucrt) | **done under Wine**: the 38 programs of `tests/winrun` print their goldens with it (`WINRUN_UCRT=1`, also in `make check`); not tried on real Windows |
| ARM Windows, i686 UCRT | not done (MSYS2 has `clangarm64`, which uses compiler-rt and not libgcc, so the driver would need to link `libclang_rt.builtins`; MSYS2 dropped 32-bit) |

The one command that makes a Windows program from Linux: `imp --target=x86_64-w64-mingw32 -o prog.exe prog.imp`.
It asks `tools/imp-sysroot` for the mingw-w64 files (fetched once from the `sysroots` branch of
https://github.com/moebiusV/cruntime, else from Debian's packages) and builds `libimp.a` for the target
(`tools/mkrt.sh`). `docs/BUILDING.md` has the details; `docs/sysroot.1` is the tool's manual page;
the README of cruntime has the provenance of every directory.

## About Wine and "run imp72 under Wine"

Wine is not the obstacle. Programs made for Windows already run under it (this session used Wine 10.0 from
Debian, set up without installing anything by `tools/wine-setup.sh` into `tmp/win`; `tmp/win/run.sh PROGRAM`
runs one; `make check` skips the test, exit 77, where that cannot be done). A newer Wine would be fetched the same
way; it was not tried and is not needed for those programs.

What stops the **compiler** running under Wine is that there is no Windows build of the compiler to run:

1. It is linked in-process with LLVM and lld (`stage0/impld.cpp`, `stage0/lvtriple.cpp`, `src/llvm.imc`), which
   need LLVM and lld *libraries for the Windows target*. Debian's mingw packages do not have them. On Windows they
   come from MSYS2 (`mingw-w64-clang-x86_64-llvm`, `-lld` in the CLANG64 environment), which is the easy way and
   works on a real Windows 10 machine (and probably under Wine, not tried).
2. The host layer assumes POSIX (below).
3. The images (`/U`, `/V`: save and restart) need a table of the compiler's own variables that COFF cannot give.

So "under Wine" and "on Windows 10" are the same job: make a Windows build of the compiler, then run it wherever.

## The compiler on Windows: the work

**Build.** On Windows 10 with MSYS2 (CLANG64): `pacman -S make mingw-w64-clang-x86_64-llvm
mingw-w64-clang-x86_64-lld mingw-w64-clang-x86_64-clang`, then `make`. The Makefile finds LLVM with
`tools/llvm.sh` (llvm-config); expect to fix what it assumes about a Unix. The seed (`stage0`, C99) builds the
first compiler, which then builds itself twice (`build/llvm-ir1`, `llvm-ir2`), and `tests/seed` compares the IR the
seed and the compiler write. Reading, not trying: the seed's C should compile with clang on mingw; the shell
scripts (`tools/*.sh`, `tests/*/run.sh`) need MSYS2's sh and coreutils.

**POSIX calls to replace** (from `grep` over `src/` and `lib/`, not from a run):

| Where | Calls | Needed for |
|---|---|---|
| `lib/imphost.iml` | `fork`, `execv`, `execvp`, `waitpid`, `kill`, `signal` | running lld as a program where there is no lld library; the children of a compilation of several files (`MULTI`); `imp --run` |
| `lib/imphost.iml` | `mkdtemp`, `memfd_create` paths (`TDMEMFD`), the temp-directory cleanup on signals | the private temp directory and the object handed to the linker |
| `src/driver.imc` | `readlink`, `realpath` (lines about 61 to 71), `dup2` (about 500), `popen` and `system` (about 558 and 579: the call of `imp-sysroot`, a shell script) | finding the compiler's own directory, redirecting, fetching sysroots |
| images (`src/image.imc`, `lib/impvars.iml`, `impvarsx-*.iml`) | the table of variables | `/U`, `/V` |

`CreateProcess` and `WaitForSingleObject` replace fork, exec and wait; `GetTempPath` and `CreateFile` replace the
temp directory; `GetModuleFileName` replaces `readlink /proc/self/exe`. `lib/impwin.iml` and `lib/impcore-win.iml`
(VirtualAlloc for the compiler's arena) are the existing Windows layer for **programs**; the compiler needs the
same kind of layer for its host calls, probably `lib/imphostx-coff.iml`'s neighbour. The roadmap, item 11, says the
same in fewer words.

**The variables table.** ELF has `__start_impvars`/`__stop_impvars`, Mach-O has `getsectiondata`; COFF has no
section the linker bounds, so `lib/impvarsx-coff.iml` is empty (`VSTART`, `VSTOP` return 0, `IMPVARLIST` finds no
table, images are refused). A COFF way exists: sections named `impvars$a`, `impvars$m`, `impvars$z` are ordered by
the part after the `$`, so a start symbol in `$a` and an end symbol in `$z` bound the table (lld's COFF linker
sorts grouped sections that way; MSVC and GNU ld do too). Not tried.

## Cygwin

Two different jobs, and the second is probably the easy one.

**A. Programs for Cygwin, made on Linux** (`imp --target=x86_64-pc-cygwin`). `imp-sysroot x86_64-pc-cygwin` fetches
the Cygwin packages (cygwin, cygwin-devel, w32api-headers, w32api-runtime, libgcc1; checked against `setup.ini`'s
SHA-512). The driver now also searches `usr/lib/w32api` for kernel32 and its kin. The link then stops:

    ld.lld: error: undefined symbol: __data_start__   (also __data_end__, __bss_start__, __bss_end__)
    >>> referenced by .../winsup/cygwin/lib/_cygwin_crt0_common.cc:154  libcygwin.a(_cygwin_crt0_common.o)

GNU ld's script for Cygwin defines these four at the ends of `.data` and `.bss`; lld (LLVM 19, here) does not. The
Cygwin library reads them to know the range of the program's data, for `fork`. Untested idea: an object put **first**
on the link line whose `.data` begins with `__data_start__` and `.bss` with `__bss_start__`, and an object whose
`.data$zzz` and `.bss$zzz` hold `__data_end__` and `__bss_end__` (grouped sections are ordered by suffix, plain
ones first), added by the driver for Cygwin only. Whether the result runs under Cygwin is unknown: it needs a
Cygwin to run it, which this machine never had. Newer lld may define the symbols itself: check on the other box
before writing anything.

**B. The compiler built and run under Cygwin.** Cygwin is a POSIX layer: `fork`, `exec`, `waitpid`, `mkdtemp`,
`realpath`, `readlink` exist, so `lib/imphost.iml` and the driver may need little or nothing. Cygwin has `gcc`,
`make`, `llvm` and `lld` packages (check which versions and whether the development files and `llvm-config` are
there; if not, building the seed with `-DHAVE_LLVM` off gives a compiler that writes objects but cannot link in
process). What is missing for certain is the table of variables (`lib/impvarsx-coff.iml` is empty, so `/U` and
`/V` are refused) and the Cygwin entries of `tools/sysconst.sh` / `tools/mkrt.sh` / the host files
(`lib/imphostx-coff.iml`), which were written from the headers and never run. Start here: `make`, `make check`,
and see what breaks.

## A test plan for a real Windows 10 box

1. On Linux: `imp --target=x86_64-w64-mingw32 -o NAME.exe tests/native/NAME.imp` for the programs listed in
   `tests/winrun/run.sh` (the `progs` line), and copy the `.exe` files with `tests/native/NAME.out` (the goldens).
   They need no DLL but msvcrt and kernel32, which every Windows has.
2. On Windows: run each, redirect stdout to a file, compare with the golden **byte for byte** (a program writes the
   bytes it means, no CR LF; the runtime sets the standard files binary: `IMPBINMODE`, lib/impwin.iml). In
   PowerShell: `.\NAME.exe | Set-Content -Encoding Byte` is **not** safe (PowerShell re-encodes); use
   `cmd /c "NAME.exe > NAME.got"` and `fc /b NAME.got NAME.out`.
3. Things Wine may hide: file name case and the path separator in `lib/impchan.iml` and `impio.iml`
   (`RENAME` closes the file first, files are opened `rb`/`wb`), console input of `TTY`, the standard files
   as `__iob_func()` offsets in a Windows that has a different msvcrt (Windows 10 has `msvcrt.dll` as a system
   file, and UCRT beside it: the programs use `msvcrt`, which is what mingw-w64's default links to).
4. The four programs left out of `tests/winrun` need a C function msvcrt lacks or treats otherwise: `structs`
   (`strptime`), `cdecl` (`htonl`: in ws2_32), `cdecl2` (ctype values), `extcall` (`environ`). Decide each: provide
   the function in a Windows-only library file, or keep them out.

## The other open item: the rest of channel I/O (roadmap 5)

Asked for on 2026-11-13: "build the rest of channel io but only what we need and what might make sense on unix, be
generous". **Not started in code**, only assessed; the plan is here so it is not lost.

The tape's manual (`decus/imp.rno`, near line 3586) documents these routines; `lib/impchan.iml` has the ones marked:

| Routine | Here |
|---|---|
| INIT, LOOKUP, ENTER, RENAME, CLOSE, INPUT, OUTPUT, USETI, USETO, GETSTS, SETSTS, STATO, STATZ | yes |
| RELEA (the manual) / RLEAS (the tape's `impm.imc`) | `RELEAS` is the library's; `RLEAS` in `src/impm.imc` calls it; the manual's `RELEA` name is missing |
| IN, OUT (aliases of INPUT, OUTPUT) | missing |
| OPEN, INBUF, OUTBUF, GETCHA, UGETF, MTAPE | missing |
| INCHRW, INCHRS, INCHWL, INCHSL, SKPINC, SKPINL, RESCAN, OUTCHR, OUTSTR, IONEOU, CLRBFI, CLRBFO, GETLCH, SETLCH | only OUTCHR, OUTSTR, INCHWL, and in the compiler's own module (`src/impm.imc`), not in `libimp.a`: a program that links the library cannot call them |

What I would build, in this order (each with a test and a manual entry; no stubs, a routine is real or absent):

1. **The console routines in the library** (`lib/impcon.iml`): INCHRW, INCHRS, INCHWL, INCHSL, SKPINC, SKPINL,
   OUTCHR, OUTSTR, IONEOU, CLRBFI, CLRBFO, RESCAN over file descriptors 0 and 1 (`poll`/`read`; line mode through
   the C library), moved out of `src/impm.imc`'s private copies so the compiler uses the library's.
2. **IN, OUT, RELEA** as the manual's names for INPUT, OUTPUT, RELEAS; **GETCHA** (the first free channel, 15 down
   or 17B up as the manual says); **UGETF** (the block after the end of the file, from `fstat`).
3. **A ring of buffers**: INBUF(CH,N) and OUTBUF(CH,N) give a channel N buffers of 1024 bytes that INPUT and OUTPUT
   go through in turn, the header's buffer word following. Default 2 as on TOPS-10.
4. **Binary mode** (mode 14): a file of records, each a word with the count of data words, the data words, and a
   checksum word (sum of the others); INPUT checks it and sets IO.BKT (40000B) on a mismatch, as TOPS-10 does.
5. **Devices**: `NUL` (/dev/null) built in, and any other device name through an environment variable
   `IMPDEV_NAME` naming a directory (files under it) or a file or character device (LPT, PTR, PTP, MTAn: one
   stream), the Unix form of TOPS-10's ASSIGN.
6. **LOOKUP/ENTER/RENAME**: the date and protection words as Unix time and mode bits through the array form of the
   calls, if the form (`LOOKUP(CHAN,ARY)`) can be told from the other by its argument count. Least certain.

`MTAPE` (tape positioning) and the project-programmer numbers have no Unix meaning; leave them out.
A pitfall found on the way: a test passed the constant `'0'` as the input buffer header of `INIT`, which **writes**
a header into it (and over the constants after it): give every channel a real 3-word header array.

## Facts a new session needs

- Repo `moebiusV/imp72` (checkout `~/claude/imp72`); files for other systems in `moebiusV/cruntime` (`main` is
  only the README and the site; the sysroots are on its `sysroots` branch). CI: GitHub Actions, tag a commit message
  with `[ci:linux]`, `[ci:alpine]`, `[ci:bsd]`, `[ci:macos]` to run one group; a push cancels the runs of the
  previous commit. Documents-only pushes (`docs/**`, `**.md`) run no CI.
- No `gh` and no API token on the machine this was written on (an SSH key only): a repository can be pushed to
  once it exists, not created. CI logs need a login, but **check-run annotations are public**:
  `https://api.github.com/repos/moebiusV/imp72/check-runs/ID/annotations` (the job ids are in
  `/actions/runs/RUN/jobs`).
- Reproduce an Alpine architecture locally: `CI_CPUSET=0-3 sh tools/ci-arch.sh ppc64le build` then `check` (Docker
  with qemu binfmt; the work stays in `tmp/ci-ppc64le/tree`). Programs cross-built with `--target=` run under
  qemu-user with `QEMU_LD_PREFIX=` the sysroot.
- The user's rules: scratch only in the repo's `tmp/` (never `/tmp`); heavy jobs `taskset`-pinned, never CPU 5;
  one container and one VM at a time; check `git status` before `git add -A`; push only when told; nothing
  overwritten until its replacement is verified; no Claude or Anthropic line in a commit message (hooks refuse it).
- Lessons from the CI this week, in case they recur: LLVM 23 has no `default<Os>`; the seed's C must be 64-bit
  clean on 32-bit hosts; a short string's word is in **memory** order (the first byte is the high byte on
  big-endian); tests must not depend on the exact value a C library returns; and `INIT` writes into its header
  arguments.
- Memory notes of the previous sessions (outside the repo) are in
  `~/.claude-opusplan/projects/-home-matthew-claude-imp72/memory/` (index `MEMORY.md`); the useful ones are
  `roadmap-state.md` and `cruntime-and-imp-sysroot.md`.
