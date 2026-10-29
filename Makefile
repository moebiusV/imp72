# Top level: stage 0 (the seed), the runtime library, stages 1 and 2, and the tests.
#
#   make            build stage 0, the library and the compilers
#   make check      run all the tests
#
# The back end is LLVM's ORC JIT, in the process: the LLVM C API must be installed (tools/llvm.sh).

# Compiler flags and where to install (make install DESTDIR=... PREFIX=...).
CC       = cc
CFLAGS   = -O1 -g -Wall
LDFLAGS  =
DESTDIR  =
PREFIX   = /usr/local

LIB = build/lib/libimp72.a

PCCIR = build/pccir2/imp72d          # the compiler with the code generator of src/pccir (tools/pccir.sh), where it can be built

all: stage0 $(LIB) pccir

$(LIB): stage0 tools/mklib.sh lib/imp72rt.imp lib/imp72io.imp lib/imp72sys.c lib/imp72main.imp
	CFLAGS="$(CFLAGS)" sh tools/mklib.sh

# stage 1 by the seed, stage 2 by stage 1 (skipped where the LLVM libraries are missing)
pccir: stage0 $(LIB)
	@sh tools/pccir.sh; r=$$?; if [ $$r != 0 ] && [ $$r != 77 ]; then exit $$r; fi

stage0:
	lf=`sh tools/llvm.sh cflags`; ll=`sh tools/llvm.sh libs`; lo=llvmorc.o; [ -n "$$lf" ] && lo="llvmorc.o ir2ll.o"; case "$$lf" in *HAVE_LLD*) lo="$$lo impld.o";; esac; \
	bd=`sh tools/llvm.sh bindir`; bdd=; [ -n "$$bd" ] && bdd="-DIMP72_LLVM_BINDIR=$$bd"; \
	$(MAKE) -C stage0 CC="$(CC)" CFLAGS="-std=c99 -pedantic $(CFLAGS)" LDFLAGS="$(LDFLAGS)" LLVMCFLAGS="$$lf" LLVMLIBS="$$ll" LLVMOBJS="$$lo" LLVMBINDIRDEF="$$bdd"

# Installed layout (imp72stage0 finds its parts relative to its own place):
#   PREFIX/bin/imp72stage0 (the seed; also as imp72 where stage 2 cannot be built)   PREFIX/bin/imp72 (stage 2)
#   PREFIX/lib/imp72/libimp72.a   PREFIX/share/imp72/syntax64.imc
#   PREFIX/share/imp72/{syntax.imc,ffi.imc}  PREFIX/share/man/man1/imp72stage0.1
install: all
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(PREFIX)/lib/imp72 \
	    $(DESTDIR)$(PREFIX)/share/imp72 $(DESTDIR)$(PREFIX)/share/man/man1
	install -m 755 stage0/imp72s0 $(DESTDIR)$(PREFIX)/bin/imp72stage0
	rm -f $(DESTDIR)$(PREFIX)/bin/imp72
	if [ -x $(PCCIR) ]; then \
	    install -m 755 $(PCCIR) $(DESTDIR)$(PREFIX)/bin/imp72; \
	    install -m 644 src/pccir/syntax64.imc $(DESTDIR)$(PREFIX)/share/imp72/syntax64.imc; \
	else ln -sf imp72stage0 $(DESTDIR)$(PREFIX)/bin/imp72; fi
	install -m 644 $(LIB) $(DESTDIR)$(PREFIX)/lib/imp72/libimp72.a
	install -d $(DESTDIR)$(PREFIX)/include
	install -m 644 lib/imp72.h $(DESTDIR)$(PREFIX)/include/imp72.h
	install -m 644 decus/syntax.imc lib/ffi.imc $(DESTDIR)$(PREFIX)/share/imp72/
	install -m 644 docs/imp72stage0.1 $(DESTDIR)$(PREFIX)/share/man/man1/imp72stage0.1
	echo '.so man1/imp72stage0.1' > $(DESTDIR)$(PREFIX)/share/man/man1/imp72.1
	install -d $(DESTDIR)$(PREFIX)/share/man/man7
	install -m 644 docs/manual/imp72.7 $(DESTDIR)$(PREFIX)/share/man/man7/imp72.7
	install -d $(DESTDIR)$(PREFIX)/share/doc/imp72
	install -m 644 docs/manual/imp72.md docs/FFI.md docs/LANGUAGE-NOTES.md $(DESTDIR)$(PREFIX)/share/doc/imp72/

check: all
	lf=`sh tools/llvm.sh cflags`; ll=`sh tools/llvm.sh libs`; lo=llvmorc.o; [ -n "$$lf" ] && lo="llvmorc.o ir2ll.o"; case "$$lf" in *HAVE_LLD*) lo="$$lo impld.o";; esac; \
	bd=`sh tools/llvm.sh bindir`; bdd=; [ -n "$$bd" ] && bdd="-DIMP72_LLVM_BINDIR=$$bd"; \
	$(MAKE) -C stage0 check CC="$(CC)" CFLAGS="-std=c99 -pedantic $(CFLAGS)" LDFLAGS="$(LDFLAGS)" LLVMCFLAGS="$$lf" LLVMLIBS="$$ll" LLVMOBJS="$$lo" LLVMBINDIRDEF="$$bdd"
	sh tests/native/run.sh
	sh tests/oracle/run.sh
	sh tests/oracle/dumps.sh
	sh tests/ieee/run.sh
	sh tests/direct/run.sh
	sh tests/direct/stage2.sh
	sh tests/direct/ir2ll.sh
	sh tests/interactive/run.sh
	sh tests/image/run.sh
	sh tests/orc/run.sh
	sh tests/llvm/gen.sh
	sh tests/link/run.sh
	sh tests/fuzz64/run.sh
	sh tests/manual/run.sh

clean:
	$(MAKE) -C stage0 clean
	rm -rf build IOTEST.TMP

distclean: clean

.PHONY: all stage0 pccir check clean distclean install
