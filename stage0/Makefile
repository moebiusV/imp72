# Stage 0 build.  Plain make (GNU or BSD); the compiler is $(CC).
CC      = cc
CFLAGS  = -std=c99 -O1 -g -Wall -Wextra -pedantic
AR      = ar
OBJS    = error.o free.o stack.o dir.o lex.o
TESTDIR = ../tests/stage0
TESTS   = test_free test_stack test_dir test_lex

all: libstage0.a

libstage0.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

$(OBJS): imp.h

.c.o:
	$(CC) $(CFLAGS) -c $<

check: $(TESTS)
	@for t in $(TESTS); do echo "== $$t"; ./$$t || exit 1; done

test_free: $(TESTDIR)/test_free.c libstage0.a
	$(CC) $(CFLAGS) -I. -o $@ $(TESTDIR)/test_free.c libstage0.a
test_stack: $(TESTDIR)/test_stack.c libstage0.a
	$(CC) $(CFLAGS) -I. -o $@ $(TESTDIR)/test_stack.c libstage0.a
test_dir: $(TESTDIR)/test_dir.c libstage0.a
	$(CC) $(CFLAGS) -I. -o $@ $(TESTDIR)/test_dir.c libstage0.a
test_lex: $(TESTDIR)/test_lex.c libstage0.a
	$(CC) $(CFLAGS) -I. -o $@ $(TESTDIR)/test_lex.c libstage0.a

clean:
	rm -f $(OBJS) libstage0.a $(TESTS)
