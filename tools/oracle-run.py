#!/usr/bin/env python3
"""oracle-run.py [-d DIR] PROG.imp...: compile, link and run IMP programs with the original
compiler (IMP 1.5 on TOPS-10 6.03, the SIMH KA10; start it with tools/oracle-boot.sh).
With --lst only compile, with /A/L, and keep the assembly listing (DIR/NAME.lst).

For each program the output the program put into its default output file (PO) is written to
DIR/NAME.oracle (default: standard output, after a header line), tabs expanded to the 8 column
stops of the terminal.  The compiler's messages go to DIR/NAME.compile."""
import os, re, sys, time
here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)
import oracle, pdp10tap


def quiet(t, idle=4, limit=600, until=None):
    out, last, end = "", time.time(), time.time() + limit
    while time.time() < end:
        d = t._read(1.0)
        if d:
            out, last = out + d, time.time()
            if until and re.search(until, out, re.M):
                break
        elif time.time() - last > idle:
            break
    return out


def run(progs, outdir, lst=False):
    tape = os.path.join(here, "..", "oracle", "tapes", "in.tap")
    names = [os.path.splitext(os.path.basename(p))[0].upper()[:6] for p in progs]
    t = oracle.Session(timeout=120)
    try:
        t.cmd("TTY WIDTH 200")           # the terminal folds lines at 72 columns: not in our output
        for p, n in zip(progs, names):      # one tape per program: several files on one tape lose the last
            pdp10tap.put(tape, [p])
            t.cmd("ASSIGN MTA0: TAPE")
            t.cmd("REWIND TAPE:")
            t.cmd("R PIP", r"^\*", 30)
            t.send("DSK:%s.IMP=TAPE:/B\r" % n)
            t.expect(r"^\*", 120)
            t.raw(b"\x03")
            t.expect(r"^\.", 20)
            t.cmd("DEASSIGN TAPE:")
        for p, n in zip(progs, names):
            t.cmd("DELETE PO.,%s.REL" % n)
            t.send("RUN IMP\r")
            t.expect(r"^\*", 60)
            t.send("%s.IMP%s\r" % (n, (os.environ.get("ORACLE_SW", "/A") + "/L") if lst else ""))
            comp = t.expect(r"^\.", 600)
            if lst:                         # the assembly listing only: no link, no run
                t.send("TYPE %s.LST\r" % n)
                text = t.expect(r"^\.", 600).split("TYPE %s.LST\n" % n, 1)[-1].rsplit("\n.", 1)[0]
                text = "\n".join(l.expandtabs(8).rstrip() for l in text.split("\n")).strip("\n") + "\n"
                base = os.path.splitext(os.path.basename(p))[0]
                if outdir:
                    open(os.path.join(outdir, base + ".lst"), "w").write(text)
                    open(os.path.join(outdir, base + ".compile"), "w").write(comp)
                else:
                    print(text)
                t.cmd("DELETE %s.LST,%s.REL" % (n, n))
                continue
            t.send("EX %s.REL,/LIBRARY,IMPLIB.LIB,SYS:FORLIB/LIBRARY\r" % n)
            try:
                ex = t.expect(r"^\.", 120)
            except TimeoutError as e:           # a program that does not stop: ^C it
                t.raw(b"\x03\x03")
                time.sleep(2)
                ex = "%s\n[TIMEOUT]\n" % e
                t._read(3.0)
            tty = ex.split("Execution]", 1)[-1].rsplit("\n.", 1)[0].strip("\n")
            time.sleep(1)
            t.send("TYPE PO.\r")
            text = t.expect(r"^\.", 120)
            text = text.split("TYPE PO.\n", 1)[-1].rsplit("\n.", 1)[0]
            text = "\n".join(l.expandtabs(8).rstrip() for l in text.split("\n")).strip("\n") + "\n"
            base = os.path.splitext(os.path.basename(p))[0]
            if outdir:
                open(os.path.join(outdir, base + ".oracle"), "w").write(text)
                open(os.path.join(outdir, base + ".compile"), "w").write(comp + ex)
                open(os.path.join(outdir, base + ".tty"), "w").write(tty + "\n" if tty else "")
            else:
                print("== %s" % n)
                print(text, end="")
    finally:
        t.close()


if __name__ == "__main__":
    args = sys.argv[1:]
    outdir = None
    lst = False
    while args[:1] and args[0].startswith("-"):
        if args[0] == "-d":
            outdir = args[1]
            os.makedirs(outdir, exist_ok=True)
            args = args[2:]
        elif args[0] == "--lst":
            lst = True
            args = args[1:]
        else:
            sys.exit("usage: oracle-run.py [-d DIR] [--lst] PROG.imp...")
    run(args, outdir, lst)
