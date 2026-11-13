#!/bin/sh
# sysconst.sh [OS ARCH [BINDIR]]: the IMP module SYSGEN on the standard output: what the runtime library (lib/impsys.iml, lib/impvars.iml, lib/imphost.iml) has to know of the system that C
# says with its headers: the flags of mmap, the layout of a directory entry, the names the C library gives its standard files, the size of a pointer, the system and the architecture.
# OS is linux, freebsd, netbsd, openbsd, darwin or cygwin, ARCH x86_64, i386, aarch64, arm, ppc64le, s390x, riscv64, loongarch64 or mips64el (SYSOS and SYSARCH say the same); without them they are
# those of this machine.  BINDIR is where lld is as a program if it is not in the PATH (tools/llvm.sh bindir).  The values are those of the headers of each system; where this machine is the
# system and has a C compiler, the one that is not the same on every machine (the size of a FILE, where the C library has its standard files in an array) is asked of the compiler.
os=${1:-${SYSOS:-$(uname -s | tr 'A-Z' 'a-z')}}
arch=${2:-$SYSARCH}
bindir=$3
if [ -z "$arch" ]; then            # this machine's: what its C compiler makes programs for, not the kernel (a 32 bit container on a 64 bit kernel says x86_64 to uname), and a 32 bit userland says so
    arch=$(${CC:-cc} -dumpmachine 2>/dev/null | sed 's/-.*//')
    [ -n "$arch" ] || arch=$(uname -m)
    if [ "$(getconf LONG_BIT 2>/dev/null)" = 32 ]; then
        case $arch in x86_64|amd64) arch=i386 ;; aarch64|arm64) arch=arm ;; esac
    fi
fi
case $arch in
amd64|x86_64) arch=x86_64 ;;
powerpc64le) arch=ppc64le ;;
i?86) arch=i386 ;;
arm64|aarch64) arch=aarch64 ;;
arm*) arch=arm ;;
ppc64le|powerpc64le) arch=ppc64le ;;
mips64*) arch=mips64el ;;
esac
case $arch in i386|arm) bits=32 ;; *) bits=64 ;; esac
[ -n "$SYSBITS" ] && bits=$SYSBITS                      # (the word size the compiler found in the object it makes for the target: tools/mkrt.sh)
die() { echo "sysconst: $*" >&2; exit 1; }

# mmap: the anonymous mapping, no reservation of swap, and an exact address (a mapping that fails where the address is taken); PROT0 is the protection the arena is made with
anon=; nores=0; exact=0; prot0=0; rtld=0; dname=; dl=1
case $os in
linux)
    anon=32; nores=16384; exact=1048576            # MAP_ANONYMOUS, MAP_NORESERVE, MAP_FIXED_NOREPLACE
    case $arch in
    mips64el) anon=2048; nores=1024 ;;
    ppc64le) nores=64 ;;
    esac
    dname=19 ;;
freebsd) anon=4096; exact=16400; rtld=-2; dname=24 ;;        # MAP_ANON; MAP_FIXED | MAP_EXCL
openbsd) anon=4096; rtld=-2; dname=24 ;;
netbsd) anon=4096; prot0=24; rtld=-2; dname=13 ;;            # PROT_MPROTECT(PROT_READ | PROT_WRITE) = 3 << 3, which PaX wants of a mapping that is made usable later
darwin) anon=4096; nores=64; rtld=-2; dname=21; dl=0 ;;
# (Cygwin's values are from memory of its headers and are not checked on a Cygwin machine: MAP_ANONYMOUS 0x20, MAP_NORESERVE 0x4000, no MAP_FIXED_NOREPLACE; d_name at 16 of a struct dirent
# (d_ino, __d_version, __d_reserved); no dl_iterate_phdr; RTLD_DEFAULT 0)
cygwin) anon=32; nores=16384; rtld=0; dname=16; dl=0 ;;
# (Windows, mingw-w64 with the C library msvcrt: no mmap, so the flags are 0 and the arena is VirtualAlloc's (lib/impcore-win.iml); struct dirent of mingw-w64 is d_ino (a long), d_reclen and d_namlen
# (two shorts) and then d_name, at 12; no dl_iterate_phdr and no dladdr; the standard files are the elements of the array that __iob_func() returns)
windows) anon=0; rtld=0; dname=12; dl=0 ;;
*) die "no system $os" ;;
esac
case $bits in 64) hint=17592186044416; resv=17179869184 ;; *) hint=536870912; resv=268435456 ;; esac

# the standard files: how the C library has them.  var: variables stdout and stdin that hold the pointers (glibc, musl, FreeBSD's __stdoutp, macOS); addr: data of their own, whose address is the FILE
# (OpenBSD from 7.5 or so: stdout is ((struct __sFILE *)__stdout)); arr: an array of FILEs, __sF, whose elements are 0 stdin, 1 stdout (NetBSD, and OpenBSD before).  Where this machine is the
# system its <stdio.h> says which; for another system the newest scheme is assumed.
stdsym=stdout; stdin=stdin; filesz=; stdkind=var
here=no; [ "$os" = "$(uname -s | tr 'A-Z' 'a-z')" ] && here=yes
case $os in
freebsd|darwin) stdsym=__stdoutp; stdin=__stdinp ;;
windows) stdkind=iobf; [ "$SYSCRT" = ucrt ] && stdkind=acrt ;;           # msvcrt: stdin is __iob_func()[0], stdout __iob_func()[1], FILEs of 32 bytes on a 32 bit machine and 48 on a 64 bit one
cygwin) stdkind=reent ;;           # newlib: stdout is (_REENT->_stdout), _REENT being _impure_ptr; struct _reent is an int _errno and then the pointers _stdin, _stdout, _stderr (not checked on Cygwin)
openbsd)
    stdkind=addr; stdsym=__stdout; stdin=__stdin
    if [ $here = yes ] && [ -f /usr/include/stdio.h ] && ! grep -q '__stdout' /usr/include/stdio.h; then stdkind=arr; stdsym=__sF; stdin=__sF; fi ;;
netbsd) stdkind=arr; stdsym=__sF; stdin=__sF ;;
esac
if [ $stdkind = arr ]; then
    if [ $here = yes ] && command -v ${CC:-cc} >/dev/null 2>&1; then
        # (sizeof(FILE), asked of the C compiler of this machine: the FILEs are not the same size on every machine)
        . "$(dirname "$0")/mktmpdir.sh"; mktmpdir sysconst
        printf '#include <stdio.h>\nint main(void) { printf("%%ld\\n", (long)((char *)stdout - (char *)stdin)); return 0; }\n' > "$t/p.c"
        ${CC:-cc} -o "$t/p" "$t/p.c" 2>/dev/null && filesz=$("$t/p")
    fi
    if [ -z "$filesz" ] && [ -n "$MKRT_SYSROOT" ] && [ -f "$MKRT_SYSROOT/usr/include/stdio.h" ] && command -v ${CC:-cc} >/dev/null 2>&1; then
        # (a target's sysroot: its stdio.h read by this machine's C compiler, which gives the size of the FILE when the two have the same word size, as an amd64 host has for an amd64 target; it is tried, size by size, as a static assertion)
        . "$(dirname "$0")/mktmpdir.sh"; mktmpdir sysconst
        n=24
        while [ $n -le 512 ]; do
            printf '#include <stdio.h>\ntypedef char chk[(sizeof(FILE) == %d) ? 1 : -1];\n' $n > "$t/q.c"
            ${CC:-cc} -fsyntax-only -nostdinc -isystem "$MKRT_SYSROOT/usr/include" -D_NETBSD_SOURCE "$t/q.c" >/dev/null 2>&1 && { filesz=$n; break; }
            n=$((n+8))
        done
    fi
    [ -n "$filesz" ] || die "do not know the size of a FILE on $os $arch: build the library on that system"
fi

cat <<EOF
#THIS IS FILE SYSGEN, GENERATED BY TOOLS/SYSCONST.SH FOR $os $arch: WHAT THE HEADERS OF THE SYSTEM SAY AND IMP CANNOT (THE FLAGS OF MMAP, THE LAYOUT OF A DIRECTORY ENTRY, THE STANDARD FILES, THE SIZE OF A POINTER) #
EOF
if [ $stdkind = arr ]; then
    echo "!__sF! IS EXTERNAL;"
elif [ $stdkind = reent ]; then
    echo "!_impure_ptr! IS EXTERNAL;"
elif [ $stdkind = iobf ]; then
    echo "# (THE STANDARD FILES ARE __IOB_FUNC()'S) #"
elif [ $stdkind = acrt ]; then
    echo "# (THE STANDARD FILES ARE __ACRT_IOB_FUNC()'S) #"
else
    echo "!$stdsym!,!$stdin! ARE EXTERNAL;"
fi
echo "SCZ IS 1 LONG;                                          # AN EMPTY C STRING #"
echo
# the byte order of this machine or of the target (tools/endian.sh says which file of names the compiler reads): the numbers of the files the library reads are in it
case $(sh "$(dirname "$0")/endian.sh" 2>/dev/null) in *-be.imc) be=1 ;; *) be=0 ;; esac
[ -n "$SYSBE" ] && be=$SYSBE                               # (the target's, when tools/mkrt.sh found it in the object it makes for the target)
echo "SUBR SCBE(Z) IS $be;                                   # 1 ON A BIG ENDIAN MACHINE #"
echo "SUBR SCPTR(Z) IS $((bits / 8));                         # SIZEOF A POINTER #"
if [ $bits = 32 ]; then echo "SUBR SCMASK(Z) IS 4294967295;                 # THE BITS OF A POINTER #"; else echo "SUBR SCMASK(Z) IS 0-1;"; fi
echo "SUBR SCANON(Z) IS $anon;                             # MAP_ANON #"
echo "SUBR SCNORES(Z) IS $nores;                           # MAP_NORESERVE (0 WHERE THE SYSTEM HAS NONE) #"
echo "SUBR SCEXACT(Z) IS $exact;                          # THE FLAGS THAT MAKE THE ADDRESS EXACT (0: A HINT ONLY) #"
echo "SUBR SCPROT0(Z) IS $prot0;                             # THE PROTECTION THE ARENA IS RESERVED WITH #"
echo "SUBR SCHINT(Z) IS $hint;                    # WHERE THE ARENA IS #"
echo "SUBR SCRESV(Z) IS $resv;                    # HOW MUCH OF IT IS RESERVED, AT MOST #"
echo "SUBR SCRTLD(Z) IS $rtld;                              # RTLD_DEFAULT #"
echo "SUBR SCDNAME(Z) IS $dname;                            # OFFSET OF D_NAME IN A STRUCT DIRENT #"
echo "SUBR SCDL(Z) IS $dl;                                 # DL_ITERATE_PHDR AND DLADDR (NOT ON MACOS) #"
echo "SUBR IMPHOSTOS(Z) IS LOC('$os');"
if [ $os = windows ]; then echo "SUBR IMPBINMODE(Z) IS (CCALL _setmode(0,32768); CCALL _setmode(1,32768); CCALL _setmode(2,32768); 0);       # O_BINARY: THE STANDARD FILES ARE BYTES, NOT TEXT WITH CR LF #"; else echo "SUBR IMPBINMODE(Z) IS 0;"; fi
# (the directory and time calls of the C library: NetBSD's plain names are the compatibility entry points of the old struct dirent and the 32 bit time_t, <dirent.h> and <time.h> rename them there; the library calls these)
case $os in
netbsd) od=__opendir30 rd=__readdir30 tm=__time50 lt=__locatime50 ;;
*) od=opendir rd=readdir tm=time lt=localtime ;;
esac
echo "SUBR IMPOPENDIR(P) IS (CCALL $od(P));"
echo "SUBR IMPREADDIR(D) IS (CCALL $rd(D));"
echo "SUBR IMPTIME(P) IS (CCALL $tm(P));"
echo "SUBR IMPLOCALTIME(P) IS (CCALL $lt(P));"
# (the system call by its number: only on Linux, where memfd_create is made; elsewhere the library does not name syscall, which OpenBSD's C library lacks)
# (syscall is declared as the C function it is, with ... : a call of a function with a variable argument list that has no prototype is made as a call of one with a fixed list, and on the 64 bit PowerPC the callee then writes its register arguments into the caller's frame, where this routine's saved return address is)
if [ $os = linux ]; then echo "EXTERNAL LONG syscall(LONG, ...);"; echo "SUBR TDSYSCALL(N,A,B) IS (CCALL syscall(N,A,B));"; else echo "SUBR TDSYSCALL(N,A,B) IS 0-1;"; fi
if [ $arch = mips64el ]; then echo "SUBR IMPHOSTARCH(Z) IS LOC(SCZ);"; else echo "SUBR IMPHOSTARCH(Z) IS LOC('$arch');"; fi
if [ -n "$bindir" ]; then echo "SUBR SCBINDIR(Z) IS LOC('$bindir');"; else echo "SUBR SCBINDIR(Z) IS 0;"; fi
case $stdkind in
arr)
    echo "SUBR IMPSTDIN(Z) IS LOC(!__sF!);"
    echo "SUBR IMPSTDOUT(Z) IS (Q ARE LOCAL; Q:=LOC(!__sF!); Q+$filesz)        # (IN A VARIABLE: THE SEED TAKES LOC(X)+N FOR N WORDS) #" ;;
iobf)
    if [ $bits = 32 ]; then filesz=32; else filesz=48; fi
    echo "SUBR IMPSTDIN(Z) IS (CCALL __iob_func(0));                    # (MSVCRT'S ARRAY OF THE STANDARD FILES: STDIN IS ITS FIRST ELEMENT) #"
    echo "SUBR IMPSTDOUT(Z) IS (Q ARE LOCAL; Q:=CCALL __iob_func(0); Q+$filesz)" ;;
acrt)
    echo "SUBR IMPSTDIN(Z) IS (CCALL __acrt_iob_func(0));                # (THE UCRT: A FUNCTION GIVES EACH OF THE STANDARD FILES; THE ARRAY OF MSVCRT IS GONE) #"
    echo "SUBR IMPSTDOUT(Z) IS (CCALL __acrt_iob_func(1))" ;;
reent)
    echo "SUBR IMPSTDIN(Z) IS IMPPEEKPTR(IMPPEEKPTR(LOC(!_impure_ptr!))+SCPTR(0));          # (NEWLIB: _REENT->_STDIN; THE POINTERS FOLLOW THE INT _ERRNO, ALIGNED TO A POINTER) #"
    echo "SUBR IMPSTDOUT(Z) IS IMPPEEKPTR(IMPPEEKPTR(LOC(!_impure_ptr!))+(2*SCPTR(0)))" ;;
addr)
    echo "SUBR IMPSTDIN(Z) IS LOC(!$stdin!);                    # (THE SYMBOL IS THE FILE) #"
    echo "SUBR IMPSTDOUT(Z) IS LOC(!$stdsym!)" ;;
*)
    echo "SUBR IMPSTDIN(Z) IS IMPPEEKPTR(LOC(!$stdin!));          # (THE VARIABLE HOLDS A POINTER, WHICH IS THE LOW HALF OF A WORD ON A LITTLE ENDIAN MACHINE ONLY: IT IS READ AS THE POINTER IT IS) #"
    echo "SUBR IMPSTDOUT(Z) IS IMPPEEKPTR(LOC(!$stdsym!))" ;;
esac
echo "%%"
