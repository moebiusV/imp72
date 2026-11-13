#!/bin/sh
# msys2-fetch.sh DIR NAME...: unpack the packages mingw-w64-ucrt-x86_64-NAME of MSYS2's ucrt64 repository (https://repo.msys2.org/mingw/ucrt64) into DIR/ucrt64, each checked against the
# SHA-256 of the repository database; the downloads are kept in DIR/dl.  Used by tools/win-seed.sh for the headers, crt, gcc (libstdc++), llvm, lld and their libraries that the seed for Windows
# is built against; nothing is installed.  Needs curl or wget, zstd, tar and sha256sum.
d=$1; shift
[ -n "$d" ] && [ $# -gt 0 ] || { echo "usage: msys2-fetch.sh DIR NAME..." >&2; exit 2; }
B=${IMP_MSYS2_MIRROR:-https://repo.msys2.org/mingw}/ucrt64
mkdir -p "$d/dl" "$d/db"
dl() { if command -v curl >/dev/null 2>&1; then curl -fsSL -m 900 -o "$2" "$1"; else wget -q -O "$2" "$1"; fi; }
if [ ! -s "$d/ucrt64.db" ] || [ -n "$(find "$d/ucrt64.db" -mtime +1 2>/dev/null)" ]; then
    dl "$B/ucrt64.db" "$d/ucrt64.db" || { echo "msys2-fetch: cannot fetch $B/ucrt64.db" >&2; exit 1; }
    rm -rf "$d/db"; mkdir -p "$d/db"
    zstd -dc "$d/ucrt64.db" | tar -xf - -C "$d/db" 2>/dev/null || tar -xzf "$d/ucrt64.db" -C "$d/db" || { echo "msys2-fetch: cannot read the database" >&2; exit 1; }
fi
for p in "$@"; do
    e=$(ls "$d/db" | grep -E "^mingw-w64-ucrt-x86_64-$p-[0-9r]" | head -1)
    [ -n "$e" ] || { echo "msys2-fetch: no package $p" >&2; exit 1; }
    fn=$(sed -n '/^%FILENAME%$/{n;p;}' "$d/db/$e/desc"); sum=$(sed -n '/^%SHA256SUM%$/{n;p;}' "$d/db/$e/desc")
    if [ ! -f "$d/dl/$fn" ]; then dl "$B/$fn" "$d/dl/$fn" || { echo "msys2-fetch: cannot fetch $fn" >&2; exit 1; }; fi
    [ "$(sha256sum "$d/dl/$fn" | cut -d' ' -f1)" = "$sum" ] || { echo "msys2-fetch: the checksum of $fn is not the database's" >&2; rm -f "$d/dl/$fn"; exit 1; }
    zstd -dc "$d/dl/$fn" | tar -xf - -C "$d" --exclude='*/share/doc' --exclude='*/share/man' --exclude='*/share/locale' --exclude='*/share/info' 2>/dev/null
    echo "msys2-fetch: $fn"
done
