# Top level: pcc's pass 2 (cc1), stage 0 and its tests, and the native tests.
#
#   make            build cc1 and stage 0
#   make check      run all the tests
#
# cc1 is the pass 2 compiler of pcc (third_party/pcc) built to read the IR text
# our translator writes; it needs only a C compiler (no yacc or lex).
PCCBUILD = build/pcc
CC1      = $(PCCBUILD)/cc/ccom/cc1

# Compiler flags and where to install (make install DESTDIR=... PREFIX=...).
CC       = cc
CFLAGS   = -O1 -g -Wall
LDFLAGS  =
DESTDIR  =
PREFIX   = /usr/local

# pcc's code generator is used where pcc works and the translator has a table
# for it: amd64 and aarch64.
# yes, no, or auto (yes on x86_64 and aarch64).  Elsewhere imp72stage0 uses the C back end.
WITH_PCC = no
PCCCONF  =		# extra arguments for pcc's configure (--build=i686-pc-linux-musl in a 32 bit userland)

LIB = build/lib/libimp72.a

IR2C     = build/bin/ir2c
IR2LL    = build/bin/ir2ll

LIBRAW = build/lib/libimp72raw.a      # the library for the code generator of src/pccir: a REAL word is the double's own bits

PCCIR = build/pccir2/imp72d          # the compiler with the code generator of src/pccir (tools/pccir.sh), where it can be built

all: stage0 $(LIB) $(LIBRAW) $(IR2C) $(IR2LL) pcc pccir

$(IR2C): tools/ir2c.c
	mkdir -p build/bin
	$(CC) -std=gnu99 $(CFLAGS) $(LDFLAGS) -o $(IR2C) tools/ir2c.c

$(IR2LL): tools/ir2ll.c
	mkdir -p build/bin
	$(CC) -std=gnu99 $(CFLAGS) $(LDFLAGS) -o $(IR2LL) tools/ir2ll.c

$(LIB): lib/imp72rt.c lib/imp72io.c lib/imp72main.c
	mkdir -p build/lib
	$(CC) -std=c99 $(CFLAGS) -c -o build/lib/imp72rt.o lib/imp72rt.c
	$(CC) -std=c99 $(CFLAGS) -c -o build/lib/imp72io.o lib/imp72io.c
	$(CC) -std=c99 $(CFLAGS) -c -o build/lib/imp72main.o lib/imp72main.c
	rm -f $(LIB)
	ar rcs $(LIB) build/lib/imp72rt.o build/lib/imp72io.o build/lib/imp72main.o

$(LIBRAW): lib/imp72rt.c lib/imp72io.c lib/imp72main.c lib/imp72real.h
	mkdir -p build/lib
	$(CC) -std=c99 -DIMP_REAL_RAW $(CFLAGS) -c -o build/lib/imp72rt_raw.o lib/imp72rt.c
	$(CC) -std=c99 -DIMP_REAL_RAW $(CFLAGS) -c -o build/lib/imp72io_raw.o lib/imp72io.c
	rm -f $(LIBRAW)
	ar rcs $(LIBRAW) build/lib/imp72rt_raw.o build/lib/imp72io_raw.o build/lib/imp72main.o

$(CC1):
	mkdir -p $(PCCBUILD)
	cd $(PCCBUILD) && ../../third_party/pcc/configure --enable-twopass $(PCCCONF) \
	    >configure.log 2>&1
	touch $(PCCBUILD)/cc/ccom/cgram.c $(PCCBUILD)/cc/ccom/cgram.h \
	    $(PCCBUILD)/cc/ccom/scan.c
	$(MAKE) -C $(PCCBUILD)/cc/ccom external.c >$(PCCBUILD)/make.log 2>&1
	$(MAKE) -C $(PCCBUILD)/cc/ccom cc1 >>$(PCCBUILD)/make.log 2>&1

# cc1, if wanted
pcc:
	@if [ "$(WITH_PCC)" = yes ] || { [ "$(WITH_PCC)" = auto ] && { [ "`uname -s`" != Darwin ] && case "`uname -m`" in x86_64|aarch64|arm64|i?86|armv[67]*) true;; *) false;; esac; }; }; then \
	    $(MAKE) $(CC1); \
	else echo "pcc not built (WITH_PCC=yes builds it): the back end is LLVM where there is a clang, else C"; fi

# stage 1 by the seed, stage 2 by stage 1 (skipped where an ELF objcopy is missing); the back end is LLVM (clang) or C, pcc if asked for
pccir: stage0 $(LIB) $(LIBRAW) pcc
	@CC1=$(CURDIR)/$(CC1) sh tools/pccir.sh; r=$$?; if [ $$r != 0 ] && [ $$r != 77 ]; then exit $$r; fi

stage0:
	$(MAKE) -C stage0 CC="$(CC)" CFLAGS="-std=c99 -pedantic $(CFLAGS)" LDFLAGS="$(LDFLAGS)"

# Installed layout (imp72stage0 finds its parts relative to its own place):
#   PREFIX/bin/imp72stage0 (the seed; also as imp72 where the pcc IR compiler cannot be built)   PREFIX/bin/imp72 (the pcc IR compiler)
#   PREFIX/lib/imp72/{ir2c,ir2ll,cc1,libimp72.a,libimp72raw.a}   PREFIX/share/imp72/syntax64.imc
#   PREFIX/share/imp72/{syntax.imc,ffi.imc}  PREFIX/share/man/man1/imp72stage0.1
install: all
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(PREFIX)/lib/imp72 \
	    $(DESTDIR)$(PREFIX)/share/imp72 $(DESTDIR)$(PREFIX)/share/man/man1
	install -m 755 stage0/imp72s0 $(DESTDIR)$(PREFIX)/bin/imp72stage0
	rm -f $(DESTDIR)$(PREFIX)/bin/imp72
	if [ -x $(PCCIR) ]; then \
	    install -m 755 $(PCCIR) $(DESTDIR)$(PREFIX)/bin/imp72; \
	    install -m 644 $(LIBRAW) $(DESTDIR)$(PREFIX)/lib/imp72/libimp72raw.a; \
	    install -m 644 src/pccir/syntax64.imc $(DESTDIR)$(PREFIX)/share/imp72/syntax64.imc; \
	else ln -sf imp72stage0 $(DESTDIR)$(PREFIX)/bin/imp72; fi
	install -m 755 $(IR2C) $(DESTDIR)$(PREFIX)/lib/imp72/ir2c
	install -m 755 $(IR2LL) $(DESTDIR)$(PREFIX)/lib/imp72/ir2ll
	if [ -x $(CC1) ]; then install -m 755 $(CC1) $(DESTDIR)$(PREFIX)/lib/imp72/cc1; fi
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
	$(MAKE) -C stage0 check CC="$(CC)" CFLAGS="-std=c99 -pedantic $(CFLAGS)" LDFLAGS="$(LDFLAGS)"
	CC1=$(CURDIR)/$(CC1) sh tests/native/run.sh
	CC1=$(CURDIR)/$(CC1) sh tests/oracle/run.sh
	sh tests/native/runc.sh
	sh tests/self/objnorm.sh
	CC1=$(CURDIR)/$(CC1) sh tests/ieee/run.sh
	CC1=$(CURDIR)/$(CC1) sh tests/direct/run.sh
	CC1=$(CURDIR)/$(CC1) sh tests/direct/stage2.sh
	sh tests/interactive/run.sh
	sh tests/image/run.sh
	sh tests/orc/run.sh
	sh tests/manual/run.sh

clean:
	$(MAKE) -C stage0 clean
	rm -rf build IOTEST.TMP

distclean: clean

.PHONY: all stage0 pcc pccir check clean distclean install
