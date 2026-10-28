# The C in `imp72`, and moving it to IMP

The rule (README, "What is next", item 2): in the compiler `imp72` nothing is C except the seed (`stage0/`, the trusted stage 0,
which also stays what builds stage 1) and the glue to LLVM ORC (`stage0/llvmorc.c`, with `tools/ir2ll.c` as a function). What
needs the operating system or the C library is called from IMP through `CCALL` (`docs/FFI.md`).

This is the survey of what is C in `imp72` now (stage 2, `tools/direct.sh`), what each piece needs, and the order of the move.
`wc -l` counts; "seed too" marks code the seed (`imp72stage0`) also uses, which stays C there, so the IMP version is a second
implementation for `imp72` and the seed keeps its own.

## What is linked into `imp72` as C

| file | lines | what it does | needs | seed too |
|---|---|---|---|---|
| `stage0/*_imp.c` (13 files) | ~900 | C entry points over the IMP modules: `foo(word)` calling `impl_foo(&x)`. The C that remains calls the IMP modules through them | nothing: they go when their callers go | no |
| `stage0/pccirsem.c` | 340 | `impstrtod`; `diri2d`, `dird2i`, `dirhigh` (integer to double and back, as halves); `irsymname`, `irkind`, `irstrval`, `irlabname`, `irpoolemit`, `irfnkind`, `irruntime`, `irtagerr`, `irjbname`, `ixcall` (names and shapes of the IR); `trans_open`, `pcode`, `ir_symreset`; the table of the IMP semantic routines registered with `dsemi` | `snprintf`, `strtod`, the directory (`DNAME`), `realloc` | no |
| `stage0/ir.c` | 474 | the builders of the pass-2 IR tree (`ir_bin`, `ir_un`, `ir_name`, `ir_temp`, ...) and its printer; `IXBIN`, `IXSYM`, `IXPASS` and the other `CCALL`s of `src/pccir/demit.imp` land here | `printf`, `malloc` | yes (`trans.c`) |
| `stage0/semtab.c` | 127 | `DSEM(name, routine)` table of the semantic routines, `semcall` calls one by id with its parameters by reference | a call through an address | yes |
| `stage0/sys.c` | 212 | `imp_out`, `impopenlist`, `impopenir`, `imp_error`, `imp_msg`, `impfatal`, `impgetc`, `impfclose`; the arena: `impcorebase` (`mmap` of the address space reserved once), `impcoreset` | `fopen`, `fclose`, `mmap`, `mprotect`, `exit` | yes (`imp_error`, `imp_msg`) |
| `stage0/image.c` | 375 | `/U`: `impimgsave`, `impimgload`: the variables of the modules (section `impvars`), the arena, the semantic routine table by name, relocation | `dl_iterate_phdr`, the section start and stop symbols, `fopen`/`fwrite`/`fread` | no |
| `stage0/imp72s0.c` | 616 | the driver: options, `findclang`, the back end choice, spawning `ir2c`/`clang`/`cc1`/`cc`, `--run`, `-I` | `fork`/`system`/`exec`, `mkdtemp`, `readlink`, `realpath`, the calls into `llvmorc.c` | the seed's driver is the same file |
| `stage0/pr.c`, `flavour0.c` | ~90 | `PRINT`'s items for the C that prints; `impk_main` | | yes |
| `lib/imp72rt.c`, `imp72io.c`, `imp72main.c` | 800 | the runtime library every compiled program links: `PRINT`/`READ`/files (the tape's `PMOD`, `TBIN`, `TPRIN`, `TRAED`), `IFIX`, `fini`, `main` | stdio | yes: the seed's programs link it |
| `tools/ir2c.c` | | the C back end: pass-2 IR to C, for hosts without LLVM | | no |
| `stage0/llvmorc.c`, `tools/ir2ll.c` | 150, ~700 | **stay**: the ORC glue and the LLVM IR writer | the LLVM C API | no |

## What the move needs

1. **A call through an address.** The semantic routine table holds the address of an IMP routine and `DOSEM` calls it with the
   parameters by reference (the tape patches the address into a `JSA`). IMP needs a way to call a routine given its address; `ffi.imc`
   can give it, as `CCALL` gave the call by value (the extension is the FFI's, not a change of IMP72's syntax).
2. **Doubles.** `CCALL` passes and returns a `long`. Where C needs a `double` (`strtod`) `sscanf` writes it through a pointer; the
   integer to double conversions of `pccirsem.c` are `FLT` and `FIX` of a `REAL` in IMP (the seed translates them now).
3. **Linker symbols.** The image uses the start and stop of section `impvars` and `dl_iterate_phdr`. From IMP these are `dlsym`
   (the program is linked `-rdynamic`) or a table of the variables the module writes itself (the tape has one for `/U`: the high segment).
4. **`main`.** A C program starts in `main`; `imp72main.c` (14 lines) calls `imp_main`. It stays as the program's start, like `crt0`.
5. **The runtime library.** The seed's programs link `libimp72.a`, so the library is built before any IMP compiler exists. Making it
   IMP means building it with the seed, or leaving it C as the seed's part. Not decided: it is for the user.

## The order

1. `pccirsem.c`'s numeric helpers: **done** for `impstrtod` (`FCON` calls `SSCANF`), `diri2d` and `dird2i` (`FLOAT` and `FIX` of a constant use `FLT` and `FIX` on a `REAL`; a `REAL` passed as an argument is made an integer by `IFIX`, so the bits go through an integer with `MEMCPY`). `dirhigh` stays until `irstrval` moves (step 3).
2. `sys.c`: `IMPK` already is IMP (`src/impm.imp`); the files, `ERROR`, `MSG` and the arena (`CCALL MMAP`) next to it.
3. The IR builders (`ir.c`) as IMP, into the IR text directly (`src/pccir/ix.imp`), then `pccirsem.c`'s names (`IRSYMNAME` ...).
4. The call through an address (`ffi.imc`), then `semtab.c`.
5. `image.c`.
6. The driver (`imp72s0.c`) as IMP in `impm.imp` and a `src/driver.imp`; with it the `*_imp.c` wrappers go.
7. Then the runtime library and `ir2c`, once decided.

Each step: the build and `make check` pass before the next; the C it replaces is deleted from `imp72`'s link (and stays in the
seed where the seed uses it).
