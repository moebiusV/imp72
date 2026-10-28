#!/usr/bin/env python3
"""objnorm.py FILE.o ...: print an ELF relocatable object in a form that two compilations of the same source agree on.

What the compiler varies from run to run is removed or canonicalised, so that comparing the output of two objects is
apples to apples:
  - OpenBSD's RETGUARD: every function has a cookie `__retguard_N` with N random per compile, in its own section
    `.openbsd.randomdata.retguard.N` and COMDAT group.  The cookie symbols are all called `__retguard`, their sections
    and groups are left out, and a relocation against one names `__retguard`.
  - the build identification: .comment, .note.*, .GCC.command.line, .gnu_debuglink, and the debug sections' sizes and
    contents (paths and producer strings; -g objects carry the directory of the compile, and the sections that hold it vary in size).
  - the order of sections and symbols (sorted by name), and the section and symbol numbers.
Everything else - the bytes of every other section, the symbols with their binding, type, visibility, section and
value, and every relocation with its type, symbol and addend - is printed.  Handles 32 and 64 bit, either byte order,
REL and RELA."""
import binascii
import re
import struct
import sys

SKIP = re.compile(r"^(\.comment|\.note.*|\.GCC\.command\.line|\.gnu_debuglink|\.openbsd\.randomdata\.retguard\..*|"
                  r"__retguard_\d+|\.group|\.llvm_addrsig|\.llvm\.call-graph-profile)$")
DEBUG = re.compile(r"^\.(debug_|zdebug_)")
RETGUARD = re.compile(r"^__retguard_\d+$")


def norm(path):
    d = open(path, "rb").read()
    if d[:4] != b"\x7fELF":
        return "not ELF: " + path
    cls = d[4]
    e = "<" if d[5] == 1 else ">"
    if cls == 2:
        shoff, = struct.unpack_from(e + "Q", d, 0x28)
        shentsize, shnum, shstrndx = struct.unpack_from(e + "HHH", d, 0x3A)
    else:
        shoff, = struct.unpack_from(e + "I", d, 0x20)
        shentsize, shnum, shstrndx = struct.unpack_from(e + "HHH", d, 0x2E)
    secs = []
    for i in range(shnum):
        o = shoff + i * shentsize
        if cls == 2:
            name, typ, flags, addr, off, size, link, info, align, entsize = struct.unpack_from(e + "IIQQQQIIQQ", d, o)
        else:
            name, typ, flags, addr, off, size, link, info, align, entsize = struct.unpack_from(e + "IIIIIIIIII", d, o)
        secs.append(dict(i=i, name=name, type=typ, flags=flags, off=off, size=size, link=link, info=info, align=align,
                         entsize=entsize))

    def cstr(sec, o):
        s = d[sec["off"] + o:]
        return s[:s.index(b"\0")].decode("latin-1")

    for s in secs:
        s["n"] = cstr(secs[shstrndx], s["name"])
    syms = {}
    symtab = None
    for s in secs:
        if s["type"] == 2:
            symtab = s
            strsec = secs[s["link"]]
            ent = 24 if cls == 2 else 16
            for k in range(s["size"] // ent):
                if cls == 2:
                    n, info, other, shndx, val, sz = struct.unpack_from(e + "IBBHQQ", d, s["off"] + k * ent)
                else:
                    n, val, sz, info, other, shndx = struct.unpack_from(e + "IIIBBH", d, s["off"] + k * ent)
                syms[k] = (cstr(strsec, n), info >> 4, info & 15, other & 3, shndx, val, sz)

    def symname(k):
        n = syms[k][0] if k in syms else "?%d" % k
        if RETGUARD.match(n):
            return "__retguard"
        if not n and k in syms and syms[k][2] == 3:        # a section symbol: name the section
            sh = syms[k][4]
            return "." + secs[sh]["n"] if sh < len(secs) else ""
        return n

    def keep(s):
        return s["n"] != "" and not SKIP.match(s["n"])

    out = ["ELF class=%d data=%s" % (cls, "LE" if e == "<" else "BE")]
    for s in sorted([s for s in secs if keep(s) and s["type"] not in (2, 3, 4, 9)], key=lambda s: s["n"]):
        if DEBUG.match(s["n"]):
            out.append("section %s type=%d (size and contents not compared)" % (s["n"], s["type"]))
            continue
        body = b"" if s["type"] == 8 else d[s["off"]:s["off"] + s["size"]]
        out.append("section %s type=%d flags=0x%x align=%d size=%d %s" % (s["n"], s["type"], s["flags"], s["align"], s["size"],
                                                                      binascii.hexlify(body).decode()))
    rows = []
    for k, (n, bind, typ, vis, shndx, val, sz) in syms.items():
        if RETGUARD.match(n) or (typ == 3) or k == 0:
            continue
        where = secs[shndx]["n"] if 0 < shndx < len(secs) else {0: "UND", 0xfff1: "ABS", 0xfff2: "COMMON"}.get(shndx, str(shndx))
        if RETGUARD.match(where) or where.startswith(".openbsd.randomdata.retguard"):
            continue
        rows.append("symbol %s bind=%d type=%d vis=%d in=%s value=%d size=%d" % (n, bind, typ, vis, where, val, sz))
    out.extend(sorted(rows))
    rrows = []
    for s in secs:
        if s["type"] in (4, 9):
            target = secs[s["info"]]["n"] if s["info"] < len(secs) else "?"
            if SKIP.match(target) or DEBUG.match(target):
                continue
            ent = (24 if cls == 2 else 12) if s["type"] == 4 else (16 if cls == 2 else 8)
            for k in range(s["size"] // ent):
                o = s["off"] + k * ent
                if s["type"] == 4:
                    if cls == 2:
                        off, info, add = struct.unpack_from(e + "QQq", d, o)
                        sym, typ = info >> 32, info & 0xffffffff
                    else:
                        off, info, add = struct.unpack_from(e + "IIi", d, o)
                        sym, typ = info >> 8, info & 0xff
                else:
                    add = None
                    if cls == 2:
                        off, info = struct.unpack_from(e + "QQ", d, o)
                        sym, typ = info >> 32, info & 0xffffffff
                    else:
                        off, info = struct.unpack_from(e + "II", d, o)
                        sym, typ = info >> 8, info & 0xff
                rrows.append("reloc %s +%d type=%d %s %s" % (target, off, typ, symname(sym), "" if add is None else "addend=%d" % add))
    out.extend(sorted(rrows))
    return "\n".join(out)


if __name__ == "__main__":
    for p in sys.argv[1:]:
        if len(sys.argv) > 2:
            print("== " + p)
        print(norm(p))
