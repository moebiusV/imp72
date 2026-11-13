# Building and running

    make                  # the seed, the runtime library, stage 1 and stage 2
    make check            # all the tests
    make install PREFIX=/usr DESTDIR=$PWD/tmp/root

## Requirements

- a C99 compiler and a C++17 compiler (the seed and `stage0/impld.cpp`; the
  compilers are also linked with `cc`), GNU make (`gmake` on the BSDs), `ar`, a POSIX shell;
- the LLVM C API: `llvm-dev` (Debian), `llvm-dev` (Alpine), Homebrew's `llvm`,
  and `llvm-config` in the `PATH` or named by `LLVM_CONFIG`;
- lld as a library to link in the process: `liblld-dev` (Debian), `lld-dev`
  (Alpine), Homebrew's `lld`, the same version as LLVM. Without it the driver
  runs `ld.lld` (`ld64.lld` on macOS) as a program, found in the `PATH` and then in
  the `bin` directory of the LLVM it was built with. OpenBSD's own linker is
  lld and it has no library;
- the C library's development files (`crt1.o`, `libc`): the driver links with
  them. It never runs `cc`;
- Python 3 for the fuzzers; the tests that need it are skipped without it.

The seed builds with `make CC=clang` or `make CC=gcc`. The make variables are
`CC`, `CFLAGS`, `LDFLAGS`, `DESTDIR` and `PREFIX` (default `/usr/local`).
`WITH_LLVM=no` and `WITH_LLD=no` (environment) leave LLVM or the lld library out
of the build; without LLVM the compilers say so and stop. `LLVM_CONFIG` names
another `llvm-config`.

## The steps

The compiler is built in three steps (`docs/BOOTSTRAP.md`). The seed compiles
`src/` into stage 1, stage 1 compiles `src/` again into stage 2, and `make
install` installs stage 2.

| target | what it makes |
|---|---|
| `stage0` | `stage0/imps0`, `stage0/impstage0` (the seed) |
| `build/lib/libimp.a` | the runtime library, built by the seed (`tools/mklib.sh`); the compilers are linked with it |
| `llvm-ir` | stage 1 in `build/llvm-ir1`, stage 2 in `build/llvm-ir2` (`tools/llvm-ir.sh`); skipped where there are no LLVM libraries |
| `runtime` | `build/rt-host/libimp.a`, built by stage 2 (`tools/mkrt.sh`) |
| `cimport` | `build/tools/cimport`, the tool `imp-cimport`, an IMP program (`tools/cimport*.imp`) that stage 2 compiles (`tools/imptool.sh`); skipped where stage 2 is not built |

By hand:

    tools/llvm-ir.sh          # stage 1 and stage 2: build/llvm-ir2/impd and impstage-llvm-ir
    tools/direct.sh           # one stage, built by the seed: build/direct/impd, impstage-llvm-ir
    build/llvm-ir2/impstage-llvm-ir -o hello tests/native/hello.imp
    build/llvm-ir2/impd -s TSX -o dump.txt -c ir.txt src/syntax.imc lib/ffi.imc prog.imp

`impd` is the compiler proper (`src/impm.imc`). Its arguments are `-i` (the
original's commands from the terminal), `-I IMAGE` (start from an image saved
with `NAME_/U`), `-s LETTERS` (the original's switches), `-o FILE` (listing and
dumps), `-c FILE` (the IR file), `-T TARGET`, then the syntax file and the
files to compile. `impstage-llvm-ir` and `imp` are the same program with the
driver around it: they find the syntax file and the library, make the object with
LLVM in the process, and link.

The seed alone: `make -C stage0`, then `stage0/impstage0 -o prog prog.imp`,
or `stage0/imps0 -s TSX -o dump.txt src/syntax.imc`.

## Installed layout

    PREFIX/bin/imp                       stage 2
    PREFIX/bin/impstage0                 the seed
    PREFIX/bin/imp-cimport               the declarations of a C header (where stage 2 is built)
    PREFIX/lib/imp/libimp.a            the library stage 2 built
    PREFIX/share/imp/syntax.imc, ffi.imc
    PREFIX/include/imp.h
    PREFIX/share/man/man1/imp.1, impstage0.1, imp-cimport.1
    PREFIX/share/man/man7/imp.7          the language manual
    PREFIX/share/doc/imp/                imp.md, FFI.md, LANGUAGE-NOTES.md

Where stage 2 cannot be built (no LLVM libraries), `imp` is a link to
`impstage0` and the library is the seed's. Both compilers find their parts
relative to their own place, and in the source tree they use the files of the
tree. `IMPSYNTAX`, `IMPFFI` and `IMPLIB` name other files.

### Temporary files

The compilers make no temporary file unless an executable is linked. The
kernel writes the IR text to memory, and `-c`, `-S` and `-Sir` work
from it, so they need no writable directory. For a link the object goes to a
file for the linker:

- On Linux the object is written to an unnamed memory file (`memfd_create`,
  looked up at run time) and the linker is given `/proc/self/fd/N`. No
  directory is made. A linker run as a program inherits the descriptor.
- Otherwise, or if the memory file cannot be made or opened as
  `/proc/self/fd/N`, or if `IMPNOMEMFD` is set to anything but `0`, the
  object goes to a private directory made at that point: `impstage0.XXXXXX`
  made by `mkdtemp` (mode 0700, an unguessable name) in `TMPDIR`, else in the
  default. `TMPDIR` is used only if it is an absolute path naming a directory
  in which a private directory can be made. The default is `tmp/` of the
  source tree, and `/tmp` for an installed compiler. The seed checks with
  `lstat` that the directory is a directory of mode 0700 owned by the
  effective user; `imp` makes it private with `chmod`, which only the owner
  can do. The file in it is created with `O_EXCL`, mode 0600.

The directory and its files are deleted with `unlink` and `rmdir` (no shell) at
exit, on an error, and on `SIGHUP`, `SIGINT`, `SIGQUIT` and `SIGTERM`; the
signal is then raised again with its default action, so the exit status is the
signal's. A signal that was ignored when the compiler started stays ignored.
Compiling several sources with `imp` runs the compiler on each source as a
process (no shell), and their objects are in the directory; a signal stops
the process first. `lld` makes the executable next to its name, as it does
for any link.

## LLVM and lld in the process

`tools/llvm.sh` gives the flags (`cflags`, `libs`, `bindir`, `imp`). The seed
(`stage0/`) and the compilers `tools/direct.sh` builds link `libLLVM`. The back
end makes the object with the host's target machine in the process, so no
`clang` runs.

The link is lld in the process (`stage0/impld.cpp`, built where `tools/llvm.sh`
finds `liblldELF` or `liblldMachO` and their headers), or lld as a program where
there is no library (`WITH_LLD=no` builds so on any system, to test it). The
loader and the C library are those the compiler itself runs with
(`lib/imphost*.iml`: `PT_INTERP`, `dl_iterate_phdr`), so the native link of
Linux works on every architecture and consults no table of them. A compiler
with no loader of its own (a static one) takes the `crt1.o` whose ELF header
(class, byte order, machine) is its own, and the `PT_INTERP` of the C library
beside it (`imphostprobe`). The BSDs keep the start files in `/usr/lib` whatever
the architecture; macOS needs the SDK and has arm64 and x86_64. When a file is
missing the message names it (`cannot link: no crt0.o in /usr/lib`). The only
program the driver starts is `ld.lld` (`ld64.lld`); `-c`, `-S` and `-Sir` start
none.

### Optimization levels

`-O0`, `-O1`, `-O2`, `-O3`, `-Os` and `-Oz` choose the pass pipeline of LLVM
(`default<O0>` and the rest) and the level of code generation (none, less,
default, aggressive for `-O0` to `-O3`; `-Os` and `-Oz` are default), and `-Os`
and `-Oz` put the attribute `optsize` (and for `-Oz` `minsize`) on each function
the module defines. `-O` alone is `-O1`, the last one given wins, and any other
`-O...` is refused with status 2. With none the level is `-Os`, or what
`IMPOPT` in the environment says (`-O2` or `2`), which is how a test suite is
run at another level: `IMPOPT=-O3 make check` builds the compilers and runs the
tests at `-O3`. The seed has no flag and reads `IMPOPT` only. The IR that
`-Sir` writes does not depend on the level, so `tests/seed` compares the seed
and stage 2 at whatever level they were built; `make check` passed with the
compilers built and tested at `-O0`, the default and `-O3`.

## Cross-compiling

The short way: `imp --target=TRIPLE -o prog prog.imp` (or `--target=OS/ARCH`).
With no `--sysroot` and `--runtime` the driver asks `imp-sysroot` (beside it
or in the PATH) for what the link needs of that system, which fetches it once
into `${XDG_CACHE_HOME:-~/.cache}/imp/sysroots/OS[/RELEASE]/ARCH` (`$IMP_SYSROOTS`
moves it) and makes `libimp.a` for the target there. That directory is the
sysroot, as `--sysroot` takes it: `usr/include` (the matching header files),
`usr/lib` with the start files (crt0.o or crt1.o, crtbegin.o), the C library,
libm and libgcc, and the runtime library. A system with releases has the
release in the path (`openbsd/v80/x86_64`, `freebsd/v151/x86_64`,
`netbsd/v111/x86_64`), and so does Linux, by distribution (`linux/debian13/aarch64`
is glibc, `linux/alpine322/x86_64` is musl, chosen by the triple: `aarch64-linux-gnu`
or `x86_64-linux-musl`); Windows has none (`windows/x86_64`, and `windows-ucrt/x86_64` for `--target=x86_64-w64-mingw32-ucrt`, the UCRT of MSYS2's ucrt64 packages in place of msvcrt: its programs link `-lucrt` and import `api-ms-win-crt-*` and kernel32 only; the 38 programs of `tests/winrun` run under Wine with it too). Another release of the
same distribution is `imp-sysroot aarch64-linux-gnu@debian12` or
`x86_64-linux-musl@alpine321`, and `--sysroot=$(imp-sysroot --path T@debian12)`
uses it. A triple whose C library is not the host's (`x86_64-linux-musl` on a glibc
machine) is a cross target, linked with its own files and not the host's. `imp-sysroot --list` says which systems it knows (the table
is `tools/sysroots.tsv`) and which are here, `imp-sysroot --path T` where one
is, `IMPFETCH=0` stops the driver fetching, and `IMP_FREEBSD_VERSION`,
`IMP_OPENBSD_VERSION` and `IMP_NETBSD_VERSION` pick a release other than the
newest.

Nothing is kept in this repository. Each system's files come first from the
`cruntime` repository (`$IMP_CRUNTIME`, default
`https://github.com/moebiusV/cruntime`, its `sysroots` branch (`$IMP_CRUNTIME_BRANCH`; `main` is only the documents), laid out as the cache is:
`openbsd/v80/x86_64`, `linux/debian13/aarch64`; fetched by git sparse
checkout of that one directory or by release tarball), and, where it has none,
from the system's own packages, checked against the checksums of their
indexes: the Debian packages for Linux (glibc) and mingw-w64, the Cygwin
packages, the base (and, for NetBSD, comp) sets of FreeBSD, OpenBSD and
NetBSD, Alpine's packages for musl. macOS has no Apple download (Apple's SDK is not
to be handed out): the SDK of a Mac is used where there is one (`xcrun`, or
`imp-sysroot --apple-sdk DIR`), and elsewhere the Darwin headers and the
`libSystem.tbd` stub of the Zig project's source tarball, which lld links against
in place of the dylib: enough for programs of the C library, not for frameworks.
The provenance and license of every directory, with the versions and checksums of
what it was made from, are in the README of the cruntime repository, which
`imp-sysroot --readme ROOT` writes. Needed here: sh, curl or wget, tar, xz and
`ar` (for `.deb`), a sha256 tool. To run what is made: qemu's user mode for
another machine's Linux, Wine for Windows (`tools/wine-setup.sh`).
`tests/sysroot` checks the table, and with `IMP_TEST_NET=1` the whole way for
Windows and aarch64 Linux.

The long way, by hand, is below.

`--target=TRIPLE` (LLVM's names; the vendor may be left out:
`aarch64-linux-gnu`, `riscv64-linux-musl`, `i686-linux-gnu`,
`armv7-linux-gnueabihf`, `x86_64-apple-darwin`) with `-c`, `-S` or `-Sir`
writes the output for that system, in its object format and with its pointer
size. The byte order of the target is the one LLVM's data layout of the
target machine gives, so a big endian target (`s390x-linux-gnu`,
`powerpc-linux-gnu`, `powerpc64-linux-gnu`) is compiled on a little endian host
and the reverse: the words of text constants, the data of pointers and the
runtime library are made for the target's order. 
The triple is checked before LLVM is asked for the target. After LLVM
normalises it, it must be made of letters, digits, `.`, `_` and `-`, and
`stage0/lvtriple.cpp` asks LLVM's own triple parser (`llvm::Triple`) about it:
the architecture, the operating system, the environment and the object format
(`ARCH-VENDOR-OS[-ENVIRONMENT[-FORMAT]]`) must be names that parser knows
(`linux`, `freebsd`, `hurd`, `emscripten`, `windows`, `gnu`, `musl`, `android`,
`msvc`, `elf`, `macho`, ...), so the set follows the installed LLVM; `unknown`
and `none` are accepted where the triple says them. The vendor is free text
(`alpine`, `pc`, `apple`), and a version may follow the system (`freebsd15.0`).
Where the LLVM C++ headers are not installed, the check is lenient: it only
looks at the characters, and LLVM's answer for the target decides. Otherwise the
message, with the part that is wrong, is

    imp: unrecognized target "TRIPLE": unknown operating system "nonsense" (expected ARCH-VENDOR-OS[-ENVIRONMENT], for example x86_64-linux-gnu)

the status is 1 and no file is written (`x86_64`, `x86_64-nonsense-os` and
`x86_64-linux-nonsense` are refused). An architecture that the installed LLVM
was built without gets LLVM's own message. The host's own triple is not
checked.

Linking for another system (Linux with glibc or musl, FreeBSD, NetBSD, OpenBSD
and macOS; the ELF and Mach-O drivers of lld are both in the compiler, so a
Linux build links macOS programs and the reverse) takes the dynamic loader from
the triple and needs that system's files. The Linux architectures of the table are `x86_64` (and the x32 ABI), `i386`
to `i686`, `arm` and `armeb` (soft and hard float), `aarch64` and `aarch64_be`,
`ppc`, `ppc64`, `ppc64le`, `s390x`, `riscv32`, `riscv64`, `loongarch32`,
`loongarch64`, `mips`, `mipsel`, `mips64` and `mips64el` (also the n32 ABI),
`sparc`, `sparc64`, `m68k` and `hexagon`. The loader of a Linux target is the
standard one of its architecture and C library (`/lib64/ld-linux-x86-64.so.2`,
`/lib/ld-linux.so.2`, `/lib/ld-linux-armhf.so.3`, `/lib/ld-linux-aarch64.so.1`,
`/lib/ld-linux-riscv64-lp64d.so.1`, `/lib64/ld64.so.2` and `/lib64/ld64.so.1`
for powerpc64le and powerpc64, `/lib/ld.so.1` for powerpc, `/lib/ld64.so.1` for
s390x, `/lib64/ld-linux-loongarch-lp64d.so.1`, the MIPS, SPARC and m68k ones,
and `/lib/ld-musl-ARCH.so.1` for musl); the BSDs' are `/libexec/ld-elf.so.1`,
`/usr/libexec/ld.elf_so` and `/usr/libexec/ld.so`. A 32-bit target also gets
`libgcc.a` from `DIR/usr/lib/gcc/TRIPLE/VERSION` when the sysroot has it, for
the division of 64 bits. A target the driver cannot link (WebAssembly, Android,
iOS, Windows with the MSVC environment) is refused with the system named:
`linking for wasi is not supported; use -c to make the object`. The files:

- `--sysroot=DIR` (or `IMPSYSROOT`): the root of that system, with `crt1.o`,
  `crti.o`, `crtn.o` and the C library under `DIR/usr/lib/...` (`crt0.o` and
  `crtbegin.o` for a BSD), or for macOS the `MacOSX.sdk` directory;
- `--runtime=FILE` (or `IMPRUNTIME`): the `libimp.a` built for it.

Without them the error message says what is missing (`cannot link for
s390x-linux-gnu: no crt1.o under DIR/usr/lib/s390x-linux-gnu, ...`). The link covers ELF (Linux, the BSDs), Mach-O (macOS) and COFF through the MinGW driver (mingw-w64 and Cygwin). For WebAssembly and the MSVC environment the compiler writes objects only.

`sh tools/mkrt.sh TRIPLE` builds the runtime library for a target with `imp`
and no C compiler, into `build/rt-TRIPLE/libimp.a`; with no triple it builds
this machine's, `build/rt-host`. It takes the byte order and the word size of the target from the object file
the compiler makes for it, and the constants of the target's C
library from `tools/sysconst.sh`. A system that script cannot describe from the
build machine (the size of a `FILE` on NetBSD) is refused: build the library
on that system. For a 32-bit target it adds the target's compiler-rt members
from the LLVM at hand (`tools/llvm.sh builtins TRIPLE`) and says so when they are
missing.

### The linker and its flags

The linker is the first of these that exists: lld in the compiler's own process
(`liblldELF`, `liblldMachO`, `liblldMinGW`; `stage0/impld.cpp`), `ld.lld` or
`ld64.lld` as a program (the `PATH`, then LLVM's bin directory), the system's own
`ld` (GNU ld, gold, a BSD base ld, Apple's ld64; for a mingw target
`x86_64-w64-mingw32-ld` and its kin), which takes the same arguments, as it does
when gcc and clang run it. `IMPLDSHOW=1` prints the arguments and a line
`linker: NAME` for the one chosen. When none is found the message names what was
looked for (`no linker found: ld.lld is not in the PATH ..., and there is no
plain ld in the PATH`).

The arguments come from tables, in `src/linktab.imc` (architecture, loader,
directory of the C library, `-m` emulation of the target) and `src/driver.imc`
(the system families); the seed's driver `stage0/imps0.c` links this machine
only, with the same arguments for it. `tests/link/cross.sh` checks them for many
triples against `tests/link/cross.out`.

| target | linker flavour | arguments |
|---|---|---|
| this machine, Linux | ELF | `-o OUT -dynamic-linker LOADER CRT1 CRTI OBJECTS LIBIMP -L DIR -lc [-lm] CRTN`; the loader and `DIR` are the running program's own |
| another Linux (glibc, musl) | ELF | the same, with `-m EMULATION` (`elf_x86_64`, `elf_i386`, `armelf_linux_eabi`, `aarch64linux`, `elf64lppc`, `elf64ppc`, `elf32ppclinux`, `elf64_s390`, `elf64lriscv`, `elf64loongarch`, `elf32btsmip`, ...) first and `--sysroot=DIR`, the loader of the architecture, `libgcc.a` of the sysroot after `-lc` when it has one |
| FreeBSD, NetBSD, OpenBSD | ELF | `--eh-frame-hdr -dynamic-linker LOADER -o OUT CRT0/CRT1 [CRTI] CRTBEGIN OBJECTS LIBIMP -lc [-lm] [libcompiler_rt.a] CRTEND [CRTN]` (OpenBSD: `libc.so.N.M` and `libm.so.N.M` by name, no `crti.o`); `-m` for a target (`elf_x86_64_fbsd`, `elf_i386_fbsd`, `aarch64fbsd`, `elf_x86_64`, ...) |
| macOS | Mach-O | `-arch arm64\|x86_64 -platform_version macos MIN MIN -syslibroot SDK -o OUT OBJECTS LIBIMP -lSystem`; `ld64.lld`, or Apple's `ld` |
| Windows, mingw-w64 | MinGW | `-m i386pep\|i386pe\|arm64pe\|thumb2pe -o OUT crt2.o OBJECTS LIBIMP -L DIR -lmingw32 -lmoldname -lmingwex -lgcc -lmsvcrt -ladvapi32 -lshell32 -luser32 -lkernel32`; `DIR` is `SYSROOT/usr/TRIPLE/lib` (or `TRIPLE/lib`, `lib`, `usr/lib`), where `libgcc.a` must be as well |
| Cygwin (`x86_64-pc-cygwin`, LLVM's `x86_64-pc-windows-cygnus`; `i686-pc-cygwin`) | MinGW | `-m i386pep\|i386pe -o OUT crt0.o [crtbegin.o] OBJECTS LIBIMP -L DIR [-L GCCDIR] -lcygwin -lgcc -ladvapi32 -lshell32 -luser32 -lkernel32 [crtend.o]`; `DIR` is `SYSROOT/usr/TRIPLE/lib` (or `TRIPLE/lib`, `lib`, `usr/lib`), `GCCDIR` is `SYSROOT/usr/lib/gcc/TRIPLE/VERSION` |

The Cygwin row is unverified, and the link does not complete: with the files imp-sysroot fetches (the Cygwin packages; the import libraries are under usr/lib/w32api, which the driver adds) lld stops at four symbols that GNU ld's script for Cygwin defines and lld does not, `__data_start__`, `__data_end__`, `__bss_start__` and `__bss_end__` (the Cygwin library reads them to know the range of the data of the program, for fork). Making them is a matter of the sections, not of a switch, and cannot be tried here without Cygwin to run the result. It follows the specs of Cygwin's gcc and has not been run on Cygwin or checked against clang's Cygwin toolchain. The start file `crt0.o`, the libraries and the gcc directory are the likely ones. The entry point, the image base and `--dynamicbase`, `--nxcompat` and `--tsaware` (lld's defaults are used) are unverified. The tests cover the argument lists (`tests/link/cross.sh`) and objects, not a run.

The runtime library for Windows (mingw-w64, the C library msvcrt: `tools/mkrt.sh x86_64-w64-mingw32`) is built and runs: `tests/winrun` compiles and links 38 of the test programs
with `imp --target=x86_64-w64-mingw32 --sysroot=... --runtime=...` and runs them under Wine, and they print what the native tests print. Its parts: the standard files of msvcrt
(`__iob_func()`, in `tools/sysconst.sh`), put in binary mode when the runtime starts (`IMPBINMODE`, so that a program writes the bytes it means and not CR LF for LF; the files
`impio` and `impchan` open are `"rb"` and `"wb"` for the same reason), the arena of the compiler's free storage by `VirtualAlloc` (`lib/impcore-win.iml`, where
`lib/impcore.iml` has `mmap`), the `_longjmp` of the non-local `GO TO` that msvcrt calls `longjmp` (`lib/impwin.iml`), and a `_setjmp` with the null frame that msvcrt's takes as its second
argument (`src/ir2llx.imc`, for the Windows target only). `RENAME` of a channel closes the file first, which Windows needs of a file it deletes or renames. Left out of the Windows
run, because they call C functions that Windows does not have: `structs` (`strptime`), `cdecl` (`htonl`, which is in ws2_32), `cdecl2` (the ctype functions with values msvcrt treats otherwise) and
`extcall` (`environ`). What is not done: the compiler itself on Windows (`lib/imphost.iml` forks and execs: `CreateProcess` is not written, the images need a table of the variables
that COFF has no section for), the UCRT (the runtime is msvcrt's), 32-bit Windows, ARM, and a run on a real Windows machine: Wine is what the programs run on. `tools/wine-setup.sh` makes the
mingw-w64 libraries and Wine from the Debian packages into `tmp/win` (nothing is installed; `apt-get download`, `dpkg-deb`, and `unshare` for the namespace that gives Wine `/usr/share/wine`).
The Cygwin library is built and has not run.

What works for Windows targets: objects (`-c`, `-S`, `-Sir`), and the link of a
program that has its own `main` and needs no runtime (`--runtime=` an empty
archive).

## Tests

`make check` runs, under `tools/guard.sh`:

| test | checks |
|---|---|
| `make -C stage0 check` | the seed's unit tests (`tests/stage0`) and its syntax test |
| `tests/native/run.sh` | each `tests/native/NAME.imp` is compiled, linked with `NAME_main.c` where there is one, run, and compared with `NAME.out` |
| `tests/oracle/run.sh` | the programs of `tests/oracle/LIST` against the output the original compiler printed (`docs/ORACLE.md`) |
| `tests/ieee/run.sh` | random real arithmetic against an independent IEEE 754 reference |
| `tests/direct/run.sh` | the native and oracle tests, the IEEE fuzz and a differential fuzz through the compiler the seed builds |
| `tests/direct/stage2.sh` | a compiler built by the seed and one built by that compiler write the same IR for every program |
| `tests/direct/ir2ll.sh` | the IMP LLVM IR writer and `tools/ir2ll.c` write the same text |
| `tests/interactive/run.sh` | a bare `imp`, the original's commands |
| `tests/image/run.sh` | saving the compiler with `/U` and starting from it with `-I`, including an image restored with the free storage at another address |
| `tests/llvm/gen.sh` | `tools/llvm.sh imp` in each of its shapes |
| `tests/link/run.sh` | the link with lld in the process, with no `cc` on the `PATH` |
| `tests/link/cross.sh` | the arguments of ld for each architecture and C library of the table of loaders, the BSDs, and the refusals, with a sysroot of empty files |
| `tests/crossrun/run.sh` | programs compiled and linked for `s390x-linux-gnu` and `powerpc-linux-gnu` (both big endian) and `i386-linux-gnu`, and run (the first two under qemu's user mode emulation, no system emulation) with sysroots that `tests/crossrun/sysroot.sh` unpacks from Debian `.deb` files in the directory `PREP_INPUTS` (default `../prebsd/mirror/dev/inputs` beside the repository root; it holds `debian-ARCH/pool`); skipped (exit 77) where `dpkg`, those files or the emulator are absent |
| `tests/version/run.sh` | every place that names the version (`VERSION`, the compiler's `--version` and banner, the packages, the port, the manual) agrees |
| `tests/variadic/run.sh` | every `CCALL` in the tree's IMP sources of a C function that takes a variable number of arguments (`printf`, `open`, `fcntl`, ...) has a declaration with `...` before it in the same compilation unit |
| `tests/runtime/run.sh` | the runtime library built by `imp`, for the host and other targets |
| `tests/tmpfiles/run.sh` | temporary files of the seed and of `imp`: none for `-S`, `-c` and `-Sir`; a link leaves `TMPDIR` empty, with and without the memory file, after an error and on `SIGHUP`, `SIGINT`, `SIGQUIT` and `SIGTERM`; a `TMPDIR` that is relative, a file or missing is not used |
| `tests/seed/run.sh` | the seed and stage 2 write the same LLVM IR |
| `tests/cli/run.sh` | the command line of `imp` |
| `tests/cimport/run.sh` | `imp-cimport`: golden outputs for preprocessed text, compiled by the seed and by `imp`; with `cc`, the declarations of `string.h`, `stdlib.h`, `stdio.h` used by a program, the layouts of `struct tm`, `timespec` (and `stat`, `dirent` on Linux) against `offsetof`, the values of macros against the C compiler's, and no file left in `TMPDIR` |
| `tests/fuzz64/run.sh` | random integer, byte-pointer and real programs against 64-bit rules worked out in Python |
| `tests/manual/run.sh` | `docs/manual/imp.md` and `imp.7` are what the RUNOFF converters make of `decus/imp.rno` |
| `tests/tokdiff/run.sh` | `tools/token-diff.imp` writes the HTML of two versions of a program token by token: white space is no difference, `_` replaced by `:=` is a replacement, a golden page, and the whole tape against the port |

### Scripts the tests share

- `tools/longbits.sh` prints 32 or 64, the width of a C `long` on this machine
  (`getconf LONG_BIT`, else from the machine type, because OpenBSD and NetBSD
  have no `LONG_BIT`).
- `tools/nativesrc.sh SOURCE DIR` prints the file to compile for a native test:
  the source itself, or on OpenBSD a copy in `DIR` that calls
  `srand_deterministic` for `srand`, because OpenBSD's `srand` ignores its seed.
- `tools/mktmpdir.sh` is sourced; `mktmpdir NAME` sets `$t` to a new private
  directory (`mktemp -d`, mode 0700, in an absolute `TMPDIR`, else `/tmp`),
  deleted when the script ends or gets `SIGHUP`, `SIGINT` or `SIGTERM`.

## The seed and `imp`

The seed (`stage0/impstage0`) and `imp` read the same language and write
the same LLVM IR for the same program. `-c`, `-S`, `-Sir` and `-o` are the same
in both, with the same file names. Their command lines differ:

| | `impstage0` | `imp` |
|---|---|---|
| `--target`, `--sysroot`, `--runtime` | not options: the seed makes code for this machine only | cross-compiling (below) |
| `-I`, `-i`, `--version`, `--help` | not options (`--help` is an unknown option, status 2) | images, the original's commands, version, usage |
| several sources | compiled together as one program, output named after the first | each compiled by itself |

`tests/seed` compares the seed's `-Sir` text with `imp -Sir`. The optimization level
is `-O` for `imp` and only `IMPOPT` for the seed (above).

## Diagnostics of the free storage and the images

The free storage of the compiler is an arena of address space reserved once
(`lib/impsys.iml`), asked for at the address `SCHINT` of `tools/sysconst.sh`. An
image (`/U`, `-I`; format version 6) is restored with the arena at any address: if the system
gives the one the image was saved at, nothing is moved; otherwise the
addresses of the arena that the modules' variables hold are moved with it
(`src/image.imc`). These variables of the environment are for tests and
diagnostics:

- `IMPCOREBASE=ADDR` (decimal, or `0x` hexadecimal) asks for the arena at
  ADDR instead of `SCHINT`. `tests/image/run.sh` saves an image at one address and
  restores it at others with it. The address must be aligned to a page.
- `IMPCOREDEBUG=1` reports on the standard error where the arena was asked for
  and where it was put, and, when an image is restored, the address it was
  saved at.
- `IMPIMGCENSUS=1` at the saving of an image lists the words of the
  variables and of the arena that hold an address of the arena, and the
  variables that hold an address near the program's own data.

## Fuzzers

`tests/fuzz64/run.sh` compiles random programs with the seed and with `imp`
and compares what they print with an answer worked out in Python from the
rules of the 64-bit word, never from a compiler. Three generators:

- `tools/imp-fuzz64.py`: integers (`+ - * /`, `AND OR XOR NOT`, comparisons,
  shifts), array indexing with offsets, bit-fields read and stored.
- `tools/imp-fuzz-bytes.py`: byte pointers (`BYTEP P[k]<S,Q>`, the pointer to
  characters `<7,36>`, `<X>`, `<X>:=E`, `<+X>`, `<+X>:=E`, the step to the next
  field and to the next word) and block moves (`MOVE A THROUGH N TO B`).
- `tools/imp-fuzz-real2.py`: REAL over the whole double (denormals to overflow,
  signs of zero, infinities), the bits of the result, the six comparisons,
  `FLT` of integers around 2**53 and 2**63, `FIX`. It generates no NaN and no
  `FIX` past 2**63, where machines differ.

`tools/fuzz64.sh DRIVER SEED COUNT` runs one (`GEN=imp-fuzz-bytes.py`), and
`tools/fuzz64-reduce.py` reduces a wrong integer program to the smallest wrong
sub-expression. `tools/ieee-fuzz.sh SEED COUNT` checks the printed text of REAL
values (`PRINT FLT`).

## Continuous integration

`.github/workflows/` has four workflows: `ci-linux.yml` (amd64, arm64, the Debian
package), `ci-alpine.yml` (musl: amd64, arm64, ppc64le, riscv64, s390x,
loong64, 386, armv7, under QEMU where the runner is another architecture),
`ci-bsd.yml` (FreeBSD, NetBSD, OpenBSD in virtual machines) and `ci-macos.yml`.
To say which a push runs:

- a commit message with tags runs only the groups it names: `[ci:linux]`,
  `[ci:alpine]`, `[ci:bsd]`, `[ci:macos]`, several together (`[ci:bsd][ci:macos]`);
  `[ci:loong64]` runs only the loong64 job of the Alpine workflow; without a tag
  all four run; the tags of the last commit of the push count;
- `[skip ci]` in the message skips every workflow;
- a push that changes only documents (`docs/`, `*.md`, `decus/`, `oracle/`,
  `LICENSE`, `.gitignore`) runs none;
- "Run workflow" in the Actions tab starts one workflow by hand;
- a push cancels the runs of the same workflow that are still going; one job
  can be run again from the page of its run.

`tools/ci-run.sh NAME` (build and `make check`, failures as GitHub annotations,
log in `tmp/ci-NAME.log`) is what the BSD and Linux jobs run. `tools/ci-host.sh
NAME` records the machine, the CPU and the triple, CPU and features that LLVM
reports (`IMPLLVMINFO=1`) as an annotation of the run. The Debian job
builds the package with `dpkg-buildpackage`, installs it and compiles a program
with it.

`tools/ci-arch.sh ARCH STAGE` runs the same in an Alpine container on one's own
machine (Docker and, for another architecture, QEMU user mode registered with
`binfmt`), one stage at a time (`tools/ci-container.sh` runs inside the
container, and writes `STAGE.log`, `STAGE.status` and `STAGE.heartbeat` to the
work directory): `build`, `check`, `link` (Docker's architecture
names: `arm64`, `riscv64`, `ppc64le`, `s390x`, `amd64`, `386`, `arm/v7`,
`loong64`). The copy of the tree, the build and the log of every stage are kept in a
work directory (`tmp/ci-ARCH` locally, `work` in CI) that is mounted into the
container and, in CI, saved in the Actions cache even when a stage fails. A
finished stage leaves a stamp and is skipped when asked for again; one that
failed leaves none and runs again, `make` going on from the objects it has.

## Runaway jobs

`make` and `make check` run under `tools/guard.sh`, which kills the whole job
(its process group) when `build/` and `tmp/` together pass 10 GB (`-g GB`,
`-m MB`; `GUARD_OFF=1` turns it off; `tmp/vm`, the images of `tools/bsd-vm`, does
not count). The scripts source `tools/limits.sh` (2 GB for a file one process
writes, 40 minutes of CPU for one process, best effort;
`IMP_NOLIMITS=1` leaves it out), and a log that may grow goes through
`tools/capped.sh FILE CMD...` (at most 200 MB, the beginning, which says why).
The scratch directory is `tmp/` of the tree (not in git).

## The BSDs on one's own machine

`tools/bsd-vm/bsd-vm.sh` runs OpenBSD 8.0 and NetBSD 11.0 (the machines of the
BSD jobs) and FreeBSD 15.0, all amd64, under QEMU with KVM in a container (docker and `/dev/kvm`
are all it needs), so that a failure of those jobs can be examined with a shell
and a debugger.

    sh tools/bsd-vm/bsd-vm.sh fetch openbsd     # (or netbsd, freebsd) the image of the cross-platform-actions project, once
    sh tools/bsd-vm/bsd-vm.sh start openbsd     # a fresh machine on a copy-on-write overlay: the image is never written to
    sh tools/bsd-vm/bsd-vm.sh ci openbsd        # copy the tree (committed and not) and run tools/ci-run.sh there; `ci openbsd check` runs only gmake check
    sh tools/bsd-vm/bsd-vm.sh ssh openbsd [CMD] # a shell as runner
    sh tools/bsd-vm/bsd-vm.sh stop openbsd      # halt the guest from inside, then the container

`status` and `clean` complete the commands. Everything is in `tmp/vm/OS`. Stop
a machine with `stop`, not by killing it: an OpenBSD killed while it relinks
its kernel at boot comes up with files missing and needs `clean` and `start`
again. `VM_CPUS` (default `6-9`) and `VM_MEM` (default `5G`) set the CPUs and
the memory. The OpenBSD image comes up without a carrier on its network card and
has no login on the serial port; the tool logs in on the video console through
the QEMU monitor (`tools/bsd-vm/type.imp` writes the `sendkey` commands) and
brings the interface up.

## Packages

`packaging/README.md`.
