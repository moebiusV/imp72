# IMP72 bootstrap on modern Unix, with pcc as the back end

## Context

IMP72 is Ned Irons's extensible systems language. This plan works from Walter Bilofsky's PDP-10 compiler, DECUS 10-198a v1.5 (1973). Most of the language is not built into the compiler. A small kernel (lexer, Irons parser, syntax graph, code-generation tree, semantic stack) starts empty. `RSYN` reads a tiny syntax-statement notation, and `syntax.imc` then defines the whole language in `<CLASS> ::= elements ::= semantics` statements. Those semantics call routines that emit PDP-10 instructions (`code*.imc`, `amod`/`rmod`/`omod`).

**Goal:** IMP72 runs natively on Linux, the BSDs and macOS, interoperates fully with ELF binaries and the C ABI through FFI, and is fully documented.

**Route:**
1. Hand-port the machine-independent kernel to C, up to and including `RSYN`.
2. At that point, stop producing PDP-10 code and produce pcc IR.
3. Self-host: the C kernel compiles the ported IMP sources of the compiler, until a fixed point.

pcc (github `moebiusv/pcc`, already at `~/claude/pcc`) is the code generator.

**Inputs already present:**
- `~/claude/hbnf/corpus/imp72/tape/`: the 30 text files of the tape, with `SHA256SUMS` and a provenance README (PD per pdp-10.trailing-edge.com, `decuslib10-03/01/43,50306`).
- The archive also holds `imp.low`, `imp.shr` and `implib.lib`.
- **Gap:** PMOD, the formatted print module linked by `r.cmd`, exists only in binary form. "RSYN2" is named in the manual but has no file; it is probably folded into `dosem`/`rsyn`, to be confirmed.

## Decisions (made with the user)

| Topic | Decision |
|---|---|
| Stage-0 language | **C99 (plus GNU extensions pcc accepts)**, linked **in-process** with pcc's `mip/` and `arch/` objects (the f77 precedent). Stage 0 and `backend/` must **compile with pcc itself**. GNU extensions are allowed, but only ones pcc accepts (CI is the arbiter); nothing from C11 on. CI builds stage 0 twice, with the host cc and with pcc. The whole toolchain then depends only on pcc |
| Word model | **64-bit word (`int64_t`), byte addresses.** `A[i]` scales by 8. Strings are NUL-terminated bytes. The compiler's own IMP sources get a documented port that removes the 36-bit idioms |
| pcc interface | **IR, not AST: pcc pass-2 `NODE` trees in `struct interpass`, fed to `pass2_compile()`** (rationale below) |
| Targets | pcc: amd64 and aarch64 ELF (Linux, FreeBSD, NetBSD, OpenBSD), amd64 Mach-O (macOS). **C-emitting fallback back end** from the same IR: macOS arm64 and any other host |
| Oracle | **SIMH + TOPS-10** running the original `imp.shr`, for differential tests and to recover PMOD |

**Why IR rather than AST.** pcc's pass-1 trees (`P1ND`, `cc/ccom`) carry C symtabs, `tdef`s and `buildtree` type rules, and they need most of ccom linked in. That fights a word-oriented, essentially untyped language. It would also duplicate what IMP72's own code tree already does: `cotree`/`entree` do pattern-matched semantic selection, constant folding (`CONOP`) and special cases. The pass-2 interface is small and documented by use:
- f77 already uses it: `f77/fcom/putscj.c` with `puthead`, `sendp2`, `putbracket` and `putlabel`, building trees through `mklnode`/`mkbinode`/`mkunode`/`ipnode` (`mip/reader.c:1730-1781`).
- It has a textual twin, the TWOPASS format (`cc/ccom/trees.c:2742`, read by `mip/reader.c:342-595`). We emit that format too (`--emit-ir`) for golden tests and debugging.

The costs are known. We write a thin per-target layer for call lowering (modeled on `arch/*/code.c` `funcode`/`argput`/`bfcode`), function labels, data and segments, and float constants in memory (`myp2tree`). With one word type plus `REAL`=double, that layer is small.

**IR layering (so the C fallback shares everything).** The front end produces **IMP-IR**: pcc `NODE` trees in a *pre-ABI* form. Calls are `CALL(name, CM-list)`, parameters are `PARAM n` and locals are TEMPs. From there, one of two paths:
- **(a)** target lowering (`backend/abi_amd64.c`, `abi_aarch64.c`), then `pass2_compile()`;
- **(b)** `backend/cemit.c` prints portable C (`int64_t`/`double`) for the host `cc`.

## Repository layout (`~/claude/imp72`, `git init`)

```
decus/            tape files verbatim + SHA256SUMS + PROVENANCE.md; binaries (imp.low, imp.shr, implib.lib) fetched + hashed. Never edited.
third_party/pcc   git submodule -> github moebiusv/pcc, pinned; any pcc fixes go to a branch of that fork
stage0/           C99 kernel: free.c dir.c stack.c lex.c parse.c graph.c ercor.c entree.c cotree.c dosem.c rsyn.c sem-pdp10.c main.c
backend/          ir.c (IMP-IR builders = new semantic primitives), abi_amd64.c, abi_aarch64.c, data.c, twopass.c (text IR), cemit.c (C fallback), pccglue.c (globals pcc expects)
src/              the compiler's IMP sources, ported: syntax.imp (retargeted), impsyn.imp, lex.imp ... rsyn.imp, pmod.imp (reconstructed)
lib/              libimp72 runtime: PRINT/PMOD, tbin/tprin/traed ports, POSIX shims replacing TOPS-10 UUOs (CALLI, OUTSTR, INIT/LOOKUP/ENTER)
oracle/           SIMH+TOPS-10 scripts, disk-image packing, capture of golden listings/dumps, PMOD disassembly notes
tests/            unit (stage0 modules), golden (oracle diffs, --emit-ir), ffi/ (C<->IMP both ways), bootstrap (fixed point)
docs/             see Documentation
configure, Makefile   POSIX sh configure + portable make (GNU and BSD make); builds pcc objects from the submodule
```

## Phases

**0. Repository and provenance**
- `git init` and add the pcc submodule.
- Copy the tape from the hbnf corpus and check it against `SHA256SUMS`.
- Fetch and hash the three binaries.
- Write `PROVENANCE.md`, reusing the hbnf README's attribution.
- Build skeleton.
- CI matrix: Linux amd64/aarch64, FreeBSD, NetBSD, OpenBSD (VM actions), macOS amd64 (pcc) and macOS arm64 (C fallback).
- First commit, with the Co-Authored-By trailer.

**1. Oracle (SIMH + TOPS-10)**
- open-simh `pdp10-ka`/`pdp10-ki` with TOPS-10 6.03, or `pdp10` (KS10) with TOPS-10 7.04 if the KA/KI images are unavailable. Scripts pack `decus/` onto a disk or BACKUP tape (`back10`-style tool) and drive the session with SIMH `expect`/`send`.
- Captures, per module:
  - `/L` source listings and `/A` assembly listings;
  - the compiler's own debug dumps via switches in `impm.imc:62-65`: `GPRINT` (syntax graph), `TPRINT` (code tree), `SEMPER` (semantics array) and `FTRACE`/`FMAP`.
- Golden captures go in `tests/golden/oracle/`.
- PMOD: find its entry points in `imp.shr` (loader symbols, plus the PRINT calls generated in the `/A` listings). Disassemble it, and document its behaviour in `docs/internals/pmod.md` as the spec for `lib/print.c` and `src/pmod.imp`.

**2. Stage 0, part 1: housekeeping (`free`, `dir`, `stack`, `lex`)**
- Port the original algorithms faithfully, with free storage as word arrays addressed by index, so the internal tables can be diffed against oracle dumps.
- The directory stores byte strings. A compatibility printer renders names the way the oracle dumps do.
- Each C file opens with a header mapping it to its `.imc` file. Each function cites the original routine.

**3. Stage 0, part 2: parsing (`graph`, `parse`, `ercor`)**
- The Irons parser, connectivity matrices (`GMATR`), error correction and disambiguation.
- Tests: graphs built from RSYN's built-in notation match the oracle's `GPRINT`.

**4. Stage 0, part 3: semantics and `RSYN` (`dosem`, `entree`, `cotree`, `rsyn`). This is the pivot milestone.**
- `rsyn.c` mirrors `rsyn.imc` (DSEM table: SYNT, SYNTS, SYTRM, SEMR, SEMP, SEMANTICS, SYNTAX, QUOSEM, EQUOSE, CASE/DEFCASE, VALU, REPVAL, SEMBIT).
- Stage 0 reads the "syntax for syntax" and "extended semantics" sections of `syntax.imc`.
- Its graph, code tree and SEMS must match the oracle (`GPRINT`/`TPRINT`/`SEMPER`) exactly. **Done means byte-identical dumps.**

**5. Switch to pcc IR (after RSYN)**
- Code-stack entries (`cotree.imc` header: DI/AR/TY, VAL, CB/CE) keep DI, AR and TY. CB/CE, a chain of PDP-10 code words in free array `CO`, becomes a **statement list (interpass chain) plus a value `NODE*`**: an rvalue for types 2 and 4, an lvalue for types 10 and 20.
- `backend/ir.c` reimplements the machine-dependent semantic primitives as IR builders: `DEWOP`, `ADDOP`, `CONOP`, `FETCH`, `STORE`, `HOOK`, `TAG`, `SUBCALL`, `DEWFUN`, `AREG1`, `REGOF`, `DECLARE`/`DECL`, shifts and rotates, byte fields `<p,s>`, `BYTEP`, `<+P>`, and so on.
- `RMOD`, `AMOD` and `OMOD` go away: pcc does register allocation, peephole work and object emission (`.s`, then the system assembler). `REGISTER` declarations become TEMPs.
- `src/syntax.imp` is a fork of `syntax.imc`. The language surface is unchanged. Semantic arguments that are PDP-10 opcodes (`DEWOP(254B,…)`, `CASE(242B,514B,…) OF SHIFTS`) become symbolic IR operators. The diff against the original is kept reviewable and explained in `docs/porting/syntax.md`.
- Tests: pcc `-Zx`-style dumps and `--emit-ir` goldens for small IMP programs, and execution tests that compare against the oracle's run output for the same programs.
- **FFI and ABI:**
  - IMP `SUBR`s are C-ABI functions taking and returning `int64_t`, with `REAL` mapping to double. On amd64 the varargs AL count is set.
  - External names: case-folded by default, plus a new syntax statement for an exact foreign name and the foreign signature (`REAL` args and result, varargs).
  - Subroutine addresses work as C function pointers, so callbacks work.
  - Pointers are byte addresses. Field access uses byte offsets plus a sized-access syntax extension (8/16/32/64-bit loads and stores), so C structs are reachable.
  - Data goes to `.data`/`.bss`/`.rodata`, and `COMMON` becomes a common or global symbol, so C code can link against it.

**6. Runtime library (`lib/`)**
- `print.c`: PRINT/PMOD semantics from the manual and the disassembly.
- Ports of `tbin`/`tprin`/`traed`.
- POSIX replacements for the TOPS-10 services used (`CALLI`, `OUTSTR`, `OUTCHR`, `INIT`/`LOOKUP`/`ENTER`/`RENAME`, `DATE`/`TIME`).
- `fortio.mac` is not ported. C interop takes its place, and the doc says so.

**7. Self-hosting**
- Port `src/*.imp` to the 64-bit model, one module at a time, with a porting note for each:
  - name packing (`DNAME`, `ADDCHAR`, `LS 28` / `RS 7` tricks) becomes byte strings;
  - `LOC(X)+n` arithmetic scales by the word size;
  - `CALLI`/`DATA(op)`/`EXECUTE` become runtime calls;
  - `TWOSEG`/`REMOTE` become sections.
- `code*`/`amod`/`rmod`/`omod` are replaced by IMP declarations of the `backend/` C API. The compiler calling its own back end through FFI is the first serious FFI test.
- Build sequence (per CLAUDE.md, each stage in its own directory, no overwriting):
  1. `stage0` compiles `src` into **stage1**.
  2. stage1 compiles `src` into **stage2**.
  3. stage2 compiles `src` into **stage3**.
  4. Require stage2 ≡ stage3 (the `.s` output and the binaries).
  5. Only then install or swap.
- Stage 0 stays in the tree as the trusted seed.

**8. Platforms**
- Enable pcc's amd64 Mach-O through the submodule's configure. Fix pcc bugs on the fork, in small upstreamable commits.
- The C fallback covers macOS arm64 until the pcc work below lands; stage1 to stage3 also pass with it.
- The pcc aarch64 and darwin/arm64 work is a parallel track on the fork; see the next section. Record it in `docs/porting/pcc-darwin-arm64.md`.

## pcc on darwin/arm64: what is needed (audit of `~/claude/pcc` @ 9db636ff)

**Main finding.** `arch/aarch64` (2020, Odidev) is largely the 32-bit ARM port renamed:
- Comments still say "8 bits + an even number of rotates" for immediates.
- `ctype()` maps LONG to LONGLONG.
- `param_64bit` splits values "half in and half out of the registers".
- Alignment is 32 everywhere (`ALDOUBLE`/`ALLONG`/`ALPOINT`/`ALSTACK`/`ALSTRUCT` = 32), and `SZLDOUBLE` = 64.
- `va_*` are the ARM32 "walk the stack" builtins.
- Global stores hard-code a hidden scratch register: `table.c:983` emits `adrp x0, AL / add x0,x0,:lo12:AL / str AR,[x0]` and clobbers x0 without telling the allocator.
- x18 is not reserved, and register classes split x0–x15 / x16–x25 / FP oddly (`macdefs.h:140-160`).

**So the work comes in two layers.** Layer A, making it a correct AAPCS64 backend, benefits Linux and the BSDs on aarch64 too, which this project needs anyway. Layer B adds the Darwin deltas on top.

### Layer A: correct the aarch64 backend (all OSes)

1. **Types and alignment:** ALLONG/ALPOINT/ALDOUBLE/ALLONGLONG = 64, ALSTACK = 128 (16-byte SP). Remove the LONG to LONGLONG `ctype` mapping. `SZBOOL` = 8. Linux long double = 128-bit IEEE quad via softfloat `__addtf3`-style calls, or keep 64 behind a flag.
2. **Registers:** a real class layout: x0–x17 temps (x16/x17 IP0/IP1 kept out of allocation), x19–x28 callee-saved, x29 FP, x30 LR, **x18 never allocated**, d0–d31 FP class with d8–d15 callee-saved. Rewrite RSTATUS, ROVERLAP, GCLASS, COLORMAP and the `rnames`/`wnames` tables.
3. **Scratch registers:** remove hard-coded `x0`/`x16` uses in `table.c`. Use `NEEDS(NAREG)` scratch, or the IP0/IP1 convention, for address formation.
4. **Constants:** `movz`/`movk` sequences for 64-bit immediates, and the logical-immediate encoder for and/orr/eor.
5. **Calls and parameters (AAPCS64):** x0–x7 and v0–v7. The stack area for args beyond those is 8-byte slotted on Linux. Composites over 16 bytes are passed by reference. HFA/HVA go in v-registers. Struct returns use x8. Rewrite `funcode`/`argput`/`bfcode`/`param_*`.
6. **Varargs (Linux and BSD):** the AAPCS64 `va_list` struct (`__stack`, `__gr_top`, `__vr_top`, `__gr_offs`, `__vr_offs`). The prologue saves x0–x7/q0–q7 into the register save area. Rewrite `va_start`/`va_arg`.
7. **Prologue/epilogue:** `stp x29,x30,[sp,#-N]!`, a valid frame record (Darwin requires one), callee-saved spills, and 16-byte alignment.
8. **Validation:** pcc's own test suite plus a C ABI conformance harness, where pcc-compiled callers and callees are linked against clang/gcc ones for every argument class. Then pcc compiles itself on aarch64 Linux.

### Layer B: Darwin deltas (Apple arm64 ABI and Mach-O)

1. **configure.ac:45-60:**
   - add `aarch64|arm64) targmach=aarch64 ;;` to the darwin case;
   - fix the `targosver` parsing, which only knows darwin 7–10 while modern macOS is darwin 20+;
   - set `stabs=no`, so pcc emits DWARF or no debug info, as modern ld/dsymutil expect.
2. **Symbol naming:** `exname()` (`arch/aarch64/local.c:439`, and the `#define exname(x) x` in `local2.c:30`) must prefix `_` under MACHOABI, as amd64 does. Local labels use `L%d` instead of `.L%d` (`LABFMT`, `macdefs.h:86`).
3. **Assembler syntax:**
   - `sym@PAGE` / `sym@PAGEOFF` instead of `sym` / `:lo12:sym`.
   - `sym@GOTPAGE` / `sym@GOTPAGEOFF` loads for extern data, which can live in dylibs.
   - No `.type`/`.size`. `.globl`, `.p2align` and `.private_extern` as needed.
   - Mach-O sections in `setseg` (`code.c:52`): `.section __TEXT,__text`, `__DATA,__data`, `__TEXT,__const`, `__TEXT,__cstring`. Use `.zerofill` for BSS. Replace `.comm` (log2 alignment on Mach-O) and `.local`. `__DATA,__mod_init_func` replaces `.ctors`/`.dtors`. TLS uses `@TLVPAGE` descriptors, which can be deferred.
   - PIE is mandatory: no absolute relocations in `__text`. Absolute pointers in `__DATA` are fine (rebased).
4. **Apple arm64 ABI ("DarwinPCS") differences from AAPCS64:**
   - **All variadic arguments go on the stack**, in 8-byte slots. `va_list` is a plain `char *`. `va_start`/`va_arg` become a simple pointer walk, close to what the current ARM32-derived builtins do, but caller-side lowering must put the `...` args on the stack. The callee needs to know the named-argument count; it comes from the prototype.
   - Non-variadic stack arguments are packed at natural size and alignment, not 8-byte slots.
   - **The caller sign- or zero-extends** char, short and bool arguments to 32 bits.
   - `long double` = `double` (64-bit), and `char` is signed (on Linux aarch64 it's unsigned).
   - x18 is reserved (Layer A item 2), and the frame record is mandatory (A7).
5. **Driver (`cc/cc/cc.c` darwin paths, `os/darwin/ccconfig.h:45-160`):**
   - find the SDK with `xcrun --show-sdk-path`, not the hard-coded `/Applications/Xcode.app/…` path (Command Line Tools installs lack it);
   - pass `ld -arch arm64 -platform_version macos <min> <sdk> -syslibroot <sdk> -lSystem`. `-macosx_version_min 10.9.0` is rejected for arm64, whose minimum is 11.0;
   - `-e _main` (or the default LC_MAIN); no `crt1.o`/`dylib1.o` on arm64 (`CRTBEGIN_S "dylib1.o"` is wrong there);
   - assemble with `as -arch arm64` (the clang integrated assembler), which accepts the syntax above;
   - ld64 ad-hoc **code-signs** arm64 output automatically. Verify, and fall back to `codesign -s -` in the driver if needed.
6. **SDK headers:** compiling C *with* pcc on macOS means parsing Apple SDK headers, which use clang-isms: `__has_feature`/`__has_include`/`__has_attribute`, `_Nullable`/`_Nonnull`, `__attribute__((availability(...)))`, blocks `^` in prototypes, `__builtin_*`, `_Float16`. Needed:
   - cpp predefines/answers for the `__has_*` family;
   - parse-and-ignore for nullability and availability;
   - a `-D__BLOCKS__=0`-style escape, or pcc wrapper headers in `os/darwin/include`.

   This is needed to build **stage 0 with pcc** on macOS. The IMP back end itself doesn't need SDK headers.
7. **Validation:** a hello-world link, the ABI harness against clang on an Apple Silicon machine (CI `macos-14` runners are arm64), pcc self-compile on macOS arm64, and then IMP72 stage1 to stage3 using pcc instead of the C fallback.

**Order:** A1–A4, A7, A5, A6 on Linux aarch64 first, which is easier to debug in CI. Then B1–B3 and B5, which give the IMP back end its first darwin/arm64 binaries, since IMP-IR needs only word and double arguments. Then B4 (varargs and calls to libc) and B6 (SDK headers, for stage 0). Estimated size: Layer A is the bulk, a substantial rewrite of `code.c`/`local.c`/`table.c`/`macdefs.h`; Layer B is a few hundred lines. Every change goes on a branch of the `moebiusv/pcc` fork, in upstreamable commits.

## Documentation (`docs/`)

- `manual/`: `imp.rno` rendered to Markdown/HTML by `tools/runoff2md` (handles RUNOFF `^` and `\` case flags and `.hl`/`.skip`/`.indent`), with the original kept beside it.
- `LANGUAGE-DIFFERENCES.md`: what changes between IMP72 on the PDP-10 and IMP72 on 64-bit hosts (word model, strings, byte pointers, I/O, FFI syntax).
- `ARCHITECTURE.md`: the module map (following manual §4), the bootstrap chain, and IMP-IR and pcc.
- `internals/<module>.md`, one per kernel module.
- `BOOTSTRAP.md`: rsyn, then syntax, then the pcc switch, then stages 1 to 3.
- `FFI.md`: the C ABI, declarations, callbacks, linking with ELF/Mach-O objects, examples.
- `BUILDING.md`: every OS, plus pcc submodule notes.
- `ORACLE.md`.
- `porting/`: per-module diffs from the original.
- `imp72(1)` man page.
- Every C source file starts with a comment block saying which original routine it implements.

## Verification

- `make check` runs:
  - stage-0 unit tests;
  - oracle goldens (byte-identical GPRINT/TPRINT/SEMPER after RSYN plus the syntax header);
  - `--emit-ir` goldens;
  - execution tests diffed against the oracle's run output.
- FFI suite:
  - IMP calls libc (`printf` varargs, `qsort` with an IMP callback, `strlen`);
  - C calls IMP `SUBR`s, and C links against IMP `COMMON`;
  - checked with both pcc and the C fallback.
- Bootstrap: stage2 ≡ stage3 on Linux amd64, Linux aarch64, a BSD, macOS amd64 (pcc) and macOS arm64 (C fallback), in CI.
- Docs: every manual example compiles and runs; `tools/runoff2md` output is checked into the repo and diffed in CI.

## Risks and open items

- PMOD exists only as a binary. Mitigated by the oracle disassembly plus the manual's PRINT chapter.
- RSYN2 has no file. Confirm from the oracle's symbol table which file holds `SETSEM`, `SEMFX1` and `RESETSEM` (`dosem.imc`).
- Getting a 1973 KA/KI binary running under TOPS-10 6.03 in SIMH may need monitor or image hunting. Fallback: the KS10 with 7.04.
- pcc aarch64 is not yet a correct AAPCS64 backend (see the audit). aarch64 ELF and darwin/arm64 both depend on Layer A. Until it lands, the C fallback covers every arm64 host.
- Running the parser on 64-bit words changes no algorithms, but bit-matrix code (`BIT(M,ROW,COL)` in `impsyn.imc` assumes 36 bits per word) must be ported to 64 deliberately.
