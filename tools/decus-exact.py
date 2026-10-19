#!/usr/bin/env python3
"""decus-exact.py TAPE OUT_DIR: exact 36-bit files of the DECUS LIB 3 tape for decus/'s binaries.

The archive's extracted files (decus/imp.shr, ...) keep 35 bits of each word; the tape image
(https://pdp-10.trailing-edge.com/tapes/decuslib10-03.tap.bz2, SIMH .tap, core-dump frames) has
all 36.  The extracted file says what the words are except for the last bit, so the words are
found in the tape and their last bit is read from there.  OUT_DIR gets NAME.words (5 frames to the
word, the form `tools/pdp10tap.py` writes to tape) for imp.low, imp.shr and implib.lib.
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pdp10tap as T


def tape_words(path):
    d = open(path, "rb").read()
    i, words = 0, []
    while i + 4 <= len(d):
        n = struct.unpack("<I", d[i:i + 4])[0]
        i += 4
        if n == 0:
            continue
        rec = d[i:i + n]
        words += T.words_from_frames(rec)
        i += n + (n & 1) + 4
    return words


def exact(lossy, W, name):
    index = {}
    for k in range(len(W) - 3):
        index.setdefault((W[k] >> 1, W[k + 1] >> 1, W[k + 2] >> 1, W[k + 3] >> 1), []).append(k)
    key = tuple(x >> 1 for x in lossy[:4])
    starts = index.get(key)
    if not starts:
        sys.exit("%s: its first words are not on the tape" % name)
    pos, out, i = starts[0], [], 0
    while i < len(lossy):
        if pos < len(W) and (W[pos] >> 1) == (lossy[i] >> 1):
            out.append(W[pos])
            pos += 1
            i += 1
            continue
        # a block boundary: the next words of the file are further on (a header between)
        want = tuple(x >> 1 for x in lossy[i:i + 4])
        nxt = [k for k in index.get(want, []) if k >= pos and k - pos < 200]
        if not nxt:
            sys.exit("%s: lost the file at word %d" % (name, i))
        pos = nxt[0]
    return out


def main(tape, outdir):
    W = tape_words(tape)
    here = os.path.dirname(os.path.abspath(__file__))
    os.makedirs(outdir, exist_ok=True)
    for name in ("imp.low", "imp.shr", "implib.lib"):
        lossy = T.words_from_archive(open(os.path.join(here, "..", "decus", name), "rb").read())
        words = exact(lossy, W, name)
        odd = sum(w & 1 for w in words)
        open(os.path.join(outdir, name + ".words"), "wb").write(T.frames(words))
        print("%s: %d words, %d with the last bit set" % (name, len(words), odd))


if __name__ == "__main__":
    main(*sys.argv[1:3])
