#!/usr/bin/env python3
"""pdp10tap.py: files of 36-bit words <-> SIMH .tap tapes for the oracle.

    pdp10tap.py put OUT.tap FILE...    one tape file (record + tape mark) per FILE
    pdp10tap.py get IN.tap  DIR        every tape file to DIR/fileN.bin in the 5-byte form

The words of the archive's binaries (decus/*.shr, *.low, *.lib) are stored 5 bytes to
a word as ASCII packs them: 7 bits of each byte in bits 35..1, bit 0 from the top bit
of the last byte.  On tape (the SIMH KA10 drive in core-dump mode) a word is 5 frames:
bits 35-28, 27-20, 19-12, 11-4, then bits 3-0 in the low nibble.
"""
import os, struct, sys

TAPE_SIZE = 8 << 20

M36 = (1 << 36) - 1


def words_from_archive(data):
    out = []
    for i in range(0, len(data) - len(data) % 5, 5):
        b = data[i:i + 5]
        w = 0
        for k in range(5):
            w |= (b[k] & 127) << (29 - 7 * k)
        w |= b[4] >> 7
        out.append(w & M36)
    return out


def archive_from_words(words):
    out = bytearray()
    for w in words:
        for k in range(5):
            out.append((w >> (29 - 7 * k)) & 127)
        out[-1] |= (w & 1) << 7
    return bytes(out)


def frames(words):
    out = bytearray()
    for w in words:
        out += bytes([(w >> 28) & 255, (w >> 20) & 255, (w >> 12) & 255, (w >> 4) & 255, w & 15])
    return bytes(out)


def words_from_frames(data):
    return [(data[i] << 28) | (data[i + 1] << 20) | (data[i + 2] << 12) | (data[i + 3] << 4) | (data[i + 4] & 15)
            for i in range(0, len(data) - len(data) % 5, 5)]


def record(data):
    pad = b"\0" if len(data) & 1 else b""
    n = struct.pack("<I", len(data))
    return n + data + pad + n


def words_from_text(data):
    """a text file as TOPS-10 holds it: CR LF line ends, five 7-bit characters to a word
    (bit 0 clear), padded with NULs to a whole block"""
    text = data.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")
    text += b"\0" * (-len(text) % 5)
    out = []
    for i in range(0, len(text), 5):
        w = 0
        for k in range(5):
            w |= (text[i + k] & 127) << (29 - 7 * k)
        out.append(w)
    return out


def put(out, files, block=128):
    """each file as records of `block` words (the tape's usual blocking), then a tape mark"""
    tape = bytearray()
    for f in files:
        raw = open(f, "rb").read()
        if f.endswith(".words"):                   # exact: 5 frames to the word
            words = words_from_frames(raw)
        else:
            words = words_from_text(raw) if f.endswith((".imp", ".imc", ".txt", ".mac", ".cmd")) \
                else words_from_archive(raw)
        words += [0] * (-len(words) % block)       # the drive reads whole blocks
        for i in range(0, len(words), block):
            tape += record(frames(words[i:i + block]))
        tape += struct.pack("<I", 0)
    tape += struct.pack("<I", 0)
    # The simulator takes the size of the tape from the file when it is attached, so a
    # tape that is rewritten later (the same file, new contents) must have been that
    # long from the start: pad with tape marks.
    tape += b"\0" * (TAPE_SIZE - len(tape))
    with open(out, "r+b" if os.path.exists(out) else "wb") as f:
        f.seek(0)
        f.write(tape)


if __name__ == "__main__":
    if sys.argv[1] == "put":
        put(sys.argv[2], sys.argv[3:])      # no FILEs: an empty tape
    else:
        sys.exit("usage: see the file header")
