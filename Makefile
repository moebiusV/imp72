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
WITH_PCC = auto
PCCCONF  =		# extra arguments for pcc's configure (--build=i686-pc-linux-musl in a 32 bit userland)

LIB = build/lib/libimp72.a

IR2C     = build/bin/ir2c

all: stage0 $(LIB) $(IR2C) pcc

$(IR2C): tools/ir2c.c
	mkdir -p build/bin
	$(CC) -std=gnu99 $(CFLAGS) $(LDFLAGS) -o $(IR2C) tools/ir2c.c

$(LIB): lib/imp72rt.c lib/imp72io.c lib/imp72main.c
	mkdir -p build/lib
	$(CC) -std=c99 $(CFLAGS) -c -o build/lib/imp72rt.o lib/imp72rt.c
	$(CC) -std=c99 $(CFLAGS) -c -o build/lib/imp72io.o lib/imp72io.c
	$(CC) -std=c99 $(CFLAGS) -c -o build/lib/imp72main.o lib/imp72main.c
	rm -f $(LIB)
	ar rcs $(LIB) build/lib/imp72rt.o build/lib/imp72io.o build/lib/imp72main.o

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
	else echo "pcc not built: imp72stage0 will use the C back end"; fi

stage0:
	$(MAKE) -C stage0 CC="$(CC)" CFLAGS="-std=c99 -pedantic $(CFLAGS)" LDFLAGS="$(LDFLAGS)"

# Installed layout (imp72stage0 finds its parts relative to its own place):
#   PREFIX/bin/imp72stage0 (also as imp72)   PREFIX/lib/imp72/{ir2c,cc1,libimp72.a}
#   PREFIX/share/imp72/{syntax.imc,ffi.imc}  PREFIX/share/man/man1/imp72stage0.1
install: all
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(PREFIX)/lib/imp72 \
	    $(DESTDIR)$(PREFIX)/share/imp72 $(DESTDIR)$(PREFIX)/share/man/man1
	install -m 755 stage0/imp72s0 $(DESTDIR)$(PREFIX)/bin/imp72stage0
	ln -sf imp72stage0 $(DESTDIR)$(PREFIX)/bin/imp72
	install -m 755 $(IR2C) $(DESTDIR)$(PREFIX)/lib/imp72/ir2c
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
	CC1=$(CURDIR)/$(CC1) sh tests/self/run.sh
	CC1=$(CURDIR)/$(CC1) sh tests/self/stage1.sh
	CC1=$(CURDIR)/$(CC1) sh tests/self/stage2.sh
	sh tests/self/objnorm.sh
	sh tests/manual/run.sh

clean:
	$(MAKE) -C stage0 clean
	rm -rf build IOTEST.TMP

distclean: clean

.PHONY: all stage0 pcc check clean distclean install
