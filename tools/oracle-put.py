#!/usr/bin/env python3
"""oracle-put.py FILE[=NAME]...: copy files of 36-bit words (the archive's 5-byte form, as in
decus/*.shr) into [100,100] on the running oracle, through the tape drive.

NAME is the TOPS-10 name (default: the file's name in capitals).  Text files are put with
oracle-text.py instead.  Needs `tools/oracle-boot.sh` to have run."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle, pdp10tap

here = os.path.dirname(os.path.abspath(__file__))
tape = os.path.join(here, "..", "oracle", "tapes", "in.tap")


def main(args):
    files = []
    for a in args:
        host, _, name = a.partition("=")
        files.append((host, (name or os.path.basename(host)).upper()))
    pdp10tap.put(tape, [h for h, _ in files])
    t = oracle.Session(timeout=60)
    try:
        t.cmd("ASSIGN MTA0: TAPE")
        t.cmd("REWIND TAPE:")
        t.cmd("R PIP", r"^\*", 30)
        for host, name in files:
            t.send("DSK:%s=TAPE:/B\r" % name)
            out = t.expect(r"^\*", 120)
            if "?" in out:
                sys.exit("%s: %s" % (name, out.strip()))
            print(name, "ok")
    finally:
        t.close()


if __name__ == "__main__":
    main(sys.argv[1:])
