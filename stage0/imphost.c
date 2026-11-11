/*
 * imphost.c -- what the link needs to know of the machine it runs on: the system (imphostos), the architecture (imphostarch), where this very program found its dynamic
 * linker (imphostinterp) and its C library (imphostlibc), the directory of the C library's crt files that goes with it (imphostcrtdir), and the newest version of a library that
 * is only there with its version (impfindlib).  The compiler runs on the system it makes programs for, so what its own loader did is what the programs it makes need.
 * src/driver.imp calls them with CCALL; the seed's driver, stage0/imps0.c, links this file in, and links the same way.
 */
#define _GNU_SOURCE
#include <dirent.h>
#ifndef __APPLE__
#include <dlfcn.h>
#include <link.h>
#endif
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the system the compiler runs on, for the link (src/driver.imp: LNK): linux, freebsd, netbsd, openbsd, darwin, else "" */
const char *
imphostos(long z)
{
    (void)z;
#if defined(__linux__)
    return "linux";
#elif defined(__FreeBSD__)
    return "freebsd";
#elif defined(__NetBSD__)
    return "netbsd";
#elif defined(__OpenBSD__)
    return "openbsd";
#elif defined(__APPLE__)
    return "darwin";
#else
    return "";
#endif
}

/* the architecture the compiler runs on, for the link (src/driver.imp: LNK): x86_64, i386, aarch64, arm, ppc64le, s390x, riscv64, loongarch64, else "" */
const char *
imphostarch(long z)
{
    (void)z;
#if defined(__x86_64__)
    return "x86_64";
#elif defined(__i386__)
    return "i386";
#elif defined(__aarch64__) || defined(__arm64__)
    return "aarch64";
#elif defined(__arm__)
    return "arm";
#elif defined(__powerpc64__) && defined(__LITTLE_ENDIAN__)
    return "ppc64le";
#elif defined(__s390x__)
    return "s390x";
#elif defined(__riscv) && __riscv_xlen == 64
    return "riscv64";
#elif defined(__loongarch64)
    return "loongarch64";
#else
    return "";
#endif
}

/* the newest version of a shared library in a directory: PREFIX followed by MAJOR.MINOR, as OpenBSD names its libraries (libc.so.97.1, with no libc.so for the linker to find
 * by -lc); the path goes to the buffer OUT, 4096 bytes; 1 if there is one.  The linker is not given -lc but this file */
long
impfindlib(long out, long dir, long prefix)
{
    DIR *d = opendir((const char *)(intptr_t)dir);
    struct dirent *e;
    const char *pre = (const char *)(intptr_t)prefix;
    size_t n = strlen(pre);
    long bmaj = -1, bmin = -1;
    char best[512] = "";

    if (d == NULL)
        return 0;
    while ((e = readdir(d)) != NULL) {
        char *q, *r;
        long maj, min = 0;

        if (strncmp(e->d_name, pre, n) != 0)
            continue;
        maj = strtol(e->d_name + n, &q, 10);
        if (q == e->d_name + n)
            continue;
        if (*q == '.') {
            min = strtol(q + 1, &r, 10);
            if (r == q + 1 || *r != '\0')
                continue;
        } else if (*q != '\0')
            continue;
        if (maj > bmaj || (maj == bmaj && min > bmin)) {
            bmaj = maj;
            bmin = min;
            snprintf(best, sizeof best, "%s", e->d_name);
        }
    }
    closedir(d);
    if (bmaj < 0)
        return 0;
    snprintf((char *)(intptr_t)out, 4096, "%s/%s", (const char *)(intptr_t)dir, best);
    return 1;
}

#ifndef __APPLE__
static int
interp_cb(struct dl_phdr_info *info, size_t size, void *data)
{
    int i;

    (void)size;
    for (i = 0; i < info->dlpi_phnum; i++)
        if (info->dlpi_phdr[i].p_type == PT_INTERP) {
            snprintf((char *)data, 4096, "%s", (const char *)(uintptr_t)(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr));
            break;
        }
    return 1;                                   /* the first object is the program itself */
}
#endif

/* the dynamic linker this program was started with (its PT_INTERP) into the buffer OUT, 4096 bytes; 1 if it has one (a static program, and macOS, have none) */
long
imphostinterp(long out)
{
    char *o = (char *)(intptr_t)out;

    *o = '\0';
#ifndef __APPLE__
    dl_iterate_phdr(interp_cb, o);
#endif
    return *o != '\0';
}

/* the file of the C library this program uses (the shared object that has getenv), into the buffer OUT; 1 if it is a file: libc.so.6, libc.so.97.1, ld-musl-x86_64.so.1 */
long
imphostlibc(long out)
{
    char *o = (char *)(intptr_t)out;

    *o = '\0';
#ifndef __APPLE__
    {
        Dl_info dl;

        if (dladdr((const void *)(uintptr_t)getenv, &dl) != 0 && dl.dli_fname != NULL && dl.dli_fname[0] == '/')
            snprintf(o, 4096, "%s", dl.dli_fname);
    }
#endif
    return *o != '\0';
}

static int
hascrt(const char *d)
{
    char p[4096];

    snprintf(p, sizeof p, "%s/crt1.o", d);
    if (access(p, R_OK) == 0)
        return 1;
    snprintf(p, sizeof p, "%s/crt0.o", d);
    return access(p, R_OK) == 0;
}

/* the directory of crt1.o (crt0.o) that goes with the C library in use: where the library is, or the same under /usr (/lib/x86_64-linux-gnu has the library, and
 * /usr/lib/x86_64-linux-gnu the crt files; /lib64 and /usr/lib64; /lib and /usr/lib), else /usr/lib; into the buffer OUT; 1 if one has them */
long
imphostcrtdir(long out)
{
    char *o = (char *)(intptr_t)out, libc[4096], d[4096], u[4100];
    char *slash;

    *o = '\0';
    if (!imphostlibc((long)(intptr_t)libc))
        return 0;
    if ((slash = strrchr(libc, '/')) == NULL)
        return 0;
    snprintf(d, sizeof d, "%.*s", (int)(slash - libc), libc);
    snprintf(u, sizeof u, "/usr%s", d);
    if (hascrt(d))
        snprintf(o, 4096, "%s", d);
    else if (hascrt(u))
        snprintf(o, 4096, "%s", u);
    else if (hascrt("/usr/lib"))
        snprintf(o, 4096, "/usr/lib");
    return *o != '\0';
}

/*
 * A program with no loader of its own (a static one) cannot say where its C library is: the files are looked for by what they are.  The start file crt1.o (crt0.o) of the machine is
 * the one whose ELF header (class, byte order, machine) is this program's own, read from /proc/self/exe, and the loader is the PT_INTERP of the C library beside it (libc.so.6 of
 * glibc, libc.so of musl, which are programs too).  No table of architectures: the machine is what the files say.  Returns 1 and the directory of the start files into CRT and
 * the loader into INTERP (buffers of 4096 bytes), else 0.  lib/imphost.imp (IMPHOSTPROBE) is the IMP twin.
 */
static int
elfident(const char *path, unsigned char *id)
{
    unsigned char h[20];
    FILE *f = fopen(path, "rb");
    size_t n;

    if (f == NULL)
        return 0;
    n = fread(h, 1, sizeof h, f);
    fclose(f);
    if (n != sizeof h || h[0] != 0x7f || h[1] != 'E' || h[2] != 'L' || h[3] != 'F')
        return 0;
    id[0] = h[4]; id[1] = h[5]; id[2] = h[18]; id[3] = h[19];
    return 1;
}

static int
elfinterp(const char *path, const unsigned char *id, char *out)
{
    unsigned char h[64], ph[56];
    FILE *f = fopen(path, "rb");
    unsigned long long off, ent, num, i, foff, fsz;
    int be = id[1] == 2, c64 = id[0] == 2, k;

#define RD(p, n, v) do { v = 0; for (k = 0; k < (n); k++) v = (v << 8) | (be ? (p)[k] : (p)[(n) - 1 - k]); } while (0)
    if (f == NULL)
        return 0;
    if (fread(h, 1, sizeof h, f) < (c64 ? 64u : 52u)) {
        fclose(f);
        return 0;
    }
    if (c64) { RD(h + 32, 8, off); RD(h + 54, 2, ent); RD(h + 56, 2, num); }
    else { RD(h + 28, 4, off); RD(h + 42, 2, ent); RD(h + 44, 2, num); }
    for (i = 0; i < num && ent <= sizeof ph; i++) {
        unsigned long long type;

        if (fseek(f, (long)(off + i * ent), SEEK_SET) != 0 || fread(ph, 1, ent, f) != ent)
            break;
        RD(ph, 4, type);
        if (type != 3)
            continue;
        if (c64) { RD(ph + 8, 8, foff); RD(ph + 32, 8, fsz); }
        else { RD(ph + 4, 4, foff); RD(ph + 16, 4, fsz); }
        if (fsz == 0 || fsz > 4000 || fseek(f, (long)foff, SEEK_SET) != 0 || fread(out, 1, fsz, f) != fsz) {
            *out = '\0';
            break;
        }
        out[fsz] = '\0';
        fclose(f);
        return out[0] == '/';
    }
#undef RD
    fclose(f);
    return 0;
}

static int
probecrt(const char *d, const unsigned char *self)
{
    char p[4096];
    unsigned char id[4];

    snprintf(p, sizeof p, "%s/crt1.o", d);
    return elfident(p, id) && memcmp(id, self, 4) == 0;
}

static int
probelibc(const char *d, const unsigned char *self, char *interp)
{
    static const char *const names[] = { "libc.so.6", "libc.so" };
    char p[4096];
    unsigned char id[4];
    int k;

    for (k = 0; k < 2; k++) {
        snprintf(p, sizeof p, "%s/%s", d, names[k]);
        if (elfident(p, id) && memcmp(id, self, 4) == 0 && elfinterp(p, self, interp))
            return 1;
    }
    return 0;
}

long
imphostprobe(long crt, long interp)
{
    static const char *const top[] = { "/usr/lib", "/usr/lib64", "/usr/lib32", "/lib", "/lib64", "/lib32" };
    char *co = (char *)(intptr_t)crt, *io = (char *)(intptr_t)interp, d[4096], u[4100];
    unsigned char self[4];
    size_t t;

    *co = *io = '\0';
    if (!elfident("/proc/self/exe", self))
        return 0;
    for (t = 0; t < sizeof top / sizeof top[0]; t++) {         /* the directory itself, then each directory in it (the multiarch ones: /usr/lib/s390x-linux-gnu) */
        DIR *dp;
        struct dirent *e;

        if (probecrt(top[t], self)) {
            snprintf(d, sizeof d, "%s", top[t]);
            goto found;
        }
        if ((dp = opendir(top[t])) == NULL)
            continue;
        while ((e = readdir(dp)) != NULL) {
            if (e->d_name[0] == '.')
                continue;
            snprintf(d, sizeof d, "%s/%s", top[t], e->d_name);
            if (probecrt(d, self)) {
                closedir(dp);
                goto found;
            }
        }
        closedir(dp);
    }
    return 0;
found:
    snprintf(co, 4096, "%s", d);
    if (probelibc(d, self, io))
        return 1;
    if (strncmp(d, "/usr/", 5) == 0) {                          /* the library may be where the system keeps it, and the start files under /usr */
        snprintf(u, sizeof u, "%s", d + 4);
        if (probelibc(u, self, io))
            return 1;
    }
    snprintf(u, sizeof u, "/lib");
    if (probelibc(u, self, io) || probelibc("/usr/lib", self, io))
        return 1;
    *co = '\0';
    return 0;
}

#ifdef IMP_LLVM_BINDIR
#define STR2(x) #x
#define STR(x) STR2(x)
#define LLVM_BINDIR STR(IMP_LLVM_BINDIR)
#endif

/* run lld as a program, found by the path under the name in argv[0] (ld.lld, ld64.lld), with the argument vector argv (NULL ended); wait for it; 0 if it linked.  For a compiler built
 * without the lld library, as OpenBSD's is (its own linker is lld, and it has no library of it) */
/* is the program NAME in a directory of the PATH, or in the bin directory of the LLVM this was built with */
static int
findprog(const char *name)
{
    const char *p = getenv("PATH");
    char cand[4096];

    while (p != NULL && *p) {
        const char *e = strchr(p, ':');
        size_t len = e ? (size_t)(e - p) : strlen(p);

        snprintf(cand, sizeof cand, "%.*s/%s", (int)(len ? len : 1), len ? p : ".", name);
        if (access(cand, X_OK) == 0)
            return 1;
        p = e ? e + 1 : NULL;
    }
#ifdef IMP_LLVM_BINDIR
    snprintf(cand, sizeof cand, "%s/%s", LLVM_BINDIR, name);
    if (access(cand, X_OK) == 0)
        return 1;
#endif
    return 0;
}

long
imprunld(long argc, long argv)
{
    char **av = (char **)(intptr_t)argv;
    pid_t pid;
    int st;

    (void)argc;
    if (!findprog(av[0])) {            /* the linker the tables name is not here.  The order: the lld library of the process, ld.lld or ld64.lld (the PATH, the bin directory of LLVM), then the system's own ld, which takes
                                * the same arguments (as gcc and clang run it): the ELF one for an ELF target, Apple's for Mach-O, the mingw one (TRIPLE-ld) for a -m i386pep link */
        const char *alt = NULL;

        if (strcmp(av[0], "ld64.lld") == 0)
            alt = "ld";
        else if (strcmp(av[0], "ld.lld") == 0 && av[1] != NULL && strcmp(av[1], "-m") == 0 && av[2] != NULL) {
            alt = strcmp(av[2], "i386pep") == 0 ? "x86_64-w64-mingw32-ld" : strcmp(av[2], "i386pe") == 0 ? "i686-w64-mingw32-ld" :
                  strcmp(av[2], "arm64pe") == 0 ? "aarch64-w64-mingw32-ld" : strcmp(av[2], "thumb2pe") == 0 ? "armv7-w64-mingw32-ld" : NULL;
            if (alt == NULL)
                alt = "ld";             /* an emulation of GNU ld (-m elf_x86_64) */
        } else if (strcmp(av[0], "ld.lld") == 0)
            alt = "ld";
        if (alt != NULL && findprog(alt))
            av[0] = (char *)alt;
        else {
            fprintf(stderr, "imp: cannot link: no linker found: %s is not in the PATH or in LLVM's bin directory%s\n", av[0],
                    strcmp(av[0], "ld.lld") == 0 ? ", and there is no plain ld in the PATH" : "");
            return 1;
        }
    }
    if (getenv("IMPLDSHOW") != NULL)
        fprintf(stderr, "linker: %s\n", av[0]);
    fflush(NULL);
    if ((pid = fork()) < 0)
        return 1;
    if (pid == 0) {
        execvp(av[0], av);
#ifdef IMP_LLVM_BINDIR
        if (errno == ENOENT) {                  /* not in the PATH: where the LLVM this was built with has its programs */
            char path[4096];

            snprintf(path, sizeof path, "%s/%s", LLVM_BINDIR, av[0]);
            execv(path, av);
        }
#endif
        perror(av[0]);
        _exit(127);
    }
    while (waitpid(pid, &st, 0) < 0)
        ;
    return WIFEXITED(st) && WEXITSTATUS(st) == 0 ? 0 : 1;
}

/*
 * The temporary files of the compiler (imptmp*): none unless the link needs a file for the object.  The object of a program to be linked goes to an unnamed memory file on Linux
 * (memfd_create, found at run time, and the linker is given /proc/self/fd/N), else to a file in a private directory: made when it is first needed, by mkdtemp in $TMPDIR (an
 * absolute path naming a directory) or else in the default (imptmpbase), checked to be a directory of mode 0700 owned by this user, and removed, with the files made in it, at exit
 * and by a handler of SIGHUP, SIGINT, SIGQUIT and SIGTERM, with unlink and rmdir and no shell.  lib/imphost.imp (IMPTMPOBJ and the others) is the IMP twin of this.
 */
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif

#define TD_LIST 65536
static char td_dflt[4096];                      /* the default base of the private directory */
static char td_try[4096];                       /* the private directory; it is armed (td_arm) from before mkdtemp makes it */
static char td_list[TD_LIST];                   /* the files made in it, as C strings one after the other, td_len bytes */
static volatile size_t td_len;
static volatile sig_atomic_t td_arm;
static volatile pid_t td_pid, td_kid;           /* the process that made it (a child of it does not remove it); a child process to stop first */
static int td_hooked;

/* removal of what was made: async signal safe (unlink, rmdir, kill, waitpid and getpid only, static data) */
static void
td_clean(void)
{
    const char *p, *e;
    int k;

    if (!td_arm || getpid() != td_pid)
        return;
    if (td_kid > 0) {
        kill(td_kid, SIGTERM);
        for (k = 0; k < 3 && waitpid(td_kid, NULL, 0) < 0; k++)
            ;
        td_kid = 0;
    }
    for (p = td_list, e = td_list + td_len; p < e; p += strlen(p) + 1)
        unlink(p);
    rmdir(td_try);
    td_arm = 0;
}

static void
td_atexit(void)
{
    td_clean();
}

static void
td_handler(int sig)
{
    int e = errno;

    td_clean();
    signal(sig, SIG_DFL);                       /* then the signal does what it does: the exit status is the signal's */
    errno = e;
    raise(sig);
}

static void
td_hook(void)
{
    static const int sigs[] = { SIGHUP, SIGINT, SIGQUIT, SIGTERM };
    struct sigaction sa, old;
    size_t k;

    if (td_hooked)
        return;
    td_hooked = 1;
    for (k = 0; k < sizeof sigs / sizeof sigs[0]; k++) {
        if (sigaction(sigs[k], NULL, &old) == 0 && old.sa_handler == SIG_IGN)
            continue;                           /* ignored (nohup, a job in the background): it stays so */
        memset(&sa, 0, sizeof sa);
        sa.sa_handler = td_handler;
        sigemptyset(&sa.sa_mask);
        sigaction(sigs[k], &sa, NULL);
    }
    atexit(td_atexit);
}

/* the default base of the private directory, which is made if it is not there (the source tree's tmp/) */
long
imptmpbase(const char *dflt)
{
    snprintf(td_dflt, sizeof td_dflt, "%s", dflt);
    return 0;
}

/* a private directory in the directory base: 1 if it was made */
static int
td_in(const char *base)
{
    struct stat st;

    td_arm = 0;
    snprintf(td_try, sizeof td_try, "%s/impstage0.XXXXXX", base);
    td_pid = getpid();
    td_len = 0;
    td_kid = 0;
    td_arm = 1;                                 /* armed before mkdtemp: a signal between it and the end of this finds the directory */
    if (mkdtemp(td_try) == NULL) {
        td_arm = 0;
        return 0;
    }
    if (lstat(td_try, &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != geteuid() || (st.st_mode & 0777) != 0700) {
        rmdir(td_try);
        td_arm = 0;
        return 0;
    }
    return 1;
}

static int
td_make(void)
{
    const char *v;
    DIR *d;

    if (td_arm)
        return 1;
    td_hook();
    v = getenv("TMPDIR");
    if (v != NULL && v[0] == '/' && (d = opendir(v)) != NULL) {
        closedir(d);
        if (td_in(v))
            return 1;
    }
    mkdir(td_dflt, 0777);
    return td_in(td_dflt);
}

/* the path of the file name in the private directory, which is made if need be, into out (4096 bytes); the file is to be removed with the directory.  0 if it worked */
long
imptmppath(const char *name, char *out)
{
    size_t l;

    if (!td_make()) {
        fprintf(stderr, "imp: cannot make a temporary directory\n");
        return 1;
    }
    snprintf(out, 4096, "%s/%s", td_try, name);
    l = strlen(out) + 1;
    if (td_len + l > TD_LIST) {
        fprintf(stderr, "imp: too many temporary files\n");
        return 1;
    }
    memcpy(td_list + td_len, out, l);
    td_len += l;                                /* (after the copy: the handler goes by td_len) */
    return 0;
}

/* a child process the handler stops and waits for before it removes the files (0: none) */
long
imptmpkid(long pid)
{
    td_kid = (pid_t)pid;
    return 0;
}

long
imptmpclean(long z)
{
    (void)z;
    td_clean();
    return 0;
}

static int
td_nomemfd(void)
{
    const char *v = getenv("IMPNOMEMFD");

    return v != NULL && *v && strcmp(v, "0") != 0;
}

static int
td_memfd(void)
{
#if defined(__linux__) && defined(SYS_memfd_create)
    long r = syscall(SYS_memfd_create, "imp", 0u);      /* (not through memfd_create: an older C library has none); no MFD_CLOEXEC, a linker run as a program has it */

    return r < 0 ? -1 : (int)r;
#else
    return -1;
#endif
}

static int
td_write(int fd, const char *p, size_t n)
{
    while (n > 0) {
        ssize_t r = write(fd, p, n);

        if (r <= 0)
            return 1;
        p += r;
        n -= (size_t)r;
    }
    return 0;
}

/* the n bytes of the object at buf in a file for the linker, whose path goes to out (4096 bytes): /proc/self/fd/N of a memory file where there is one, else a file of the private
 * directory made for the purpose (O_EXCL, mode 0600).  0 if it worked */
long
imptmpobj(const char *buf, size_t n, char *out)
{
    int fd;

    if (!td_nomemfd() && (fd = td_memfd()) >= 0) {
        if (td_write(fd, buf, n) == 0) {
            int t;

            snprintf(out, 4096, "/proc/self/fd/%d", fd);
            if ((t = open(out, O_RDONLY)) >= 0) {
                close(t);
                return 0;
            }
        }
        close(fd);
    }
    if (imptmppath("p.o", out) != 0)
        return 1;
    fd = open(out, O_WRONLY | O_CREAT | O_EXCL
#ifdef O_NOFOLLOW
              | O_NOFOLLOW
#endif
#ifdef O_CLOEXEC
              | O_CLOEXEC
#endif
              , 0600);
    if (fd < 0) {
        perror(out);
        return 1;
    }
    if (td_write(fd, buf, n) != 0 || close(fd) != 0) {
        perror(out);
        return 1;
    }
    return 0;
}
