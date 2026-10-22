#!/bin/sh
# Random programs of real arithmetic against an independent IEEE 754 binary64 reference (tools/imp-fuzz-ieee.py).
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
command -v python3 >/dev/null 2>&1 || { echo "ieee fuzz skipped: no python3"; exit 0; }
CC1=${CC1:-$root/build/pcc/cc/ccom/cc1}; export CC1
tools/ieee-fuzz.sh 1 8
