# Top level: stage 0 (the seed), the runtime library, stages 1 and 2, and the tests.
#
#   make            build stage 0, the library and the compilers
#   make check      run all the tests
#
# The back end is LLVM's ORC JIT, in the process: the LLVM C API must be installed (tools/llvm.sh).

# Compiler flags and where to install (make install DESTDIR=... PREFIX=...).
CC       = cc
CXX      = c++
export CC CXX                           # the scripts of tools/ use them too
CFLAGS   = -O1 -g -Wall
LDFLAGS  =
DESTDIR  =
PREFIX   = /usr/local
MANDIR   = $(PREFIX)/share/man
DOCDIR   = $(PREFIX)/share/doc/imp

LIB = build/lib/libimp.a             # the runtime library the seed makes (tools/mklib.sh): the compilers are linked with it
RTLIB = build/rt-host/libimp.a        # the same library made by the compiler itself, stage 2 (tools/mkrt.sh): what is installed, where stage 2 could be built

LLVMIR = build/llvm-ir2/impd          # the compiler with the code generator of src/llvm-ir (tools/llvm-ir.sh), where it can be built

all: stage0 $(LIB) llvm-ir runtime cimport

$(LIB): stage0 tools/mklib.sh lib/imprt.imp lib/impio.imp lib/impsys.imp lib/imphost.imp lib/imphostx-elf.imp lib/imphostx-macho.imp lib/impvars.imp lib/impvarsx-elf.imp lib/impvarsx-macho.imp tools/sysconst.sh lib/impmain.imp
	CFLAGS="$(CFLAGS)" sh tools/guard.sh sh tools/mklib.sh

# the runtime library by stage 2, no C compiler (skipped where stage 2 could not be built)
runtime: llvm-ir
	@if [ -x build/llvm-ir2/impstage-llvm-ir ]; then $(MAKE) $(RTLIB) || exit 1; fi

$(RTLIB): build/llvm-ir2/impstage-llvm-ir tools/mkrt.sh tools/sysconst.sh lib/imprt.imp lib/impio.imp lib/impsys.imp lib/imphost.imp lib/imphostx-elf.imp lib/imphostx-macho.imp lib/impvars.imp lib/impvarsx-elf.imp lib/impvarsx-macho.imp lib/impmain.imp
	sh tools/guard.sh sh tools/mkrt.sh "" build/rt-host

# the tool imp-cimport (tools/cimport*.imp, an IMP program the compiler of the tree builds; skipped where stage 2 could not be built)
cimport: runtime
	@if [ -x build/llvm-ir2/impstage-llvm-ir ]; then sh tools/imptool.sh cimport -h >/dev/null 2>&1; [ $$? = 2 ] || { sh tools/imptool.sh cimport -h; exit 1; }; fi

# stage 1 by the seed, stage 2 by stage 1 (skipped where the LLVM libraries are missing)
llvm-ir: stage0 $(LIB)
	@LDFLAGS="$(LDFLAGS)" sh tools/guard.sh sh tools/llvm-ir.sh; r=$$?; if [ $$r != 0 ] && [ $$r != 77 ]; then exit $$r; fi

stage0:
	sf=`sh tools/hasstrl.sh`; lf=`sh tools/llvm.sh cflags`; ll=`sh tools/llvm.sh libs`; lo=llvmorc.o; [ -n "$$lf" ] && lo="llvmorc.o ir2ll.o lvtriple.o"; case "$$lf" in *HAVE_LLD*) lo="$$lo impld.o";; esac; \
	bd=`sh tools/llvm.sh bindir`; bdd=; [ -n "$$bd" ] && bdd="-DIMP_LLVM_BINDIR=$$bd"; \
	$(MAKE) -C stage0 CC="$(CC)" CXX="$(CXX)" CFLAGS="-std=c99 -pedantic $(CFLAGS) $$sf" LDFLAGS="$(LDFLAGS)" LLVMCFLAGS="$$lf" LLVMLIBS="$$ll" LLVMOBJS="$$lo" LLVMBINDIRDEF="$$bdd"

# Installed layout (impstage0 finds its parts relative to its own place):
#   PREFIX/bin/impstage0 (the seed; also as imp where stage 2 cannot be built)   PREFIX/bin/imp (stage 2)
#   PREFIX/lib/imp/libimp.a (made by stage 2, else by the seed)   PREFIX/share/imp/syntax64.imc
#   PREFIX/share/imp/ffi.imc  MANDIR/man1/{imp.1,impstage0.1} (MANDIR is PREFIX/share/man)
#   PREFIX/bin/imp-cimport and MANDIR/man1/imp-cimport.1 (where stage 2 is built)
install: all
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(PREFIX)/lib/imp \
	    $(DESTDIR)$(PREFIX)/share/imp $(DESTDIR)$(MANDIR)/man1
	install -m 755 stage0/imps0 $(DESTDIR)$(PREFIX)/bin/impstage0
	rm -f $(DESTDIR)$(PREFIX)/bin/imp
	if [ -x $(LLVMIR) ]; then \
	    install -m 755 $(LLVMIR) $(DESTDIR)$(PREFIX)/bin/imp; \
	    install -m 644 src/llvm-ir/syntax64.imc $(DESTDIR)$(PREFIX)/share/imp/syntax64.imc; \
	else ln -sf impstage0 $(DESTDIR)$(PREFIX)/bin/imp; fi
	if [ -f $(RTLIB) ]; then install -m 644 $(RTLIB) $(DESTDIR)$(PREFIX)/lib/imp/libimp.a; \
	else install -m 644 $(LIB) $(DESTDIR)$(PREFIX)/lib/imp/libimp.a; fi
	install -d $(DESTDIR)$(PREFIX)/include
	install -m 644 lib/imp.h $(DESTDIR)$(PREFIX)/include/imp.h
	install -m 644 src/llvm-ir/syntax64.imc lib/ffi.imc $(DESTDIR)$(PREFIX)/share/imp/
	install -m 644 docs/impstage0.1 $(DESTDIR)$(MANDIR)/man1/impstage0.1
	install -m 644 docs/imp.1 $(DESTDIR)$(MANDIR)/man1/imp.1
	if [ -x build/tools/cimport ]; then \
	    install -m 755 build/tools/cimport $(DESTDIR)$(PREFIX)/bin/imp-cimport; \
	    install -m 644 docs/cimport.1 $(DESTDIR)$(MANDIR)/man1/imp-cimport.1; fi
	install -d $(DESTDIR)$(MANDIR)/man7
	install -m 644 docs/manual/imp.7 $(DESTDIR)$(MANDIR)/man7/imp.7
	install -d $(DESTDIR)$(DOCDIR)
	install -m 644 docs/manual/imp.md docs/FFI.md docs/LANGUAGE-NOTES.md $(DESTDIR)$(DOCDIR)/

# (the whole suite runs under tools/guard.sh: a job that grows build/ and tmp/ past 10 GB is killed, not the disk)
check: all
	@sh tools/guard.sh $(MAKE) check-run

check-run:
	sf=`sh tools/hasstrl.sh`; lf=`sh tools/llvm.sh cflags`; ll=`sh tools/llvm.sh libs`; lo=llvmorc.o; [ -n "$$lf" ] && lo="llvmorc.o ir2ll.o lvtriple.o"; case "$$lf" in *HAVE_LLD*) lo="$$lo impld.o";; esac; \
	bd=`sh tools/llvm.sh bindir`; bdd=; [ -n "$$bd" ] && bdd="-DIMP_LLVM_BINDIR=$$bd"; \
	$(MAKE) -C stage0 check CC="$(CC)" CFLAGS="-std=c99 -pedantic $(CFLAGS) $$sf" LDFLAGS="$(LDFLAGS)" LLVMCFLAGS="$$lf" LLVMLIBS="$$ll" LLVMOBJS="$$lo" LLVMBINDIRDEF="$$bdd"
	sh tests/native/run.sh
	sh tests/oracle/run.sh
	sh tests/ieee/run.sh
	sh tests/direct/run.sh
	sh tests/direct/stage2.sh
	sh tests/direct/ir2ll.sh
	sh tests/interactive/run.sh
	sh tests/image/run.sh
	sh tests/orc/run.sh
	sh tests/llvm/gen.sh
	sh tests/link/run.sh
	sh tests/link/cross.sh
	sh tests/runtime/run.sh
	sh tests/version/run.sh
	sh tests/variadic/run.sh
	sh tests/tmpfiles/run.sh
	sh tests/seed/run.sh
	sh tests/cli/run.sh
	sh tests/cimport/run.sh
	sh tests/fuzz64/run.sh
	sh tests/manual/run.sh
	sh tests/crossrun/run.sh || [ $$? = 77 ]

clean:
	$(MAKE) -C stage0 clean
	rm -rf build IOTEST.TMP

distclean: clean

.PHONY: all stage0 llvm-ir runtime cimport check clean distclean install
