# Top level: pcc's pass 2 (cc1), stage 0 and its tests, and the native tests.
#
#   make            build cc1 and stage 0
#   make check      run all the tests
#
# cc1 is the pass 2 compiler of pcc (third_party/pcc) built to read the IR text
# our translator writes; it needs only a C compiler (no yacc or lex).
PCCBUILD = build/pcc
CC1      = $(PCCBUILD)/cc/ccom/cc1

all: $(CC1) stage0

$(CC1):
	mkdir -p $(PCCBUILD)
	cd $(PCCBUILD) && ../../third_party/pcc/configure --enable-twopass \
	    >configure.log 2>&1
	touch $(PCCBUILD)/cc/ccom/cgram.c $(PCCBUILD)/cc/ccom/cgram.h \
	    $(PCCBUILD)/cc/ccom/scan.c
	$(MAKE) -C $(PCCBUILD)/cc/ccom cc1 >$(PCCBUILD)/make.log 2>&1

stage0:
	$(MAKE) -C stage0

check: all
	$(MAKE) -C stage0 check
	CC1=$(CURDIR)/$(CC1) sh tests/native/run.sh

clean:
	$(MAKE) -C stage0 clean
	rm -rf $(PCCBUILD) build/native

.PHONY: all stage0 check clean
