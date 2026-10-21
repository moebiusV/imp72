#!/bin/sh
# The manual in docs/manual/imp72.md is tools/runoff2md.py applied to decus/imp.rno: regenerate and compare.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
command -v python3 >/dev/null 2>&1 || { echo "manual check skipped: no python3"; exit 0; }
python3 tools/runoff2md.py decus/imp.rno > build/imp72.md.new 2>/dev/null || { mkdir -p build; python3 tools/runoff2md.py decus/imp.rno > build/imp72.md.new; }
cmp -s build/imp72.md.new docs/manual/imp72.md || { diff build/imp72.md.new docs/manual/imp72.md | head -5; echo "FAILED: docs/manual/imp72.md is not what tools/runoff2md.py makes" >&2; exit 1; }
echo ok
