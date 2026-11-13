# Handoff: Windows 10 and Cygwin

Written 2026-11-13 at `07afb89` (main; every CI group green at that commit: Linux amd64 and arm64, the Debian package,
macOS arm64, FreeBSD, NetBSD, OpenBSD and Alpine on eight architectures). For whoever picks up the Windows 10 and
Cygwin work on another machine. What was run and what was only read is said for each item; nothing here is guessed
without saying so.

## Where things are

Everything marked "Wine" was run under **Wine 10.0 on Debian x86_64** and nowhere else. **Nothing has run on a real
Windows machine yet: that is the first job.**

| Thing | State |
|---|---|
| Programs **for** Windows, msvcrt (`--target=x86_64-w64-mingw32`) | 38 programs of `tests/native` and the oracle compile, link and print the goldens (Wine; `tests/winrun`, in `make check`) |
| Programs for Windows, **UCRT** (`--target=x86_64-w64-mingw32-ucrt`) | the same 38 programs (Wine; `WINRUN_UCRT=1`, in `make check`); they import `api-ms-win-crt-*` and kernel32 only |
| The **seed** (stage0, C) built for Windows | `sh tools/win-seed.sh` cross-builds `build/win-seed/impstage0.exe`; under Wine it compiles and links, with lld in its own process, the 38 programs (`WINRUN_SEED=1`) |
| The **compiler written in IMP** built for Windows | `sh tools/win-imp.sh` cross-builds `build/win-imp/impstage-llvm-ir.exe`; under Wine it compiles and links the 38 programs (`WINRUN_WINIMP=1`), and its object of hello.imp is the seed's byte for byte |
| Images (`/U`, `/V`) on Windows | not possible yet: COFF has no table of the compiler's variables (below) |
| The compiler building **itself** on Windows | not tried |
| 32-bit Windows (i686-w64-mingw32, msvcrt) | the sysroot is fetched and a hello.exe links; never run |
| ARM Windows, i686 UCRT | not done (MSYS2 has `clangarm64`, which uses compiler-rt and not libgcc, so the driver would need `libclang_rt.builtins`; MSYS2 dropped 32-bit) |
| Programs for **Cygwin** (x86_64-pc-cygwin) | the runtime library builds; the link **stops** at four symbols (below); nothing ever run |
| The compiler on **Cygwin** | not tried |

The one command that makes a Windows program from Linux: `imp --target=x86_64-w64-mingw32-ucrt -o prog.exe prog.imp`.
It asks `tools/imp-sysroot` for the mingw-w64 files (fetched once from the `sysroots` branch of
https://github.com/moebiusV/cruntime: `windows-ucrt/x86_64` from MSYS2's ucrt64 packages, `windows/x86_64` from Debian's
mingw-w64) and builds `libimp.a` for the target (`tools/mkrt.sh`). `docs/BUILDING.md` has the details,
`docs/sysroot.1` is the tool's manual page, and the README of cruntime has the provenance and license of every directory.

## How the Windows builds are made (all from Linux; nothing is installed)

| Script | Makes | Needs |
|---|---|---|
| `tools/msys2-fetch.sh DIR NAME...` | unpacks MSYS2 ucrt64 packages into `DIR/ucrt64`, each checked against the SHA-256 of the repository database | curl or wget, zstd, tar, sha256sum |
| `tools/win-seed.sh` | `build/win-seed/impstage0.exe` (about 158 MB: LLVM 23 and lld are in it, every target) + `libxml2-16.dll libz-1.dll libzstd.dll libiconv-2.dll` beside it | clang and ld.lld of this machine's LLVM (19 was used), the MSYS2 tree (fetched into `tmp/msys/w`, about 1.1 GB unpacked, by the script) |
| `tools/win-imp.sh` | `build/win-imp/impstage-llvm-ir.exe` + the same four DLLs + `build/win-imp/rt/libimp.a` (the runtime for the UCRT) | the above, and `make` done first (the Linux compiler `build/llvm-ir2/impstage-llvm-ir` and `build/llvm-ir2/imprename`) |
| `tools/wine-setup.sh` | Wine 10.0 from the Debian packages into `tmp/win` (`tmp/win/run.sh PROGRAM.EXE` runs one; TMPDIR must not be set) | apt-get download, dpkg-deb, unshare |

Two Wine details learned the hard way: Wine's `user32.dll` needs `zlib1.dll` in the prefix's `system32`
(`cp tmp/win/wine/usr/lib/x86_64-linux-gnu/wine/x86_64-windows/zlib1.dll tmp/win/prefix/drive_c/windows/system32/`);
and a program run under it sees Linux paths as `Z:/home/...` (a path starting with `/` works too).

**The seed and the IMP compiler for Windows find their parts like this**: the syntax files by the environment
(`IMPSYNTAX`, `IMPFFI`; `lib/utf8.imc` is beside `IMPFFI`), the runtime library by `IMPLIB`, and mingw-w64's files by
`IMPSYSROOT` or the directory above the one the program is in (the tree of MSYS2's ucrt64 has `bin/`, `lib/`, `include/`:
the start file `lib/crt2.o`, `libmingw32.a`, `libucrt.a`, libgcc). The UCRT is linked unless `IMPCRT=msvcrt`.
The programs are named `impstage0.exe` and `impstage-llvm-ir.exe` on purpose: the name starting `impstage` is what
makes them the compiler and not the kernel alone (the test looks at the part after the last `/` **or** `\`).

How the pieces of the IMP compiler for Windows are made, so that a fix goes to the right place:

- **The modules** are compiled to COFF by the Linux compiler (`--target=x86_64-w64-mingw32-ucrt -c`), **renamed**
  by `tools/imprename` (it reads and writes COFF now: x86-64, i386 with the underscore of its C names, ARM64, ARM;
  `tests/imprename`), and linked by lld with `libimp.a`, the two C++ shims (`stage0/lvtriple.cpp`, `impld.cpp`), the
  string functions of the seed, and MSYS2's LLVM and lld static libraries.
- **The host layer for Windows** is `lib/imphost-win.iml` (IMP, with Win32 calls by CCALL), used by `tools/mkrt.sh`
  for Windows in place of `lib/imphost.iml`: the private temporary directory removed at exit, `CreateProcess` for
  the linker as a program and for the children of a compilation of several files, the program's own path
  (`IMPEXEPATH`, `IMPFULLPATH`), the questions about an ELF system answered none, and the memory streams
  (`IMPMEMOPEN`, `IMPMEMCLOSE`, `IMPMEMFROM`: the C library of Windows has no `open_memstream` or `fmemopen`, so they
  are temporary files deleted on close). The seed has its own, in C: `stage0/imphost-win.c`, `stage0/winport.h`.
- **The driver** (`src/driver.imc`): `LNKWIN` links on Windows for Windows through `LNKCROSS` (the cross link for
  `-m i386pep`) with the sysroot defaults above.
- Not an error: `ld.lld` warns `/aligncomm symbol X not found` for every common symbol (the object's directive names it
  by the name from before the rename); the warnings go to `build/win-imp/link.log`; lld aligns a common symbol by its size.

## What to do on the Windows 10 machine, in order

1. **Copy** from a Linux build (or rebuild there: this is the only way to make them, the scripts need Linux) to the
   Windows box: `build/win-seed/` (`impstage0.exe` and the four DLLs), `build/win-imp/` (`impstage-llvm-ir.exe`, the
   DLLs, `rt/libimp.a`), a UCRT sysroot (the directory `imp-sysroot --path x86_64-w64-mingw32-ucrt` prints, or MSYS2's
   `ucrt64` tree: it has `lib/crt2.o`), `src/syntax.imc`, `lib/ffi.imc`, `lib/utf8.imc`, and `tests/native` with its goldens.
   The programs need no installation: they use `ucrtbase` (a part of Windows 10) and the four DLLs beside them.
2. **Programs first** (no compiler involved): on Linux `imp --target=x86_64-w64-mingw32-ucrt -o NAME.exe
   tests/native/NAME.imp` for the programs on the `progs` line of `tests/winrun/run.sh` (and the oracle list), copy the
   `.exe` files; on Windows run each with its output redirected, `cmd /c "NAME.exe > NAME.got"`, and compare byte for
   byte, `fc /b NAME.got NAME.out` (a program writes the bytes it means, no CR LF: the runtime sets the standard files
   binary, `IMPBINMODE`; **not** PowerShell's `|` or `Set-Content`, which re-encode).
3. **The seed on Windows**: set `IMPSYNTAX`, `IMPFFI`, `IMPLIB` (the `libimp.a`), `IMPSYSROOT` (the sysroot) and run
   `impstage0.exe -o hello.exe hello.imp`; then `hello.exe`. Then every program of step 2 compiled on Windows and
   compared with its golden (this is `WINRUN_SEED=1`, by hand).
4. **The IMP compiler on Windows**: the same with `impstage-llvm-ir.exe` (`WINRUN_WINIMP=1`, by hand; set
   `IMPFETCH=0`, because the driver would otherwise call `imp-sysroot`, a shell script, through `system()`).
5. **Things Wine may have hidden**: file names and `\` against `/` in `lib/impchan.iml`, `impio.iml`; console input of
   `TTY`; `RENAME` (the file is closed first, which Windows needs); the temporary directory (`TEMP`, `TMP`);
   an anti-virus holding a freshly written `.exe`; a path with spaces (the link arguments are not all quoted); the
   child processes of `imp a.imp b.imp` (`IMPRUNKID` in `lib/imphost-win.iml`: **written, never run**; Wine only
   compiled one file at a time); `imp --run`.
6. The four programs left out of `tests/winrun` need a C function the UCRT or msvcrt lacks or treats otherwise:
   `structs` (`strptime`), `cdecl` (`htonl`: in ws2_32), `cdecl2` (ctype values), `extcall` (`environ`). Decide
   each: provide the function in a Windows-only library file, or keep them out.
7. If it all runs: native building of the seed and the compiler on Windows with MSYS2 (`UCRT64` shell,
   `pacman -S make mingw-w64-ucrt-x86_64-llvm mingw-w64-ucrt-x86_64-lld mingw-w64-ucrt-x86_64-clang`, then `make`):
   not tried; the Makefile and `tools/*.sh` assume a Unix shell (MSYS2 has one), and `tools/direct.sh` renames symbols
   with `imprename`, which does COFF now.

## What is left to build for Windows

**The variables table (images, `/U` and `/V`).** ELF has `__start_impvars`/`__stop_impvars`, Mach-O has
`getsectiondata`; COFF has no section the linker bounds, so `lib/impvarsx-coff.iml` is empty (`VSTART`, `VSTOP` return
0, `IMPVARLIST` finds no table, images are refused). A COFF way exists: sections named `impvars$a`, `impvars$m`,
`impvars$z` are ordered by the part after the `$`, so a start symbol in `$a` and an end symbol in `$z` bound the
table (lld's COFF linker sorts grouped sections that way). Not tried. Needed before the images work on Windows, and
for the compiler to build itself there.

**The compiler building itself on Windows**: needs the above, a shell (MSYS2), and `tools/direct.sh` run with the
Windows compiler: not tried.

## Cygwin

Two different jobs, and the second is probably the easy one.

**A. Programs for Cygwin, made on Linux** (`imp --target=x86_64-pc-cygwin`). `imp-sysroot x86_64-pc-cygwin` fetches
the Cygwin packages (cygwin, cygwin-devel, w32api-headers, w32api-runtime, libgcc1; checked against `setup.ini`'s
SHA-512). The driver searches `usr/lib/w32api` for kernel32 and its kin. The link then stops:

    ld.lld: error: undefined symbol: __data_start__   (also __data_end__, __bss_start__, __bss_end__)
    >>> referenced by .../winsup/cygwin/lib/_cygwin_crt0_common.cc:154  libcygwin.a(_cygwin_crt0_common.o)

GNU ld's script for Cygwin defines these four at the ends of `.data` and `.bss`; lld (LLVM 19, here) does not. The
Cygwin library reads them to know the range of the program's data, for `fork`. Untested idea: an object put **first**
on the link line whose `.data` begins with `__data_start__` and `.bss` with `__bss_start__`, and an object whose
`.data$zzz` and `.bss$zzz` hold `__data_end__` and `__bss_end__` (grouped sections are ordered by suffix, plain
ones first), added by the driver for Cygwin only. Whether the result runs is unknown: it needs a Cygwin. Newer lld
may define the symbols itself: check there before writing anything.

**B. The compiler built and run under Cygwin.** Cygwin is a POSIX layer: `fork`, `exec`, `waitpid`, `mkdtemp`,
`realpath`, `readlink` exist, so `lib/imphost.iml` and the driver may need nothing (the Windows host file is for
mingw, not Cygwin). Cygwin has `gcc`, `make`, `llvm` and `lld` packages (check the versions and whether the
development files and `llvm-config` are there; if not, the seed can be built without LLVM: it then writes objects but
cannot link in process). Missing for certain: the table of variables (`lib/impvarsx-coff.iml` is empty), and the
Cygwin entries of `tools/sysconst.sh`, `tools/mkrt.sh` and `lib/imphostx-coff.iml`, which were written from the
headers and never run. Start here: `make`, `make check`, and see what breaks.

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
   the C library), moved out of `src/impm.imc`'s private copies so the compiler uses the library's. (On Windows these
   need `_kbhit`/`_getch` or the console API in `lib/imphost-win.iml`'s neighbour.)
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

- Repos: `moebiusV/imp72` (checkout `~/claude/imp72`), the files for other systems in `moebiusV/cruntime` (its `main` is
  only the README, `LICENSES/` and the Pages site https://moebiusv.github.io/cruntime/; the sysroots are on its
  `sysroots` branch: `linux/debian13`, `linux/alpine322`, `windows`, `windows-ucrt`, `freebsd`, `openbsd`, `netbsd`;
  macOS is not there on purpose: the libSystem stub has no license of its own, `imp-sysroot` fetches it from Zig), and
  the user's Pages index, `moebiusV/moebiusv.github.io`. CI: GitHub Actions; tag a commit message with `[ci:linux]`,
  `[ci:alpine]`, `[ci:bsd]` or `[ci:macos]` to run one group; **a push cancels the runs of the previous commit**
  (the Alpine qemu architectures take 40 to 70 minutes); documents-only pushes (`docs/**`, `**.md`) run no CI.
- No `gh` and no API token on the machine this was written on (an SSH key only): a repository can be pushed to once
  it exists, not created. CI logs need a login, but **check-run annotations are public**:
  `https://api.github.com/repos/moebiusV/imp72/check-runs/ID/annotations` (the job ids are in `/actions/runs/RUN/jobs`).
- Reproduce an Alpine architecture locally: `CI_CPUSET=0-3 sh tools/ci-arch.sh ppc64le build` then `check` (Docker
  with qemu binfmt; the work stays in `tmp/ci-ppc64le/tree`, **owned by root**: remove it with a container). Programs
  cross-built with `--target=` run under qemu-user with `QEMU_LD_PREFIX=` the sysroot.
- The disk guard (`tools/guard.sh`) kills a build when `build/` and `tmp/` together pass 10 GB, silently as exit 137:
  `tmp/` holds big assets (the BSD VMs, Wine, the MSYS2 tree), so run `make` as `sh tools/guard.sh -g 30 make`.
- The user's rules: scratch only in the repo's `tmp/` (never `/tmp`); heavy jobs `taskset`-pinned, never CPU 5; one
  container and one VM at a time; check `git status` before `git add -A`; **push only when told**; nothing overwritten
  until its replacement is verified; no Claude or Anthropic line in a commit message (the hooks refuse it).
- IMP traps met this week: a comment with no closing `#` swallows the lines after it; an apostrophe in a comment can
  end it; a conditional with no `ELSE` is a statement, not a boolean (write `((A) AND (B))`); library files cannot use
  `IMPBYTE` (it is not in `libimp.a`; use `memcpy`, `strchr` and the like); a constant offset in brackets is words, a
  variable one is bytes.
- Lessons from the CI, in case they recur: LLVM 23 has no `default<Os>`; the seed's C must be 64-bit clean on 32-bit
  hosts; a short string's word is in **memory** order (the first byte is the high byte on big-endian); tests must not
  depend on the exact value a C library returns; `INIT` writes into its header arguments.
- Memory notes of the previous sessions (outside the repo) are in
  `~/.claude-opusplan/projects/-home-matthew-claude-imp72/memory/` (index `MEMORY.md`); the useful ones are
  `roadmap-state.md` and `cruntime-and-imp-sysroot.md`.
