#!/bin/sh
# A call of a C function that takes a variable number of arguments is made with the variadic call type only where the function is declared with `...`.  An undeclared function is called with a fixed number of integer arguments, the wrong convention for a variadic callee (the registers it spills are the caller's frame on 64 bit PowerPC, the arguments go on the stack on Apple arm64, the count of vector registers is set on x86-64).  This test reads every IMP source of the tree and fails for a CCALL of one of the functions in VARIADIC that has no declaration with `...` before it in the same file and compilation unit (a unit ends at a line holding %%).
# Run from the top of the tree.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
echo "== variadic"
# the C library's functions whose prototype ends with ... (glibc, musl, the BSDs, Darwin)
VARIADIC="printf fprintf sprintf snprintf dprintf asprintf sscanf fscanf scanf open open64 openat openat64 fcntl fcntl64 ioctl syscall execl execle execlp prctl ptrace mq_open sem_open semctl shmctl msgctl syslog err errx warn warnx error printw wprintf swprintf fwprintf wscanf swscanf fwscanf strfmon"
# (tests/cimport/basic.use.imp and qsort.imp have the declarations of printf from the output of imp72-cimport put before them by tests/cimport/run.sh)
files=$(find lib src tools tests -type f \( -name '*.imp' -o -name '*.imc' -o -name '*.sh' \) ! -name basic.use.imp ! -path tests/cimport/qsort.imp | sort)
fail=0
for f in $files; do
    awk -v names="$VARIADIC" -v file="$f" '
    BEGIN { n = split(names, a, " "); for (i = 1; i <= n; i++) v[a[i]] = 1 }
    $0 == "%%" { delete decl; next }
    {
        line = $0
        while (match(line, /EXTERNAL +[A-Z]+ +[A-Za-z_0-9]+ *\([^)]*\.\.\. *\)/)) {
            rs = RSTART; rl = RLENGTH
            s = substr(line, rs, rl)
            sub(/^EXTERNAL +[A-Z]+ +/, "", s); sub(/ *\(.*/, "", s)
            check(substr(line, 1, rs - 1))
            decl[s] = 1
            line = substr(line, rs + rl)
        }
        check(line)
    }
    function check(t,   s) {
        while (match(t, /CCALL +[A-Za-z_0-9]+ *\(/)) {
            s = substr(t, RSTART, RLENGTH); sub(/^CCALL +/, "", s); sub(/ *\(.*/, "", s)
            if ((s in v) && !(s in decl)) {
                printf "FAILED: variadic: %s:%d: CCALL %s has no EXTERNAL declaration with ... before it\n", file, NR, s > "/dev/stderr"
                bad = 1
            }
            t = substr(t, RSTART + RLENGTH)
        }
    }
    END { exit bad }' "$f" || fail=1
done
[ $fail = 0 ] && echo ok
exit $fail
