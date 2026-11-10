# IMP72: decisions and status

## About this ledger

- What it is: for each design and implementation choice the author made about imp72 over time, where it stands now against the current docs and code: what is done, what is left to do, what was rejected, and what is left in the air.
- Precedence (the author's rule): more recent decisions have greater precedence; each subject's status follows its latest standing line.
- Each subject: `Status`, `Decided` (the latest standing line with its source `[R<n>]`, a row of the decision register), `Evidence` (file, line, quote or commit) or `not found: searched ...`, `Doc differs` where a doc or code says something else, `Left` for what remains.
- The decision register: each of the author's messages cited here is a numbered row [R<n>] of the append-only [decision register](decision-register.md) (the date, the kind, his words, the subjects, and what a later row replaced); numbers are never reused, a replaced row points forward.
- Checked, not guessed: a status of DONE needs evidence; what could not be checked is IN THE AIR and says why.
- `Rationale` is why he chose it, in his own words; `Precedent` is a system, language, standard or design named as a referent, with whether his choice follows it (chosen), he turned it down (rejected) or it was only compared (considered), and who named it (the author, or the assistant and he accepted it or did not take it up). Only what the messages say: nothing is added from outside. The precedents are indexed by name near the end.
- The chains behind it are in the decision log of the same project; the evidence is as of the repositories read on 2026-11-10.
- Checked against the repository at commit a7bff02 (2026-11-07), which is newer than the author's checkout on his other machine (clean at eab123c, 2026-11-05, nothing uncommitted), and the packaging repositories.

## Contents

- [Summary](#summary)
- [Left to do (10)](#left-to-do-10)
- [Left in the air (2)](#left-in-the-air-2)
- [Rejected or replaced (1)](#rejected-or-replaced-1)
- [Done (43)](#done-43)
- [Precedents, by name](#precedents-by-name)
- [Where the current docs or code differ from the decisions (20)](#where-the-current-docs-or-code-differ-from-the-decisions-20)

# Summary

- Left to do: 10 of 56 subjects
- Left in the air: 2 of 56 subjects
- Rejected or replaced: 1 of 56 subjects
- Done: 43 of 56 subjects

# Left to do (10)

- Decided, and not yet in the current code or docs (or only partly).

## Other

### CI test runs
- Status: TO DO — the author chose option 1 (pause the agent while debugging), a process step, details pending: no repository trace (latest decision 2026-11-04 \[R166\])
- Decided: 2026-11-04 approved-proposal: "1": option 1, pausing the agent for up to 30 minutes while debugging in dev-debian-sid-i386, then unpausing \[R166\]
- Rationale: accepted from the assistant: pause the other agent's container 20-30 minutes to debug in dev-debian-sid-i386, since it holds the single docker/qemu slot \[R166\]
- Evidence: not checked: no repository trace of the pause; the CI files exist (.github/workflows/)
- Left: details pending: nothing to verify in the code or docs; whether the pause was done is not recorded

### CPU pinning
- Status: TO DO — the author's working rule for the agent, details pending: not a repository artifact, so no document states it (latest decision 2026-11-04 \[R153\])
- Decided: 2026-11-04 decided: "stay away from core 5 and 11, pin on everything but them" \[R153\]
- Rationale and precedent: none stated in the sources
- Evidence: not checked: searched tools/\*.sh and docs/BUILDING.md for taskset/pinning, found none
- Left: details pending: nothing in the repository states the rule; whether it belongs in a doc is not decided

### Optimization default
- Status: TO DO — the optimizer runs at O1 and the target machine at the default level, not all optimizations (latest decision 2026-10-23 \[R64\])
- Decided: 2026-10-23 decided: "default to all optimizations enabled" \[R64\]
- Rationale and precedent: none stated in the sources
- Evidence: stage0/llvmorc.c:129 "default\<O1\>"; src/llvm.imp:130 "LLVMRunPasses(M,LOC('default\<O1\>')"; LLVMCodeGenLevelDefault at llvmorc.c:124
- Doc differs: docs/ARCHITECTURE.md:52 says "optimizes it at \`default\<O1\>\`"
- Left: set the pass pipeline and code generation level to the highest in both stage0/llvmorc.c and src/llvm.imp; searched docs/ROADMAP.md, docs/imp72.1, docs/BUILDING.md for an -O option and found none

### Quirks
- Status: TO DO — details pending: the chain does not say which quirks; no way to map "those quirks" to a commit (latest decision 2026-11-03 \[R145\])
- Decided: 2026-11-03 decided: "fix those quirks you found." \[R145\]
- Rationale and precedent: none stated in the sources
- Evidence: not checked: the quirks are not named in the chain; commits of 2026-11-03 fix code generator bugs (412a39f, 34b481b) but cannot be tied to the decision
- Left: details pending: which quirks were meant, and whether each is fixed

### README and documentation
- Status: TO DO — the author asked for the NSA mention in the profile README, a repository outside this clone; imp72's own README has no such text, details pending (latest decision 2026-11-05 \[R179\])
- Decided: 2026-11-05 decided: Also "mention imp72 was developed for and used by the nsa". \[R179\]
- Rationale: "people love stories like that. the roadmap from pdp10 to amd64 is a great one, just do it as simple narrative, not as slop." \[R70\]
- Rationale: "update the readme make it comprehensive with our design decisions" \[R22\]
- Rationale: tagline belongs to "defensive coding" section, "which would include ada, the crypto stuff, and imp72 in one category"; also "imp72 was developed for and used by the nsa" \[R179\]
- Precedent: pcc (chosen; named by author): backend after the PDP-10 code, IR or AST his choice of assistant \[R14\]
- Precedent: DECUS documentation and sources for imp72 (chosen; named by author): basis to bootstrap onto modern Linux, BSD and macOS \[R14\]
- Precedent: PDP-10 (considered; named by author): ok to mention the port from the old code in the README \[R70\]
- Evidence: not checked: the NSA/defensive-coding tagline belongs to the author's profile README (conversation 8f210f97), not given; searched README.md and docs/ for "nsa", "military", "defensive": none. Earlier lines: commit cda1b68 docs agree with the code, history words removed
- Left: details pending: whether the profile README carries the tagline and the NSA mention

### Reference comparison with the original
- Status: TO DO — details pending: the repository retired the listing comparison; no record of the 15 differences being classified (latest decision 2026-10-29 \[R118\])
- Decided: 2026-10-29 decided: "finish classifying the 15 listing differences". \[R118\]
- Rationale: "comparisons should take care of things that the compiler varies, like timestamps and retguards to comparisons are apples to apples" \[R51\]
- Rationale: "did you check against the oracle? maybe rewriting names is proper behavior" \[R68\]
- Rationale: accepted from the assistant: port debug.imc first because its graph, tree and semantics dumps are what is compared with the oracle \[R20\]
- Precedent: the original compiler (oracle) (chosen; named by author): reference for judging behaviour such as name rewriting \[R68\]
- Evidence: docs/ORACLE.md:134-140 "these captures are not compared with their output"; commit 8a2db1a "Retired on purpose: tests/oracle/dumps.sh"; tests/golden/oracle-lst and tools/lst-compare.imp remain; program output is compared (tests/oracle/run.sh)
- Left: details pending: whether the 15 listing differences were classified anywhere; the decision (2026-10-29) predates the retirement of the comparison

## 02 Language and compiler design

### Assignment token and the \_ to := conversion
- Status: TO DO — \`:=\` is done; the author said the converter is no longer needed while the repository still carries and uses it for the oracle, details pending (latest decision 2026-11-03 \[R137\])
- Decided: 2026-11-03 decided: "no more need for the := converter." \[R137\]
- Rationale: "it would be without \_ for backward compatibility, \_ would join the chars allowed in names, making imp elf compatible" \[R97\]
- Rationale: "imp has all the facilities for text transformations; why not do the \_ conversion for the oracle in imp itself?" \[R124\]
- Rationale: "no more need for the := converter." \[R137\]
- Precedent: original IMP72 \`\_\` assignment (considered; named by author): kept backward compatible, \`\_\` becomes a name letter \[R97\]
- Precedent: modern ELF symbol names (chosen; named by author): \`\_\` allowed in names for ELF compatibility \[R97\]
- Evidence: aa2fb2e "assignment is := and not \_, and \_ is a letter of names"; docs/LANGUAGE-DIFFERENCES.md:8 "Assignment is \`:=\`, and \`\_\` is a letter"; 620acd1 removed the converter, then 5926e54 brought it back as tools/imp-assign.imp (IMP, not Python) because tools/oracle-run.imp sends programs to TOPS-10
- Doc differs: docs/ORACLE.md:53 says "\`tools/imp-assign.imp -T\` converts \`:=\`"
- Left: details pending: the converter is kept for the oracle (the original compiler needs \`\_\`); whether "no more need" meant for the tape/ audit only or for the oracle too is not settled by the records

## 03 Bootstrap and port to Linux

### TOPS-10 ABI and CALLI
- Status: TO DO — CALLI is partly kept (impcalli) and CORE is done another way; tops10-abi.txt is not in the repository (latest decision 2026-10-24 \[R74\])
- Decided: 2026-10-24 decided: "ccall can replace calli where it makes sense", but if calli(11b) maps to core then define core to call realloc; asks why 11b was chosen. \[R74\]
- Rationale: "ccall can replace calli where it makes sense"; if calli(11b) maps to core "we can define core to call realloc and it is fine". \[R74\]
- Rationale: Interface to the ABI "as similar as is reasonable"; "just needs a little translation, not new grammar or words". \[R73\]
- Precedent: TOPS-10 (considered; named by author): keep code interfacing with it similar in our compiler \[R73\]
- Precedent: libc realloc (chosen; named by author): CORE defined to call realloc \[R74\]
- Evidence: lib/imp72rt.imp:34-35 impcalli handles codes 0, 012, 014, 023, 024, 027 and 030 only (the comment says "the others do nothing"; no 11B); src/free.imp CORE makes the first K+1 words of a reserved arena usable (IMPCORESET, mmap); src/free.imp:133 uses CCALL realloc only for the relocation table; tops10-abi.txt (and pre-1.0-studies.md) was deleted on purpose in 34b481b of 2026-11-03 "their lasting content moved into the guides"; no commit since 2026-11-05 touches CORE/CALLI (clone HEAD a7bff02: README links only)
- Doc differs: docs/porting/tape-names.md says CORE "makes the first \`K+1\` words of the reserved arena usable", not a realloc
- Left: the file is gone by design, so only the answer whether CALLI(11B) is dropped; CORE/RLEAS are tied to the channel I/O of ROADMAP item 5

### prebsd repository
- Status: TO DO — details pending: prebsd is a separate repository, not in this clone; the OpenBSD VM tool is here (latest decision 2026-11-04 \[R169\])
- Decided: 2026-11-04 decided: "the prep agent should set up openbsd first, then the agent should proceed as soon as openbsd is ready to be installed" \[R169\]
- Rationale: "the prep agent should set up openbsd first, then the agent should proceed as soon as openbsd is ready to be installed" \[R169\]
- Rationale: "i want images that have dev env prepared so it can be used for any of my projects" \[R165\]
- Rationale: "prebsd also needs to keep local copies ... the local backups are just in case the online stuff goes offline" \[R39\]
- Precedent: SIMH PDP-10 KA10 (considered; named by author): build simh with proper hardware and ini configs ready \[R39\]
- Evidence: in this repository only tools/bsd-vm/bsd-vm.sh (OpenBSD 8.0, NetBSD 11.0, FreeBSD 15.0 under QEMU/KVM, 29fa20a) and docs/BUILDING.md:392-398 describe the VM side (OpenBSD, NetBSD and FreeBSD); README.md has no mention of TOPS-10 disk images, consistent with "no mention in the readme"
- Left: the prebsd contents (exported VM images, ISO installs, TOPS-10 entries, boot.py retirement) are to be checked in the prebsd repository

## 04 Code generation and targets

### Target platforms and coverage
- Status: TO DO — the last line asks to pull and evaluate arch/os coverage (dated after HEAD of 2026-11-07); coverage is wide but Windows and Cygwin have no runtime library (latest decision 2026-11-09 \[R180\])
- Decided: 2026-11-09 decided: "pull and eval now; especially arch/os coverage" \[R180\]
- Rationale: "we aren't refusing windows without good reason; if llvm and mingw can do the job, we'll use them" \[R172\]
- Rationale: "an unrecognized triple should politely exit rather than output junk" \[R167\]
- Precedent: LLVM and mingw (Windows) (chosen; named by author): use them if they can do the job \[R172\]
- Precedent: clang and gcc linking (considered; named by author): if they link with the system ld, "then we can too" \[R172\]
- Precedent: C fallback for arm64 macOS (chosen; named by assistant, accepted): answered "C fallback for now (Recommended)" \[R17\]
- Precedent: pcc (considered; named by assistant, not taken up): pcc lacks darwin/arm64; Mach-O coverage discussed \[R17\]
- Evidence: README.md:93-110 (Linux amd64/arm64 and the Alpine arches, macOS arm64, FreeBSD, NetBSD, OpenBSD; --target for every LLVM target; Linux glibc/musl ~25 arches, BSDs, macOS, mingw-w64, Cygwin); 130226f stage0/lvtriple.cpp asks LLVM's triple parser (the "c++ shim"); 93d1a7f unrecognized triple refused; 25979df Cygwin; eab123c bare-metal triples
- Doc differs: docs/ROADMAP.md:### 11 "A runtime library for Windows and Cygwin" is not done ("There is no runtime library for Windows or Cygwin"); 130226f "OpenBSD, macOS, older LLVM headers: not run"; Cygwin values "from memory and marked unverified" (25979df)
- Left: the evaluation of the 2026-11-09 request is not in the repository at 2026-11-07; the potentials on the shim superseding earlier fixes and localising the linker are partly met (hand lists of src/llvm.imp gone) but unrecorded as decisions

# Left in the air (2)

- Only ideas, open questions, conflicting decisions, or points the record and the repository do not settle.

## Other

### Channel I/O
- Status: IN THE AIR — no standing decision, only a potential to study before v1.0.0 (latest decision no standing decision)
- Decided: no standing decision (only potential decisions)
- Rationale: "you could use them 'Underneath' to implement the channel stuff that was in use before"; to study before v1.0.0 \[R82\]
- Precedent: original channel I/O (considered; named by author): to be implemented underneath with fopen and putc \[R82\]
- Precedent: fopen/putc (C stdio) (considered; named by author): used underneath for the channel statements \[R82\]
- Precedent: UTF-8 (considered; named by author): other matters for study before v1.0.0 \[R82\]
- Evidence: docs/ROADMAP.md item 5 "The original's channel I/O" proposes statements over fopen/getc/putc, "None is implemented"; docs/porting/pre-1.0-studies.md is gone from HEAD (the item is docs/ROADMAP.md:73)
- Left: decide whether to build the channel statements; the study note was removed

### musl changes
- Status: IN THE AIR — no standing decision, only a question (latest decision no standing decision)
- Decided: no standing decision (only potential decisions)
- Precedent: musl (considered; named by author): asked what happened with the musl changes the imp72 project added \[R37\]
- Evidence: searched README.md, docs/\*.md and git log for a musl change record: only the musl link table and Alpine CI (README.md:96, :104), no record of "the musl changes the project added"
- Left: the question is unanswered in the repository

# Rejected or replaced (1)

- Rejected, reversed, or replaced by a later decision (the later one has precedence).

## 04 Code generation and targets

### Machine word and 36-bit target
- Status: REJECTED — the 36-bit target is out (rejected 2026-11-03 \[R136\]); the repository carries no 36-bit target
- Decided: not in force (rejected 2026-11-03)
- Rationale: "forget about a 36 bit target that isn't even a consideration" \[R136\]
- Rationale: "for now we just focus on modern platforms, the 36bit version will always be a separate thing" \[R46\]
- Rationale: accepted from the assistant: 64-bit word, byte addresses, so native code with FFI to C ABIs works \[R16\]
- Precedent: 64-bit word, byte addresses (C ABI) (chosen; named by assistant, accepted): answered "64-bit word, byte addrs (Recommended)" \[R16\]
- Precedent: IMP72 36-bit word-addressed memory (rejected; named by author): 36-bit target dropped \[R136\]
- Precedent: SIMH + TOPS-10 (chosen; named by assistant, accepted): answered "Yes, SIMH + TOPS-10" for an oracle \[R16\]
- Precedent: pcc IR (considered; named by author): "pcc ir can handle it" about 36 bits \[R40\]
- Evidence: 8a2db1a removed the seed's 36-bit emulation (trans.c, sx36/u36/L18/R18, IMP72NARROW); README.md:127 "The IMP word is 64 bits"; the 36-bit forms remain only in decus/ and the oracle (docs/ORACLE.md), stage tests retired on purpose

# Done (43)

- Implemented in the current code or stated in the current docs, with the evidence.

## Other

### Architecture test matrix
- Status: DONE — alpine s390x is in the CI matrix (latest decision 2026-11-04 \[R170\])
- Decided: 2026-11-04 decided: "might as well add alpine-s390x too" \[R170\]
- Rationale: "for speed maybe do the s390x on debian instead of alpine; only if the alpine s390x fails would we do a local run" \[R163\]
- Rationale: "for 32le let's do i386 that should run very fast on our native amd64" \[R161\]
- Precedent: Debian amd64, i386, powerpc, s390x (chosen; named by author): local matrix, with FreeBSD amd64 and open/net bsd amd64 i386 \[R164\]
- Precedent: Alpine s390x (chosen; named by author): "might as well add alpine-s390x too" \[R170\]
- Precedent: i386 for 32-bit little-endian (chosen; named by author): fast on native amd64 \[R161\]
- Precedent: NetBSD/macppc in qemu-system-ppc (considered; named by assistant, not taken up): 32-bit big-endian candidate, slow full machine \[R161\]
- Evidence: .github/workflows/ci-alpine.yml arches 386, amd64, arm/v7, arm64, loong64, ppc64le, riscv64, s390x; tests/crossrun (s390x, powerpc, i386 under qemu user mode)
- Doc differs: README.md:96 lists the CI hosts; Debian powerpc/i386 and the OpenBSD/NetBSD i386 CI runs of the earlier lines are not in the workflows (ci-bsd.yml uses the amd64 VM actions)

### Bounded strings and temporary files
- Status: DONE — strlcpy/strlcat brought in; temp files made safely or avoided (latest decision 2026-11-04 \[R150\])
- Decided: 2026-11-04 decided: "make sure any and all tmpfile usage follows the best djb practices, or even better, if possible" \[R150\]
- Rationale: "since we are porting to openbsd, bring in strlcpy and friends" \[R149\]
- Rationale: "follows the best djb practices, or even better, if possible" \[R150\]
- Precedent: OpenBSD strlcpy and strlcat (chosen; named by author): bring in strlcpy and friends \[R149\]
- Precedent: djb tmpfile practices (chosen; named by author): best djb practices or better \[R150\]
- Evidence: stage0/strlcpy.c, stage0/strlcat.c, stage0/strl.c; commit ebce211 (memfd, mkdtemp 0700, O\_EXCL, unlink/rmdir, signal cleanup); docs/BUILDING.md:77-106 "Temporary files"; tests/tmpfiles; README.md:187-188

### CI configuration
- Status: DONE — all workflows can be run, with tags and manual run (latest decision 2026-10-29 \[R116\])
- Decided: 2026-10-29 changed: "push it and run every workflow", lifting the don't-rerun rule. \[R116\]
- Rationale: "if loong64 has no emulator, don't ci it. shouldn't you set up all builds in step so the ci can restart where they leave off and every crash leaves a trail?" \[R111\]
- Rationale: "shouldn't you record what ci cpu reports +d etc for all the ci runs on all arches/oses?" \[R112\]
- Rationale: accepted from the assistant: one untagged push so every workflow runs together on the same commit and confirms a fully green state ("push it and run every workflow") \[R116\]
- Rationale: accepted from the assistant: split workflow files so each can be restarted and cancelled on its own; path filters save QEMU time ("do 2, 3 4 and 5") \[R107\]
- Evidence: .github/workflows/ci-linux.yml, ci-alpine.yml, ci-bsd.yml, ci-macos.yml; docs/BUILDING.md:349-356 (\[ci:...\] tags, "Run workflow"); commit 05d7824 starting CI runs; loong64 job kept in ci-alpine.yml

### COBOL preprocessing in hbnf
- Status: DONE — xml and json grammars exist and COBOL went ahead as decided; the COPY/REPLACE preprocessing the subject names is not built (latest decision 2026-11-03 \[R131\])
- Decided: 2026-11-03 changed: "no need to wait on xml and json." \[R131\]
- Rationale: asks whether cpp and copy/replace can apply "as we go" so stacks "map errors back to the original text"; "no need to wait on xml and json." \[R131\]
- Precedent: imp72 planned error mapping to original text (considered; named by author): "kind of like what we have planned in imp72" \[R131\]
- Evidence: hbnf clone (HEAD 5224390): corpus/xml and corpus/json exist; corpus/cobol/README.md:34 "Not yet: fixed format ..., \`COPY\` and \`REPLACE\`" (belongs to the C preprocessor item of RFCPLAN.md); commit 58aff2e COBOL grammar against GnuCOBOL's tests
- Left: whether "no need to wait" was about starting COBOL (done) or about the preprocessor layer (not built; details pending)

### Command-line flags and object files
- Status: DONE — -c, -o, -S (assembly) and -Sir (IR) work, objects are .o (latest decision 2026-10-31 \[R123\])
- Decided: 2026-10-31 decided: "-Sir would mean emit llvm IR". \[R123\]
- Rationale: "-S should emit assembler for the target arch, not llvm-ir" \[R122\]
- Rationale: "imp barenames or names in general must all be valid names in C" and ".o files instead of .rel files" \[R121\]
- Rationale: asked whether -o -c -S work "as a unix compiler user would expect" \[R119\]
- Precedent: Unix compiler -o -c -S behaviour (chosen; named by author): flags should match a Unix compiler user's expectation \[R119\]
- Precedent: .rel object files (rejected; named by author): emit .o files instead \[R121\]
- Precedent: C identifier rules (chosen; named by author): names must be valid C names \[R121\]
- Precedent: TOPS-10 /U and /V in -i mode (considered; named by author): asked if they work as a TOPS-10 user expects \[R119\]
- Evidence: README.md quick start "imp72 -c a.imp b.imp  # a.o and b.o", "-S ... assembly, hello.s", "-Sir ... LLVM IR text, hello.ll"; docs/imp72.1 synopsis "Fl c \| S \| Sir"
- Doc differs: docs/ROADMAP.md item 10 says the seed's -S still writes IR and it has no -Sir (parity not done, the seed is not imp72)

### Debian, OpenBSD and Alpine packages
- Status: DONE — an OpenBSD port exists (latest decision 2026-11-03 \[R147\])
- Decided: 2026-11-03 decided: "push; start obsd port." \[R147\]
- Rationale: "make it totally compliant and no evidence of ai slop" \[R144\]
- Rationale: "I'd like to submit an openbsd port of imp72" \[R146\]
- Precedent: Debian packaging (chosen; named by author): fully compliant Debian package \[R144\]
- Precedent: OpenBSD ports (chosen; named by author): asked if imp72 meets them \[R146\]
- Precedent: Alpine aports (chosen; named by author): "do an aport too" \[R44\]
- Evidence: packaging/openbsd/lang/imp72/{Makefile,pkg/DESCR,pkg/PLIST}; packaging/README.md "## OpenBSD: \`packaging/openbsd/lang/imp72\`"; debian/, packaging/alpine/APKBUILD; commit dbabe28 (lintian pedantic)
- Doc differs: packaging/README.md says the port has no distinfo until the tag exists (not yet submitted)

### Decision log
- Status: DONE — docs/decisions-and-status.md is in the repository and linked from the README (latest decision 2026-11-10 \[R182\])
- Decided: 2026-11-10 decided: "afterward do the exact same thing for imp72"; its suggestions and decisions "need to be turned into a proper plan" \[R182\]
- Rationale and precedent: none stated in the sources
- Evidence: commit 4874255 (2026-11-09) "docs: decisions-and-status.md, each design and implementation choice ..."; README.md:165 links docs/decisions-and-status.md; the decision log itself (the chains) lives in the pdp64 decision-log project, outside this repository

### Disk guard, VM limits and housekeeping
- Status: DONE — tools/guard.sh holds the disk guard; the latest line is housekeeping of agent worktrees, outside the repository (latest decision 2026-11-04 \[R159\])
- Decided: 2026-11-04 approved-proposal: "yes remove finished worktrees" \[R159\]
- Rationale: "something blew up the disk, make sure it doesn't happen again isn't there some tripwire so if a dir has more than 10 gigs it will choke just the program, not everything else?" \[R125\]
- Evidence: not checked: worktrees are not repository state; related earlier lines hold: tools/guard.sh (10 GB default, docs/BUILDING.md:380-383 "Runaway jobs")
- Left: nothing to verify in the code or docs

### Linking
- Status: DONE — lld in the process, else ld.lld, else the system ld, with per-OS/arch tables (latest decision 2026-11-05 \[R173\])
- Decided: 2026-11-05 decided: "thoroughly support native ld/lld where the lld lib isn't available, and make sure each arch/os gets its appropriate flags" \[R173\]
- Rationale: "vendoring objcopy and lld makes most sense because i want all the work kept in-process" \[R108\]
- Rationale: "why would it invoke any out of process stuff doesn't libllvmorc include linking ability?" \[R100\]
- Rationale: "if they both link using whatever ld is on the box, then we can too" \[R172\]
- Rationale: "thoroughly support native ld/lld where the lld lib isn't available, and make sure each arch/os gets its appropriate flags" \[R173\]
- Precedent: lld in-process (chosen; named by author): vendoring objcopy and lld to stay in-process \[R108\]
- Precedent: native ld / ld.lld (chosen; named by author): fallback where the lld library is missing \[R173\]
- Precedent: clang and gcc linking (considered; named by author): asked whether they link in-process or via system ld \[R172\]
- Precedent: standalone lld fork (rejected; named by author): "defer that indefinitely"; earlier a future project \[R173\]
- Precedent: classic Unix leading underscore (chosen; named by author): "the only mangling I'm ok with" \[R105\]
- Evidence: docs/BUILDING.md:200-207 "The linker is the first of these that exists: lld in the compiler's own process ... \`ld.lld\` ... the system's own"; src/linktab.imp; commit ada6836 "The link takes its files from the machine and the target"; README.md "It never runs \`cc\`"
- Doc differs: docs/BUILDING.md:225 says the Cygwin row is unverified (values from memory, not run on Cygwin)

### Local VMs and local-before-CI
- Status: DONE — a FreeBSD image is in the local VM tool (latest decision 2026-11-04 \[R158\])
- Decided: 2026-11-04 decided: "do freebsd before openbsd (when you resume, not yet)" \[R158\]
- Rationale: "why aren't you doing it all in docker or qemu before doing the ci?" \[R154\]
- Rationale: "don't start ci for an arch/os until it passes here first" \[R155\]
- Precedent: local qemu OpenBSD and NetBSD images (chosen; named by author): asked why no local netbsd/openbsd qemu \[R132\]
- Precedent: docker or qemu before CI (chosen; named by author): test locally first \[R154\]
- Precedent: FreeBSD before OpenBSD (chosen; named by author): order when resuming \[R158\]
- Evidence: tools/bsd-vm/bsd-vm.sh case freebsd (freebsd-15.0-x86-64.qcow2, port 2224); docs/BUILDING.md:392-398 on tools/bsd-vm (OpenBSD 8.0, NetBSD 11.0 and FreeBSD 15.0); the bsd-vm.sh header (bd7e668) now names FreeBSD 15.0

### No /tmp
- Status: DONE — tmp/ is gitignored and the default in the source tree (latest decision 2026-10-29 \[R104\])
- Decided: 2026-10-29 decided: "both the gitignore and the default" \[R104\]
- Rationale: "how many times do I have to tell you, never use /tmp ? how much work did you lose?" \[R98\]
- Rationale: "if you really want a tmp/ dir, do it as a gitignored dir inside this repo" \[R99\]
- Rationale: "yes make it default to repo local, in fact, make this a settign for all git repos" \[R103\]
- Evidence: .gitignore "tmp/"; docs/BUILDING.md:92-93 "The default is \`tmp/\` of the source tree"; commit 26299ab
- Doc differs: docs/BUILDING.md:93 says "\`/tmp\` for an installed compiler" (commit c6364a5), and tools/mktmpdir.sh falls back to /tmp

### OpenBSD srand
- Status: DONE — the OpenBSD branch uses srand\_deterministic (latest decision 2026-11-04 \[R157\])
- Decided: 2026-11-04 decided: "do the openbsd branch so it uses srand\_deterministic on openbsd" \[R157\]
- Rationale: accepted from the assistant: keep the repeatability check for other platforms and use srand\_deterministic on OpenBSD \[R157\]
- Precedent: OpenBSD srand\_deterministic (chosen; named by assistant, accepted): "do the openbsd branch so it uses srand\_deterministic" \[R157\]
- Precedent: removing the repeatability check (rejected; named by author): "why not fix srand on openbsd?" \[R156\]
- Evidence: tools/nativesrc.sh "srand(/srand\_deterministic(/g"; tests/native/run.sh:15; docs/BUILDING.md:279

### Origin, license and push policy
- Status: DONE — the ISC licence and the origin are in the README and LICENSE; the latest line is a scope instruction for one session (latest decision 2026-10-24 \[R72\])
- Decided: 2026-10-24 decided: "just fix the hang on amd64 and then fix the docs, leave the rest of the work for next week" \[R72\]
- Rationale: "push it once something is actually working and runnable" \[R21\]
- Rationale: "so we can pause for a few days" \[R71\]
- Precedent: ISC licence (2 clause) (chosen; named by author): "obviously 2 clause isc license" \[R24\]
- Evidence: not checked: no commit titled as the amd64 hang fix found; the earlier lines hold: README.md:186 "ISC licence", README.md:3 origin github.com/moebiusV/imp72, LICENSE
- Left: whether the amd64 hang was fixed (git log search for "hang" found no such commit)

### Other projects
- Status: DONE — the prompt history was imported into the db (the h0 to h7 messages of thread a history-only thread are in the log); other tools are outside imp72 (latest decision 2026-11-10 \[R183\])
- Decided: 2026-11-10 decided: "import my prompts anyways" (the assistant tool's history older than 30 days is lost); later "import it into the db". \[R183\]
- Rationale: "we can have a second qemu when the occasional qemu like s390x is running for my ada toolchain work." \[R178\]
- Evidence: docs/decision-log/data/imp72/messages.jsonl holds the "history only" messages of 2026-11-10; the ssh wrapper and pause rule are outside the imp72 repository (not checked)

### PIE and relocatable images
- Status: DONE — an image restores with the arena at any address; the lexer pointers are moved by the base difference (latest decision 2026-11-05 \[R177\])
- Decided: 2026-11-05 approved-proposal: "ok": agent chooses index form or rebase list for each pointer set \[R177\]
- Rationale: "pie means position independent executable so shouldn't all our pointers and function addresses etc be base-relative if compiled as PIE?" \[R175\]
- Rationale: "that is a semantic property you threw away, so get it back!" \[R176\]
- Rationale: accepted from the assistant: make each lexer pointer an FB-relative index or a pointer rebased on load \[R177\]
- Precedent: original IMP72 base-relative addressing (chosen; named by author): asked to get the base-relative property back \[R176\]
- Precedent: PIE (chosen; named by author): asked whether PIE would allow relocatable images \[R174\]
- Evidence: commit afd0fed "An image restores with the arena at any address (format version 6)"; 254e804 census; docs/BUILDING.md and src/image.imp header say so; tests/image

### Removing pcc names
- Status: DONE — pccir renamed to llvm-ir in files, directories, scripts and make targets (latest decision 2026-10-31 \[R130\])
- Decided: 2026-10-31 decided: "do the rename now; fix every source file, every comment, every file name"; "rename pccir to llvm-ir so we leave that door open a little bit". \[R130\]
- Rationale: "in case we want a pcc backend in the future, rename pccir to llvm-ir so we leave that door open a little bit" \[R130\]
- Rationale: "why is there still any reference to pcc ... now that we are exclusively llvm jit orc?" \[R129\]
- Precedent: LLVM ORC JIT (chosen; named by author): exclusively llvm jit orc \[R129\]
- Precedent: pcc back end (considered; named by author): possible future back end, door left open \[R130\]
- Precedent: pcc IR (rejected; named by author): renamed to llvm-ir \[R130\]
- Evidence: commit f0db94a "no pcc in the names: pccir is llvm-ir"; src/llvm-ir/, tools/llvm-ir.sh, tests/golden/llvm-ir exist; git ls-files has no file with pcc in its name
- Doc differs: stage0/ir.h:4, src/ir2ll.imp:9 and docs/internals/code-generator.md:175 still name pcc as the source of the IR tree format (kept on purpose per f0db94a)

### Versioning and releases
- Status: DONE — version is 0.10.0 and unreleased (latest decision 2026-11-04 \[R152\])
- Decided: 2026-11-04 decided: "c types aren't done yet? we'll go straight to v0.10 before cutting a new release then." \[R152\]
- Rationale: "c types aren't done yet? we'll go straight to v0.10 before cutting a new release then." \[R152\]
- Rationale: "following the release versioning standards, bump the version in whichever way is most appropriate" \[R151\]
- Precedent: SemVer (considered; named by assistant, not taken up): assistant: without a stability document 1.0.0 means nothing under SemVer \[R152\]
- Evidence: VERSION "0.10.0"; commit 4d72a9f "version 0.10.0 (not yet released)"; git tag shows only v0.9.0; debian/changelog 0.10.0

### W-grammar and Algol 68 in hbnf
- Status: DONE — hbnf's own notation divides the operators as the decision asked: = rule, := hyperrule, ::= metarule, and the other dialects read them as one (latest decision 2026-10-18 \[R28\])
- Decided: 2026-10-18 decided: "hbnf when not in an explicit historical mode, can redefine whatever it wants"; asks best way to divide "::= := = :" between hyper and meta rules \[R28\]
- Rationale: "best to stick with historical and precedented syntax where possible" \[R25\]
- Rationale: "hbnf when not in an explicit historical mode, can redefine whatever it wants" \[R28\]
- Rationale: "smooth sipping, clear, minimal sntax chnge"; hoped the new example would look no different from hbnf \[R26\]
- Precedent: W-grammar (considered; named by author): "what can we learn from it" for context-sensitive grammars \[R25\]
- Precedent: ::= := = and : (considered; named by author): which marks a hyper-rule and which a meta-rule \[R27\]
- Precedent: Van Wijngaarden one-level form using ':' (considered; named by assistant, not taken up): assistant: ':' strongest precedent, free in HBNF \[R28\]
- Precedent: ALGOL 68 (considered; named by assistant, not taken up): similar visual rule style \[R28\]
- Precedent: ABNF (considered; named by assistant, accepted): he liked the example as minimal syntax change \[R26\]
- Evidence: hbnf clone (HEAD 5224390): ABNF.md:209 "\`=\` defines a rule, \`:=\` a rule with parameters (a hyperrule), \`::=\` a domain (a metarule) ... \`dialect bnf\` (or \`ybnf\`, \`ebnf\`, \`abnf\`) reads \`::=\`, \`:=\`, \`:\` and \`=\` as one operator"; ABNF.md:353 dialect line
- Doc differs: the Algol 68 grammar itself is not started (hbnf TODO.md:101 "ALGOL 68, C++ grammars: not started"), a different subject

### hbnf scope and parser options
- Status: DONE — hbnf lists the fork points on stderr when generating, under --forks (latest decision 2026-10-18 \[R11\])
- Decided: 2026-10-18 decided: "The fork points can be logged to stout when generating the parser". \[R11\]
- Rationale: "I never agreed to "context free" grammars. ever." Wants context that "isn't jarring or surprising to bnf users, and that is pleasant and easy to use" \[R10\]
- Rationale: bit-field syntax chosen "based on principle of least astonishment" \[R6\]
- Precedent: IMP72 (considered; named by author): bit-field syntax revisited; top 5 historical options asked \[R6\]
- Precedent: imp72 parsing C++ (considered; named by author): semantic actions store data giving the parser context \[R10\]
- Precedent: META II (considered; named by author): asked if it gives a standard name for jets \[R2\]
- Precedent: yacc (considered; named by author): possible source of precedent for naming jets \[R2\]
- Precedent: ALGOL 60 (chosen; named by author): corpus/{algol60,imp72,bip39,...} layout \[R9\]
- Evidence: hbnf clone (HEAD 5224390): ABNF.md:112 "Local forks \| \`--forks\` ... The forks are listed on stderr (\`--quiet-forks\` silences them)"; tests/forks/
- Doc differs: ABNF.md:112 says "Off by default" and stderr, where the decision says stdout; only the default and the stream differ

### imp72 compared with meta2, Ada and hbnf
- Status: DONE — the comparison exists in the hbnf repository as METACOMPILERS.md, with %scan/%action analogues in its table (latest decision 2026-10-17 \[R1\])
- Decided: 2026-10-17 decided: After benchmark, "download the imp72 manual and compare it to meta2 snobol4 and to hbnf"; wonders whether %scan/%action have analogues there. \[R1\]
- Rationale: "i have a feeling our %scan/%action directives have some analogqs there"; "hbnf seeks to be a meta compiler, not a "narrow niche" for config files" \[R1\]
- Rationale: "compare imp72 to ada since both are based on algol" \[R5\]
- Rationale: "imp72 built itself from a minimal bnf compiler"; asks what it takes for hbnf to bootstrap itself into a full imp72 from the DECUS tapes \[R3\]
- Rationale: "maybe we should halt the posix/imp72 ports until hbnf is feature complete including bit fields" \[R4\]
- Precedent: META II (considered; named by author): compared with imp72 and hbnf; hbnf may learn from it \[R1\]
- Precedent: SNOBOL4 (considered; named by author): compared with imp72 manual \[R1\]
- Precedent: Ada (considered; named by author): compared with imp72, "both are based on algol" \[R5\]
- Precedent: ALGOL (considered; named by author): common base of imp72 and Ada \[R5\]
- Precedent: hbnf (considered; named by author): as minimal bootstrapping compiler for imp72 \[R3\]
- Precedent: LLVM ORC JIT and liblld (chosen; named by author): via action and emit, "the way imp72 project is doing" \[R106\]
- Evidence: hbnf clone (HEAD 5224390): METACOMPILERS.md:1 "# HyperBNF beside META II, IMP72 and SNOBOL4", :82 table row "Tokens" and :90 "Your hunch about \`%scan\` and \`%action\`: yes" naming \`%scan\`, \`%action\`, \`%where\`, \`%emit\` against META II, IMP72 and SNOBOL4; README.md:101 links it; imp72 clone itself has no mention (the comparison lives in hbnf)

## 01 Goals and priorities

### Project goal, native port and fidelity
- Status: DONE — README states the port: 64-bit words, IEEE 754, LLVM, unix; the sources are the tape translated (latest decision 2026-10-28 \[R94\])
- Decided: 2026-10-28 decided: "we're trying to be as close as possible to the decus tapes, given the changes we've agreed to; 64 bit words, binary64, unix, llvm." \[R94\]
- Rationale: "we're trying to be as close as possible to the decus tapes, given the changes we've agreed to; 64 bit words, binary64, unix, llvm." \[R94\]
- Rationale: "since this is a native port" \[R80\]
- Rationale: Changes listed: "ieee754 float, 64 bit int, utf8 chars, unix instead of tops-10, pcc ir instead of pdp-10 asm." \[R77\]
- Precedent: decus tapes (IMP72 original) (chosen; named by author): stay as close as possible given agreed changes \[R94\]
- Precedent: TOPS-10 (rejected; named by author): replaced by unix \[R77\]
- Precedent: PDP-10 (rejected; named by author): replaced by pcc IR \[R77\]
- Precedent: pcc (chosen; named by author): backend, IR or AST at assistant's choice \[R14\]
- Precedent: LLVM (chosen; named by author): native code route \[R94\]
- Evidence: README.md:5-9 "The compiler is the original's design and source, translated to IMP for 64-bit words and IEEE 754 doubles. It compiles to native code through LLVM"; docs/porting/modules.md:3 "\`src/\` is the tape's source (\`decus/\`, which is kept as it is) translated to IMP for a 64 bit machine"

### Studies deferred to later releases
- Status: DONE — the empty-string question is written up as a study with a proposal, not implemented, as decided (latest decision 2026-10-29 \[R109\])
- Decided: 2026-10-29 decided: "add that to the list of things to study before v1.0 if empty strings should be allowed" \[R109\]
- Rationale: "using utf8 allows us to use nice mathematical operators and symbols in our code, minimally, tastefully"; study only, not implement. \[R78\]
- Rationale: "i'm more partial to strings that carry their length with them. to be studied in depth." \[R91\]
- Rationale: Study of UTF-8 symbols and C strings goes "after v0.9 and before v1.0, not after v1.0". \[R81\]
- Precedent: modern C string and character syntax (considered; named by author): study its impact on the codebase \[R78\]
- Precedent: unix filename limits (considered; named by author): study making filenames match them \[R95\]
- Evidence: docs/ROADMAP.md:45 "### 3. The empty string \`''\`" ("Proposed: \`''\` is the constant 0"), with items 1 (UTF-8 symbols), 2 (C strings), 4 (file names), 6 (counted strings) and, since bd7e668 (2026-11-09), item 12 "The result type of an IMP routine, seen from C" (:187, "a study, before 1.0"); 50d1115 added docs/porting/pre-1.0-studies.md, since folded into ROADMAP (f2e4840)

### imp72 status and scope
- Status: DONE — imp72 is a standalone project with its own compiler, as decided; the c++/algol68 deferral is not visible here (latest decision 2026-10-22 \[R49\])
- Decided: 2026-10-22 decided: "cancel imp72 altogether. imp72 is its own thing and will get its own compiler" \[R49\]
- Rationale and precedent: none stated in the sources
- Evidence: README.md:5-9 "IMP72 on modern Unix machines ... The compiler is the original's design and source"; the repository is its own git history (360 commits at HEAD bd7e668, 2026-11-09) with its own seed, src/ and LLVM back end; not checked: the pdp64 side of the cancellation (outside this clone)

## 02 Language and compiler design

### C interop
- Status: DONE — typed declarations, callbacks, structs and imp72-cimport exist; typed export is left alone (latest decision 2026-11-04 \[R168\])
- Decided: 2026-11-04 decided: "leave typed export alone for now" \[R168\]
- Rationale: "I want c interop to be as safe, seamless easy and "natural" as possible." \[R143\]
- Rationale: "c funcs need proper return types, putting them all as "long" sounded wrong from the beginning." \[R148\]
- Rationale: "we implemented ffi for a reason, use it" \[R102\]
- Precedent: C long (rejected; named by author): all C functions as long "sounded wrong" \[R148\]
- Evidence: 0271559 typed declarations (EXTERNAL INT puts(PTR)), e16af80 callbacks and function pointers, 085b76c STRUCT layouts, bac2716 C interop part 4 imp72-cimport; docs/FFI.md; docs/ROADMAP.md:### 8 "A typed export (\`EXPORT INT F(INT)\` with C's argument passing) is not planned"
- Doc differs: the potentials (generated .h, Zig/Rust, init/run split) are ROADMAP items 8 and 9, "None is implemented"; -lm added in 7ac3ec2 as decided

### CCALL
- Status: DONE — CCALL stays and is always written (latest decision 2026-11-03 \[R140\])
- Decided: 2026-11-03 decided: "ccall stays" \[R140\]
- Rationale: Asked "is there any reason CCALL shouldn't be CALL?" then decided "ccall stays." \[R139\]
- Precedent: plain CALL (rejected; named by author): asked whether CCALL could be CALL; then "ccall stays" \[R139\]
- Evidence: docs/ROADMAP.md:11 "A call to C stays visibly different from a call to IMP, whose arguments are by reference: \`CCALL\` is always written."; docs/FFI.md:11; lib/ffi.imc

### Case folding and valid names
- Status: DONE — names are case sensitive, no folding; the later "case folding" line is conditional and the docs settle it on TOPS-10 grounds (latest decision 2026-10-31 \[R128\])
- Decided: 2026-10-31 changed: "is imp72 in tops-10 case insensitive? if so, then we should have case folding". \[R128\]
- Rationale: "C interop doesn't require case folding." \[R126\]
- Rationale: "don't defer; remove the case folding stuff, who knows what that is interfering with" \[R127\]
- Rationale: "imp barenames or names in general must all be valid names in C" \[R121\]
- Precedent: TOPS-10 IMP72 case rules (considered; named by author): folding only if TOPS-10 names are case insensitive \[R128\]
- Precedent: C names (chosen; named by author): all IMP names valid C names; emit .o not .rel \[R121\]
- Evidence: docs/LANGUAGE-DIFFERENCES.md:15 "Names are case sensitive. \`ab\` and \`AB\` are two variables, as on TOPS-10"; docs/FFI.md:12 "IMP72 names are case sensitive, as the original's are, and nothing is folded"; names are valid C names (\`\_\` a letter, :aa2fb2e)
- Doc differs: docs/manual/imp72.7:216 "Names are case sensitive, as the original's are" (the TOPS-10 premise of the condition is answered "as on TOPS-10", so no folding)

### Index base
- Status: DONE — indexing is from 0 and \`A\[I\]\` scales by 8 (latest decision 2026-10-17 \[R8\])
- Decided: 2026-10-17 decided: "our index are 0 based because C and IMP72 agree, right or left follows the RFC"; commit and push. \[R8\]
- Rationale: "our index are 0 based because C and IMP72 agree" \[R8\]
- Precedent: C (chosen; named by author): 0-based agrees with IMP72 \[R8\]
- Precedent: IMP72 (chosen; named by author): 0-based \[R8\]
- Precedent: ALGOL 60 (considered; named by author): asked whether it used 0-based offsets \[R7\]
- Precedent: RFC packet diagrams (chosen; named by author): "right or left follows the RFC" \[R8\]
- Precedent: Python (considered; named by assistant, not taken up): 0-based like C \[R8\]
- Precedent: Pascal/Ada/ALGOL 68 (considered; named by assistant, not taken up): declared range or default 1 \[R8\]
- Evidence: README.md:50 "\`FOR K TO N\` runs K from 0 through N"; docs/LANGUAGE-DIFFERENCES.md:18 "\`A\[I\]\` scales by 8"; docs/FFI.md:281 "counted from 0"; the "right or left follows the RFC" clause not checked (no RFC reference found)

### Interactive mode, switches and TOPS-10 compatibility
- Status: DONE — \`-i\` restores the tape's commands, /U /V save images, the banner has the date, lines are 136 columns (latest decision 2026-10-31 \[R120\])
- Decided: 2026-10-31 decided: Match original imp on tops-10 where possible; "our workflow should be almost identical"; "a rel file on tops-10 is a .o file on unix". \[R120\]
- Rationale: "this is part of the work of making imp72 match the original imp on tops-10 where possible. our workflow should be almost identical." \[R120\]
- Rationale: "as imp72 compiles itself, it must be saving the compiled code somewhere; that means it can be checkpointed. if you can compile it, you can save it." \[R83\]
- Rationale: "since we are using llvm, we should make this cross compilating stupidly dead easy." \[R120\]
- Precedent: original IMP on TOPS-10 (chosen; named by author): workflow almost identical; REL file equals .o file \[R120\]
- Precedent: Linux /proc checkpointing (rejected; named by author): does not think it is what U and V mean \[R84\]
- Precedent: ELF/Mach-O object files (chosen; named by author): replace REL as the binary format per OS/arch \[R120\]
- Evidence: 8588292 "imp72 -i runs the tape's commands from the installed driver; the banner has the date again"; docs/BUILDING.md:255-256 (tests/interactive/run.sh, tests/image/run.sh "saving the compiler with \`/U\` and starting from it with \`-I\`"); src/impm.imp:8 "136 COLUMNS"; docs/imp72.1:87 "The object is what a REL file is on TOPS-10: ELF, Mach-O or whatever the"
- Doc differs: docs/imp72.1 documents \`NAME\_/U\` with the \`\_\` separator in the -i command line, which docs/LANGUAGE-DIFFERENCES.md confirms stays (the command line is not IMP source)

### Naming of sem, impsem and codegen files
- Status: DONE — the files carry plain names, no -pdp10 suffix (latest decision 2026-10-22 \[R46\])
- Decided: 2026-10-22 decided: sem, impsem, codegen files to be "fully ported to 8bit arches"; "just let them have their proper names, no need to put -pdp10 at the end" \[R46\]
- Rationale: "just let them have their proper names, no need to put -pdp10 at the end"; focus on modern platforms, 36bit version "a separate thing". \[R46\]
- Precedent: PDP-10 (considered; named by author): later task, kept separate from modern platform code \[R46\]
- Evidence: src/llvm-ir/impsem.imp, code.imp, code2.imp, code3.imp, demit.imp; find found no \*-pdp10 file in the tree

### Operator set and precedence
- Status: DONE — no precedence, right to left, as the original (latest decision 2026-10-18 \[R15\])
- Decided: 2026-10-18 decided: "first 2 imp72 precedence rules, but for 3, instead of arbitrary, stick with first match". \[R15\]
- Precedent: IMP72 precedence rules (chosen; named by author): first 2 rules kept \[R15\]
- Precedent: facet language (considered; named by author): asked how hard to match its operator set and precedence \[R12\]
- Evidence: docs/LANGUAGE-NOTES.md:8 "No precedence, right to left. \`3\*N+1\` is \`3\*(N+1)\`"; README.md:49 "\`A+B\*C\` is \`A+(B\*C)\`"; the facet/Ada operator set stays an unasked potential (not found in docs)

### Return type and imp\_word typedef
- Status: DONE — the typedef stays out; no imp\_word exists and docs say int64\_t (latest decision 2026-10-28 \[R92\])
- Decided: 2026-10-28 changed: "leave out; return to your assigned work" (the imp\_word typedef stays out until the study) \[R92\]
- Precedent: C long (considered; named by author): asked to make IMP routines return long instead of int64\_t \[R89\]
- Precedent: imp\_word typedef (rejected; named by author): "leave out; return to your assigned work" \[R92\]
- Evidence: not found: grep imp\_word over the tree; docs/FFI.md:7-8 "A caller declares the result \`int64\_t\`, never \`long\`"; lib/imp72.h has no imp\_word typedef; docs/ROADMAP.md:187 item 12 lists the study the line waits for ("Until the study is made the word stays \`int64\_t\`")

### String representation
- Status: DONE — strings are ASCIZ; counted strings are a separate type, proposed only (latest decision 2026-11-03 \[R142\])
- Decided: 2026-11-03 changed: "'' keeps original meaning", "imp72 used asciz" so stick with that; "strings with lengths attached would be a separate object type and their own set of handler functions" \[R142\]
- Rationale: "imp72 used asciz" so "we stick with that"; "'' keeps original meaning". \[R142\]
- Rationale: "willing to pay the cost for strings that carry their own length and they must ALSO be nul terminated." \[R141\]
- Rationale: "i'm more partial to strings that carry their length with them." \[R91\]
- Precedent: IMP72 ASCIZ strings (chosen; named by author): kept; counted strings a separate object type \[R142\]
- Precedent: NUL-terminated C strings (considered; named by author): compared against length-carrying strings \[R91\]
- Precedent: length-carrying strings (chosen; named by author): also NUL terminated, separate type with own handlers \[R141\]
- Evidence: README.md:129 "Strings are NUL-terminated bytes"; docs/ROADMAP.md:91 ### 6 "String constants are ASCIZ, as in the original ... This stays." with the counted string as "a separate object type beside it, with its own handler functions"; ROADMAP item 3 keeps \`''\` a syntax error until decided
- Doc differs: ROADMAP item 3 proposes \`''\` as the constant 0, which is the "original meaning" per the manual; not yet implemented

### Syntax freeze
- Status: DONE — the language is the original's; the differences are the mandated ones (latest decision 2026-10-27 \[R79\])
- Decided: 2026-10-27 decided: "no syntax changes to imp72" for now; stay "as close to the original semantics and syntax" as the "ieee754/64bit/utf8/pcc-ir/unix/c-abi mandate allows". \[R79\]
- Rationale: "we stay as close to the original semantics and syntax as our ieee754/64bit/utf8/pcc-ir/unix/c-abi mandate allows" \[R79\]
- Precedent: original IMP72 (chosen; named by author): no syntax changes for now \[R79\]
- Evidence: docs/LANGUAGE-DIFFERENCES.md:3 "The language is the original's"; docs/ROADMAP.md items 1-3 (UTF-8 symbols, C strings, \`''\`) are proposals only, "None is implemented"
- Doc differs: the \`:=\` assignment and \`\_\` as a letter (aa2fb2e, 2026-10-31) came after this line and are a later decision, not a conflict

## 03 Bootstrap and port to Linux

### Endianness and big-endian self-hosting
- Status: DONE — the byte order is the target's from LLVM's data layout, automatic (latest decision 2026-11-05 \[R171\])
- Decided: 2026-11-05 decided: "endian support" should be "automatic": "big little, whatever the platform has, that is what we compile for" \[R171\]
- Rationale: "big little, whatever the platform has, that is what we compile for" \[R171\]
- Precedent: LLVM IR (considered; named by author): "perhaps we should switch to llvm ir" for big-endian self-hosting \[R85\]
- Precedent: native words, endian-aware text (chosen; named by assistant, accepted): answered "Native words, endian-aware text (Recommended)" for big-endian hosts \[R86\]
- Evidence: 1382328 "The byte order of the target is LLVM's, from the data layout of its target machine, and --target no longer refuses the other order"; README.md:97-98 "Hosts are little- and big-endian, 32 and 64 bit"; tests/crossrun (s390x, powerpc, i386 under qemu-user)

### Fidelity to the original tape and rename policy
- Status: DONE — tape names restored, deviations documented, tape/ deleted, audit points at decus/ (latest decision 2026-11-03 \[R140\])
- Decided: 2026-11-03 decided: goes with the assistant's recommendations for the tape-name audit, "but they should be exmained again once channel io is back" \[R140\]
- Rationale: "don't deviate from the tape without a good reason" \[R76\]
- Rationale: "don't rename functions without a very good reason and without telling me." \[R75\]
- Precedent: the original tape's routine names (chosen; named by author): restore and keep tape names \[R76\]
- Evidence: 9a81151 "the tape's names back: CORE (was FCORE), CASE, LEXRES, SWITCH"; 620acd1 "tape/ is gone ... decus/ is the tape as received"; docs/porting/tape-names.md (the audit, "nothing renamed"); docs/ROADMAP.md "Open question: tape routines" (CORE and RLEAS to be examined together with channel I/O, item 5)
- Doc differs: channel I/O is not implemented (ROADMAP item 5), so the re-examination the line asks for is pending

### OpenBSD self-hosting failure
- Status: DONE — root cause found and removed; OpenBSD builds and passes the suite (latest decision 2026-10-22 \[R50\])
- Decided: 2026-10-22 decided: research all OpenBSD stack smashing mitigations, make sure none cause the bug, "find out the proper openbsd way to work with them" \[R50\]
- Rationale: "openbsd has all kinds of stack smashing mitigations; you should researach all of them nad make sure none of them are the source of the bug" \[R50\]
- Rationale: "find out why (on openbsd) i want answers and a lesson learned in your memories" \[R48\]
- Precedent: OpenBSD W^X (considered; named by author): suggested possible cause: memory page protections \[R47\]
- Precedent: OpenBSD RETGUARD (considered; named by author): one of the stack smashing mitigations to check \[R50\]
- Evidence: 6ef7f95 "OpenBSD's GNU objcopy 2.17 corrupts the COMDAT groups of the RETGUARD cookies; root cause of the non-PIE crash"; 7d329c0 "nm and objcopy are gone from the build"; 29fa20a "FreeBSD 15.0 and OpenBSD 8.0 pass"; the "lesson in memory" is outside the repo (not checked)

### Port plan and work order
- Status: DONE — the seed/stage work is finished: seed, stage 1, stage 2 build and agree (latest decision 2026-11-03 \[R135\])
- Decided: 2026-11-03 approved-proposal: "yes, do 1 then 2" (items 1 and 2 finish the seed/stage work) \[R135\]
- Rationale: Accepted from the assistant: items 1 and 2 "finish the seed/stage work", so "yes, do 1 then 2". \[R135\]
- Evidence: 8a2db1a "the seed has no PDP-10 in it" (2026-11-02, tests/seed: seed and stage 2 write the same IR); README.md:61-64 "The seed and stage 2 write byte-identical LLVM IR"; docs/BOOTSTRAP.md; items 1 and 2 themselves are not visible in the records (the 2026-11-03 "unclear" line), so the match is by the seed/stage content only

### Porting C and Python tools to IMP
- Status: DONE — the doc generators and helpers are IMP; the fuzzers stay Python by choice (latest decision 2026-11-03 \[R138\])
- Decided: 2026-11-03 decided: Python tools in make check/CI that should also become IMP: "Doc generators, Small helpers" \[R138\]
- Rationale: "other than the seed, and the linkage to llvm orc, why leave anything in C? what need loading, use ffi" \[R93\]
- Rationale: "why are you implementing this in C instead of in IMP? Didn't we do that very minimal IMP in C so that it could extend itself a bit at a time?" \[R56\]
- Precedent: Python tools (rejected; named by author): doc generators and small helpers should become IMP \[R138\]
- Evidence: 1548295 "the manual generators and four helpers are IMP, not Python"; tools/runoff2md.imp, runoff2mdoc\*.imp, pdp10tap.imp, oracle\*.imp, imptool.sh; 0618e6f "the fuzzers stay Python so that their reference does not come from the compiler under test"
- Doc differs: C left outside the seed: tools/ir2ll.c (the seed's IR writer, kept per 74aae52), stage0/llvmorc.c, impld.cpp, lvtriple.cpp (LLVM linkage, allowed by 2026-10-28), tests/\*\_main.c (C callers on purpose); tools/\*fuzz\*.py remain

### Stage-0 seed
- Status: DONE — the 36-bit emulation and trans.c are gone; the seed is C99 over the 64-bit generator (latest decision 2026-11-03 \[R134\])
- Decided: 2026-11-03 approved-proposal: "yes, do it. remove the 36 bit emulation": seed runs a C port of the stage generator, dropping trans.c. \[R134\]
- Rationale: "why does the seed in C have pdp-10 semantics? it should parse and compile the updated tape" \[R133\]
- Rationale: Accepted from the assistant: C port of the direct generator removes the duplicated implementations; "yes, do it. remove the 36 bit emulation". \[R134\]
- Rationale: "write bootstrap in c99; ... c99 is the best choice so pcc can compile it" \[R18\]
- Precedent: C99 (chosen; named by author): stage-0 language, so pcc can compile it \[R18\]
- Precedent: C23 (rejected; named by author): "i've repented"; pcc cannot compile it \[R18\]
- Precedent: GNU extensions (chosen; named by author): fine as long as pcc can compile them \[R19\]
- Precedent: pcc (chosen; named by author): in-process, with C fallback for arm64 macOS \[R17\]
- Evidence: 8a2db1a "stage0/gen\_\*.c is a C port of src/llvm-ir/{impsem,code,code2,code3,demit}.imp ... codegen.c, impsem.c, trans.c, pcode.c ... are gone"; stage0/ has gen\_code.c, gen\_impsem.c, gen\_demit.c and no trans.c; README.md:57 "The seed (\`stage0/\`, C99)"

### Tape translation to 64 bit
- Status: DONE — src/ is the 64-bit translation; decus/ is untouched; constants are in 64-bit form (latest decision 2026-10-23 \[R69\])
- Decided: 2026-10-23 decided: "better to translate the constants to 64bit form in the tape than to translate the constants into and out of 36bit at runtime" \[R69\]
- Rationale: "better to translate the constants to 64bit form in the tape than to translate the constants into and out of 36bit at runtime" \[R69\]
- Rationale: "except for the 36-\>64 shift, and ieee754 floats, and pccir output instead of pdp10 asm, it should all be the same" \[R65\]
- Precedent: PDP-10 (rejected; named by author): constants translated once, not at runtime \[R69\]
- Precedent: pdp10 asm output (rejected; named by author): dewop emits pccir instead \[R67\]
- Precedent: Lisp apply (considered; named by author): "isn't it like apply in lisp?" about EXECUTE \[R65\]
- Evidence: docs/porting/modules.md:3 "\`src/\` is the tape's source (\`decus/\`, which is kept as it is) translated to IMP for a 64 bit machine, a module at a time"; docs/LANGUAGE-DIFFERENCES.md "\`777777777777B\` is 2\*\*36-1, and -1 is \`1777777777777777777777B\`"; docs/porting/tape-names.md compares names; ad368cf EXECUTE does one instruction of the IR machine

## 04 Code generation and targets

### Backend: pcc IR to LLVM ORC JIT
- Status: DONE — pcc and the C back end are removed; LLVM ORC in process is the only back end; the optimisation level is still fixed (latest decision 2026-11-10 \[R181\], a potential)
- Decided: 2026-10-29 decided: "delete ir2c.imp and remove pcc and the c backend" \[R101\]
- Rationale: "i said to use the jit; why are you still keeping code to spawn clang?" \[R96\]
- Rationale: earlier: "since c backend is working, leave pcc alone for now, do the other stuff" \[R53\]
- Precedent: pcc (chosen; named by author): "we use pcc as the backend (at least initially, and possibly for quite a long time)" \[R14\]
- Precedent: pcc and C back end (rejected; named by author): "delete ir2c.imp and remove pcc and the c backend" \[R101\]
- Precedent: LLVM ORC JIT (chosen; named by author): "i said to use the jit" \[R96\]
- Precedent: spawning clang (rejected; named by author): questions keeping code to spawn clang \[R96\]
- Precedent: LLVM IR (considered; named by author): asked how hard to emit pcc IR or AST instead of LLVM IR \[R13\]
- Evidence: 0626138 "pcc and the C back end are removed: the third\_party/pcc submodule ... tools/ir2c.c"; 4a01132 "LLVM is the only back end of the drivers"; 5f1dfd8 docs; README.md:82 "LLVM is the back end, in the compiler's process"; docs/ARCHITECTURE.md (llvm module, ORC)
- Doc differs: stage0/ir.h:4, src/ir2ll.imp:9 and docs/internals/code-generator.md:175 still say the IR has "the operators and types of the pass 2 of pcc" (a description of the IR's origin, not a back end); the 2026-11-10 question about optimisation levels is dated after HEAD, src/llvm.imp:130 still runs \`default\<O1\>\`

### IEEE 754 floating point
- Status: DONE — REAL is a binary64 word in both generators and the library, with fuzz checks (latest decision 2026-10-29 \[R117\])
- Decided: 2026-10-29 approved-proposal: "start with item 5", the arithmetic check. \[R117\]
- Rationale: "not just the IR, fix the IMP src too, we are 64bit ieee now" \[R110\]
- Rationale: asked whether compares were updated for ieee 754, "just as we changed those constants that represented -1" \[R88\]
- Precedent: IEEE 754 floating point (chosen; named by author): "go ahead with binary64" \[R52\]
- Precedent: original 27-bit signed-magnitude REAL (considered; named by author): asked "what is that? why? how does that fit with ieee 754?" \[R87\]
- Evidence: docs/LANGUAGE-DIFFERENCES.md "Reals are IEEE 754 binary64"; docs/BUILDING.md tests/ieee/run.sh; 995071a "the seed's REAL word is the double's own bits"; 6fe8cb4 "the arithmetic of both code generators is 64 bit, checked against ... tools/imp-fuzz64.py"; tests/ieee, tools/ieee-fuzz.sh

### PDP-10 back end
- Status: DONE — no PDP-10 back end in the tree; it is a later nice-to-have, as decided (latest decision 2026-10-21 \[R43\])
- Decided: 2026-10-21 changed: "pdp-10 backend is a future nice thing to have; right now just get this thing working according to the original plan" \[R43\]
- Rationale: "a future nice thing to have; right now just get this thing working according to the original plan" \[R43\]
- Rationale: "it would be nice for imp72 to be able to compile itself for pdp-10, but the original 36 bit version" \[R45\]
- Precedent: PDP-10 (considered; named by author): compiling itself for the original 36-bit version, later \[R45\]
- Precedent: KA10 oracle (considered; named by assistant, accepted): "yes, start with rmod, amod and omod" to run hello on KA10; later deferred \[R42\]
- Evidence: not found: no rmod/amod/omod port or \*-pdp10 module in src/ or stage0/; docs/porting/tape-names.md lists amod/omod/rmod as "no counterpart: \`demit.imp\` writes IR and LLVM does this work"; 8a2db1a "the seed has no PDP-10 in it"

# Precedents, by name

- The systems, languages, standards and designs named as referents, with the subjects they shaped; chosen = his choice follows it, rejected = he turned it down, considered = compared or cited.

- pcc: chosen in Project goal, native port and fidelity; Stage-0 seed; Backend: pcc IR to LLVM ORC JIT; README and documentation, considered in Target platforms and coverage
- PDP-10: rejected in Project goal, native port and fidelity; Tape translation to 64 bit, considered in Naming of sem, impsem and codegen files; PDP-10 back end; README and documentation
- ALGOL 60: chosen in hbnf scope and parser options, considered in Index base
- C long: rejected in C interop, considered in Return type and imp\_word typedef
- clang and gcc linking: considered in Target platforms and coverage; Linking
- IMP72: chosen in Index base, considered in hbnf scope and parser options
- LLVM IR: considered in Endianness and big-endian self-hosting; Backend: pcc IR to LLVM ORC JIT
- LLVM ORC JIT: chosen in Backend: pcc IR to LLVM ORC JIT; Removing pcc names
- META II: considered in imp72 compared with meta2, Ada and hbnf; hbnf scope and parser options
- pcc IR: rejected in Removing pcc names, considered in Machine word and 36-bit target
- TOPS-10: rejected in Project goal, native port and fidelity, considered in TOPS-10 ABI and CALLI
- 64-bit word, byte addresses (C ABI): chosen in Machine word and 36-bit target
- ABNF: considered in W-grammar and Algol 68 in hbnf
- Ada: considered in imp72 compared with meta2, Ada and hbnf
- ALGOL: considered in imp72 compared with meta2, Ada and hbnf
- ALGOL 68: considered in W-grammar and Algol 68 in hbnf
- Alpine aports: chosen in Debian, OpenBSD and Alpine packages
- Alpine s390x: chosen in Architecture test matrix
- ::= := = and :: considered in W-grammar and Algol 68 in hbnf
- C: chosen in Index base
- C fallback for arm64 macOS: chosen in Target platforms and coverage
- C identifier rules: chosen in Command-line flags and object files
- C names: chosen in Case folding and valid names
- C23: rejected in Stage-0 seed
- C99: chosen in Stage-0 seed
- classic Unix leading underscore: chosen in Linking
- Debian amd64, i386, powerpc, s390x: chosen in Architecture test matrix
- Debian packaging: chosen in Debian, OpenBSD and Alpine packages
- DECUS documentation and sources for imp72: chosen in README and documentation
- decus tapes (IMP72 original): chosen in Project goal, native port and fidelity
- djb tmpfile practices: chosen in Bounded strings and temporary files
- docker or qemu before CI: chosen in Local VMs and local-before-CI
- ELF/Mach-O object files: chosen in Interactive mode, switches and TOPS-10 compatibility
- facet language: considered in Operator set and precedence
- fopen/putc (C stdio): considered in Channel I/O
- FreeBSD before OpenBSD: chosen in Local VMs and local-before-CI
- GNU extensions: chosen in Stage-0 seed
- hbnf: considered in imp72 compared with meta2, Ada and hbnf
- i386 for 32-bit little-endian: chosen in Architecture test matrix
- IEEE 754 floating point: chosen in IEEE 754 floating point
- imp\_word typedef: rejected in Return type and imp\_word typedef
- IMP72 36-bit word-addressed memory: rejected in Machine word and 36-bit target
- IMP72 ASCIZ strings: chosen in String representation
- imp72 parsing C++: considered in hbnf scope and parser options
- imp72 planned error mapping to original text: considered in COBOL preprocessing in hbnf
- IMP72 precedence rules: chosen in Operator set and precedence
- ISC licence (2 clause): chosen in Origin, license and push policy
- KA10 oracle: considered in PDP-10 back end
- length-carrying strings: chosen in String representation
- libc realloc: chosen in TOPS-10 ABI and CALLI
- Linux /proc checkpointing: rejected in Interactive mode, switches and TOPS-10 compatibility
- Lisp apply: considered in Tape translation to 64 bit
- lld in-process: chosen in Linking
- LLVM: chosen in Project goal, native port and fidelity
- LLVM and mingw (Windows): chosen in Target platforms and coverage
- LLVM ORC JIT and liblld: chosen in imp72 compared with meta2, Ada and hbnf
- local qemu OpenBSD and NetBSD images: chosen in Local VMs and local-before-CI
- modern C string and character syntax: considered in Studies deferred to later releases
- modern ELF symbol names: chosen in Assignment token and the \_ to := conversion
- musl: considered in musl changes
- native ld / ld.lld: chosen in Linking
- native words, endian-aware text: chosen in Endianness and big-endian self-hosting
- NetBSD/macppc in qemu-system-ppc: considered in Architecture test matrix
- NUL-terminated C strings: considered in String representation
- OpenBSD ports: chosen in Debian, OpenBSD and Alpine packages
- OpenBSD RETGUARD: considered in OpenBSD self-hosting failure
- OpenBSD srand\_deterministic: chosen in OpenBSD srand
- OpenBSD strlcpy and strlcat: chosen in Bounded strings and temporary files
- OpenBSD W^X: considered in OpenBSD self-hosting failure
- original 27-bit signed-magnitude REAL: considered in IEEE 754 floating point
- original channel I/O: considered in Channel I/O
- original IMP on TOPS-10: chosen in Interactive mode, switches and TOPS-10 compatibility
- original IMP72: chosen in Syntax freeze
- original IMP72 \`\_\` assignment: considered in Assignment token and the \_ to := conversion
- original IMP72 base-relative addressing: chosen in PIE and relocatable images
- Pascal/Ada/ALGOL 68: considered in Index base
- pcc and C back end: rejected in Backend: pcc IR to LLVM ORC JIT
- pcc back end: considered in Removing pcc names
- pdp10 asm output: rejected in Tape translation to 64 bit
- PIE: chosen in PIE and relocatable images
- plain CALL: rejected in CCALL
- Python: considered in Index base
- Python tools: rejected in Porting C and Python tools to IMP
- .rel object files: rejected in Command-line flags and object files
- removing the repeatability check: rejected in OpenBSD srand
- RFC packet diagrams: chosen in Index base
- SemVer: considered in Versioning and releases
- SIMH PDP-10 KA10: considered in prebsd repository
- SIMH + TOPS-10: chosen in Machine word and 36-bit target
- SNOBOL4: considered in imp72 compared with meta2, Ada and hbnf
- spawning clang: rejected in Backend: pcc IR to LLVM ORC JIT
- standalone lld fork: rejected in Linking
- the original compiler (oracle): chosen in Reference comparison with the original
- the original tape's routine names: chosen in Fidelity to the original tape and rename policy
- TOPS-10 IMP72 case rules: considered in Case folding and valid names
- TOPS-10 /U and /V in -i mode: considered in Command-line flags and object files
- Unix compiler -o -c -S behaviour: chosen in Command-line flags and object files
- unix filename limits: considered in Studies deferred to later releases
- UTF-8: considered in Channel I/O
- Van Wijngaarden one-level form using ':': considered in W-grammar and Algol 68 in hbnf
- W-grammar: considered in W-grammar and Algol 68 in hbnf
- yacc: considered in hbnf scope and parser options

# Where the current docs or code differ from the decisions (20)

- Syntax freeze: the \`:=\` assignment and \`\_\` as a letter (aa2fb2e, 2026-10-31) came after this line and are a later decision, not a conflict
- Interactive mode, switches and TOPS-10 compatibility: docs/imp72.1 documents \`NAME\_/U\` with the \`\_\` separator in the -i command line, which docs/LANGUAGE-DIFFERENCES.md confirms stays (the command line is not IMP source)
- String representation: ROADMAP item 3 proposes \`''\` as the constant 0, which is the "original meaning" per the manual; not yet implemented
- Assignment token and the \_ to := conversion: docs/ORACLE.md:53 says "\`tools/imp-assign.imp -T\` converts \`:=\`"
- C interop: the potentials (generated .h, Zig/Rust, init/run split) are ROADMAP items 8 and 9, "None is implemented"; -lm added in 7ac3ec2 as decided
- Case folding and valid names: docs/manual/imp72.7:216 "Names are case sensitive, as the original's are" (the TOPS-10 premise of the condition is answered "as on TOPS-10", so no folding)
- Porting C and Python tools to IMP: C left outside the seed: tools/ir2ll.c (the seed's IR writer, kept per 74aae52), stage0/llvmorc.c, impld.cpp, lvtriple.cpp (LLVM linkage, allowed by 2026-10-28), tests/\*\_main.c (C callers on purpose); tools/\*fuzz\*.py remain
- TOPS-10 ABI and CALLI: docs/porting/tape-names.md says CORE "makes the first \`K+1\` words of the reserved arena usable", not a realloc
- Fidelity to the original tape and rename policy: channel I/O is not implemented (ROADMAP item 5), so the re-examination the line asks for is pending
- Backend: pcc IR to LLVM ORC JIT: stage0/ir.h:4, src/ir2ll.imp:9 and docs/internals/code-generator.md:175 still say the IR has "the operators and types of the pass 2 of pcc" (a description of the IR's origin, not a back end); the 2026-11-10 question about optimisation levels is dated after HEAD, src/llvm.imp:130 still runs \`default\<O1\>\`
- Target platforms and coverage: docs/ROADMAP.md:### 11 "A runtime library for Windows and Cygwin" is not done ("There is no runtime library for Windows or Cygwin"); 130226f "OpenBSD, macOS, older LLVM headers: not run"; Cygwin values "from memory and marked unverified" (25979df)
- Optimization default: docs/ARCHITECTURE.md:52 says "optimizes it at \`default\<O1\>\`"
- Linking: docs/BUILDING.md:225 says the Cygwin row is unverified (values from memory, not run on Cygwin)
- Command-line flags and object files: docs/ROADMAP.md item 10 says the seed's -S still writes IR and it has no -Sir (parity not done, the seed is not imp72)
- Removing pcc names: stage0/ir.h:4, src/ir2ll.imp:9 and docs/internals/code-generator.md:175 still name pcc as the source of the IR tree format (kept on purpose per f0db94a)
- Architecture test matrix: README.md:96 lists the CI hosts; Debian powerpc/i386 and the OpenBSD/NetBSD i386 CI runs of the earlier lines are not in the workflows (ci-bsd.yml uses the amd64 VM actions)
- Debian, OpenBSD and Alpine packages: packaging/README.md says the port has no distinfo until the tag exists (not yet submitted)
- No /tmp: docs/BUILDING.md:93 says "\`/tmp\` for an installed compiler" (commit c6364a5), and tools/mktmpdir.sh falls back to /tmp
- hbnf scope and parser options: ABNF.md:112 says "Off by default" and stderr, where the decision says stdout; only the default and the stream differ
- W-grammar and Algol 68 in hbnf: the Algol 68 grammar itself is not started (hbnf TODO.md:101 "ALGOL 68, C++ grammars: not started"), a different subject
