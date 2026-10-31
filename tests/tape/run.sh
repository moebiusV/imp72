#!/bin/sh
# tape/ is what tools/tape.sh makes of decus/ (assignment is := and not _), and decus/ is the tape as received.  Run from the top of the tree (make check does).
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
echo "== tape"
sh tools/tape.sh check || exit 1
if command -v sha256sum >/dev/null 2>&1; then
    (cd decus && sha256sum -c SHA256SUMS >/dev/null 2>&1) || { echo "FAILED: tape: decus/ is not the tape as received (SHA256SUMS)" >&2; exit 1; }
fi
