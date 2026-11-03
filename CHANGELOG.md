# Changes

## 0.9.0

IMP72 (Ned Irons's extensible systems language, in Walter Bilofsky's PDP-10 compiler of 1973, DECUS 10-198a) on modern machines: words of 64 bits, IEEE doubles, UTF-8 text, Unix.

- **The compiler is IMP.** The kernel, the semantics, the code generator, the driver, the LLVM glue, the IR writer and the runtime library are IMP, compiled by themselves (stage 2). The only C is
  the seed (`stage0/`, which builds stage 1 and stays C), the lld glue (`stage0/impld.cpp`, lld has no C interface) and the tests of C calling IMP.
- **The back end is LLVM in the compiler's own process** (ORC for `--run`, object files, LLVM IR with `-S`), and the programs are linked by lld in that process. There is no host C compiler
  in the path: the package needs the C library's development files. `--target=TRIPLE` makes objects for other systems, and links for them given `--sysroot` and `--runtime`.
- **The runtime library is built by `imp72` itself, for any target** (`tools/mkrt.sh TRIPLE`; `make install` installs the one stage 2 made). A library made on x86-64 for aarch64-linux-musl linked nine
  test programs that ran on aarch64.
- **The seed has no PDP-10 in it.** It reads the 64-bit syntax (`src/llvm-ir/syntax64.imc`) and its code generator (`stage0/gen_*.c`) is a C port of `src/llvm-ir/*.imp`; the seed and stage 2 write the
  same LLVM IR byte for byte (`tests/seed`). The comparison of the seed's dumps and `/A` listings with the real TOPS-10 compiler is retired with the 36-bit words; the program outputs are still
  compared with it (`tests/oracle`).
- **Platforms (CI):** Linux amd64 and arm64 (and the Debian package), macOS arm64, FreeBSD, OpenBSD, NetBSD, and Alpine containers of amd64, i386, arm64, armv7, riscv64, ppc64le, s390x and loong64.
  `tools/bsd-vm/bsd-vm.sh` runs the OpenBSD and NetBSD machines of CI on one's own computer.
- **Language:** assignment is `:=` and `_` is a letter, so IMP names are C and ELF names; names are case sensitive; `CCALL` calls C by value; `EXTERNAL` names exact C symbols; `impcallp`/`impcallw` call
  through an address.
- **Safety:** `make` and `make check` run under `tools/guard.sh`, which kills a job that grows `build/` and `tmp/` past 10 GB.
- **Open, for the user to decide before 1.0:** `docs/porting/tape-names.md` (the routines of the tape against the port's) and `docs/porting/pre-1.0-studies.md` (eight questions about the language, studied and not
  implemented).
