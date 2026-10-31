# limits.sh: source it (". tools/limits.sh") at the top of a script that runs compilers or tests.  What it sets applies to the script and the programs under it, and to nothing else:
#   file size   2 GB for a file one process writes (ulimit -f counts blocks of 1024 bytes in sh, 512 in some shells: it is set in the unit the shell says)
#   CPU time    40 minutes for one process
# A process that passes one of them is killed (SIGXFSZ, SIGXCPU) and the script goes on to say why it failed.  tools/guard.sh watches the size of the directories; a log that may get
# long goes through tools/capped.sh.  IMP72_NOLIMITS=1 leaves it all out.
if [ -z "$IMP72_NOLIMITS" ]; then
    # (best effort: a system that refuses a limit goes without it, and a script under set -e must not stop for that)
    ulimit -f 2097152 2>/dev/null || ulimit -f 4194304 2>/dev/null || true
    ulimit -t 2400 2>/dev/null || true
fi
