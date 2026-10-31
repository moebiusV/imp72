/*
 * imphost.c -- what the link needs to know of the machine it runs on: the system (imphostos), the architecture (imphostarch), where this very program found its dynamic
 * linker (imphostinterp) and its C library (imphostlibc), the directory of the C library's crt files that goes with it (imphostcrtdir), and the newest version of a library that
 * is only there with its version (impfindlib).  The compiler runs on the system it makes programs for, so what its own loader did is what the programs it makes need.
 * src/driver.imp calls them with CCALL; the seed's driver, stage0/imp72s0.c, links this file in, and links the same way.
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

#ifdef IMP72_LLVM_BINDIR
#define STR2(x) #x
#define STR(x) STR2(x)
#define LLVM_BINDIR STR(IMP72_LLVM_BINDIR)
#endif

/* run lld as a program, found by the path under the name in argv[0] (ld.lld, ld64.lld), with the argument vector argv (NULL ended); wait for it; 0 if it linked.  For a compiler built
 * without the lld library, as OpenBSD's is (its own linker is lld, and it has no library of it) */
long
imprunld(long argc, long argv)
{
    char **av = (char **)(intptr_t)argv;
    pid_t pid;
    int st;

    (void)argc;
    fflush(NULL);
    if ((pid = fork()) < 0)
        return 1;
    if (pid == 0) {
        execvp(av[0], av);
#ifdef IMP72_LLVM_BINDIR
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
