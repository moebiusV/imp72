/*
 * imphost-win.c -- the host layer of the seed (stage0/imphost.c) for Windows: the temporary files, the linker run as a program, the memory files, the path of the program itself.
 * imphost.c is ELF and POSIX (fork, mkdtemp, the program interpreter in the program headers); a Windows program has none of those, so what imps0.c asks of the host that has no
 * meaning here (the loader, the C library of the machine, the directory of the start files of an ELF system) is answered "none", and the rest is Win32.
 */
#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "winport.h"

const char *
imphostos(long z)
{
    (void)z;
    return "windows";
}

const char *
imphostarch(long z)
{
    (void)z;
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64";
#elif defined(__i386__) || defined(_M_IX86)
    return "i386";
#else
    return "arm";
#endif
}

/* an ELF program's loader, C library and start-file directory: a Windows program has none */
long imphostinterp(long out) { (void)out; return 0; }
long imphostlibc(long out) { (void)out; return 0; }
long imphostcrtdir(long out) { (void)out; return 0; }
long imphostprobe(long crt, long interp) { (void)crt; (void)interp; return 0; }
long impfindlib(long out, long dir, long prefix) { (void)out; (void)dir; (void)prefix; return 0; }

int
winexepath(char *out, size_t n)
{
    DWORD k = GetModuleFileNameA(NULL, out, (DWORD)n);
    char *p;

    if (k == 0 || k >= n)
        return -1;
    for (p = out; *p; p++)
        if (*p == '\\')
            *p = '/';
    return 0;
}

FILE *
winmemfile(void)
{
    char dir[MAX_PATH], name[MAX_PATH];
    HANDLE h;
    int fd;
    FILE *f;

    if (GetTempPathA(sizeof dir, dir) == 0 || GetTempFileNameA(dir, "imp", 0, name) == 0)
        return NULL;
    h = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return NULL;
    fd = _open_osfhandle((intptr_t)h, _O_BINARY | _O_RDWR);
    if (fd < 0) {
        CloseHandle(h);
        return NULL;
    }
    f = _fdopen(fd, "w+b");
    if (f == NULL)
        _close(fd);
    return f;
}

FILE *
winmemfrom(const char *p, size_t n)
{
    FILE *f = winmemfile();

    if (f == NULL)
        return NULL;
    if (n > 0 && fwrite(p, 1, n, f) != n) {
        fclose(f);
        return NULL;
    }
    fflush(f);
    rewind(f);
    return f;
}

/* open_memstream: a temporary file, and a table of which file has which buf and len, filled in at winfclose */
static struct { FILE *f; char **buf; size_t *len; } ms[32];

FILE *
winopen_memstream(char **buf, size_t *len)
{
    FILE *f = winmemfile();
    int i;

    if (f == NULL)
        return NULL;
    for (i = 0; i < 32; i++)
        if (ms[i].f == NULL) {
            ms[i].f = f; ms[i].buf = buf; ms[i].len = len;
            *buf = NULL; *len = 0;
            return f;
        }
    fclose(f);
    return NULL;
}

int winmemslurp_keep(FILE *f, char **buf, size_t *len);

int
winfclose(FILE *f)
{
    int i, rc = 0;

    for (i = 0; i < 32; i++)
        if (ms[i].f == f) {
            rc = winmemslurp_keep(f, ms[i].buf, ms[i].len);
            ms[i].f = NULL;
            break;
        }
    return fclose(f) != 0 ? -1 : rc;
}

int
winmemslurp_keep(FILE *f, char **buf, size_t *len)
{
    long n;
    char *b;

    if (fflush(f) != 0 || fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0)
        return -1;
    b = malloc((size_t)n + 1);
    if (b == NULL || (n > 0 && fread(b, 1, (size_t)n, f) != (size_t)n)) {
        free(b);
        return -1;
    }
    b[n] = '\0';
    *buf = b;
    *len = (size_t)n;
    return 0;
}

int
winmemslurp(FILE *f, char **buf, size_t *len)
{
    long n;
    char *b;

    if (fflush(f) != 0 || fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    b = malloc((size_t)n + 1);
    if (b == NULL || (n > 0 && fread(b, 1, (size_t)n, f) != (size_t)n)) {
        free(b);
        fclose(f);
        return -1;
    }
    b[n] = '\0';
    *buf = b;
    *len = (size_t)n;
    fclose(f);
    return 0;
}

/* the temporary files: a private directory made on the first use, removed with what is in it at exit */
static char td_dflt[4096], td_dir[4096], *td_list;
static size_t td_len, td_cap;
static int td_hooked;

long
imptmpbase(const char *dflt)
{
    snprintf(td_dflt, sizeof td_dflt, "%s", dflt);
    return 0;
}

static void
td_clean(void)
{
    size_t i = 0;

    while (i < td_len) {
        DeleteFileA(td_list + i);
        i += strlen(td_list + i) + 1;
    }
    if (td_dir[0])
        RemoveDirectoryA(td_dir);
}

static int
td_make(void)
{
    char base[MAX_PATH];
    int k;

    if (td_dir[0])
        return 1;
    if (GetTempPathA(sizeof base, base) == 0)
        return 0;
    for (k = 0; k < 1000; k++) {
        snprintf(td_dir, sizeof td_dir, "%simp%lu_%d", base, (unsigned long)GetCurrentProcessId(), k);
        if (CreateDirectoryA(td_dir, NULL)) {
            if (!td_hooked) {
                atexit(td_clean);
                td_hooked = 1;
            }
            return 1;
        }
        if (GetLastError() != ERROR_ALREADY_EXISTS)
            break;
    }
    td_dir[0] = '\0';
    return 0;
}

static int
imptmppath(const char *name, char *out)
{
    size_t l;

    if (!td_make()) {
        fprintf(stderr, "imp: cannot make a temporary directory\n");
        return 1;
    }
    snprintf(out, 4096, "%s/%s", td_dir, name);
    l = strlen(out) + 1;
    if (td_len + l > td_cap) {
        char *n = realloc(td_list, td_cap = (td_len + l) * 2);

        if (n == NULL)
            return 1;
        td_list = n;
    }
    memcpy(td_list + td_len, out, l);
    td_len += l;
    return 0;
}

long
imptmpobj(const char *buf, size_t n, char *out)
{
    FILE *f;

    if (imptmppath("p.o", out) != 0)
        return 1;
    f = fopen(out, "wb");
    if (f == NULL) {
        perror(out);
        return 1;
    }
    if ((n > 0 && fwrite(buf, 1, n, f) != n) || fclose(f) != 0) {
        perror(out);
        return 1;
    }
    return 0;
}

long
imptmpkid(long pid)
{
    (void)pid;
    return 0;
}

long
imptmpclean(long z)
{
    (void)z;
    td_clean();
    return 0;
}

/* the linker as a program (where there is no lld library): argv[0] with the arguments, quoted for the command line of CreateProcess; 0 if it ran and succeeded */
long
imprunld(long argc, long argvp)
{
    const char **argv = (const char **)(intptr_t)argvp;
    char *cmd;
    size_t cap = 1, i;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD code = 1;
    char *p;

    for (i = 0; i < (size_t)argc; i++)
        cap += 2 * strlen(argv[i]) + 3;
    p = cmd = malloc(cap);
    if (cmd == NULL)
        return 1;
    for (i = 0; i < (size_t)argc; i++) {
        const char *a = argv[i];

        if (i)
            *p++ = ' ';
        *p++ = '"';
        for (; *a; a++) {
            if (*a == '"')
                *p++ = '\\';
            *p++ = *a;
        }
        *p++ = '"';
    }
    *p = '\0';
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "imp: cannot run %s\n", argv[0]);
        free(cmd);
        return 1;
    }
    free(cmd);
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (long)code;
}
#endif
