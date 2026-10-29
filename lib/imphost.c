/*
 * imphost.c -- what the link needs to know of the machine it runs on: the system (imphostos), the architecture (imphostarch) and the newest version of a library that is
 * only there with its version (impfindlib).  src/driver.imp calls them with CCALL; the seed's driver, stage0/imp72s0.c, links this file in, and links the same way.
 */
#include <dirent.h>
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
