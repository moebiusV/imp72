/*
 * sys.c -- what the IMP compiler's driver program (src/impm.imp) asks of the operating system: the command line, the listing file,
 * the file of pcc IR, the terminal.  The routines are called from IMP with CCALL, by value, and take C strings.  The ERROR and MSG of
 * the tape are IMP (impm.imp); imp_error and imp_msg are for the C that is left (semtab.c, ir.c, pccirsem.c).
 */
#define _DEFAULT_SOURCE 1
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "ir.h"
#include "imp.h"

extern word impl_error(word *, word *), impl_msg(word *);

FILE *listing;                          /* the listing and the print-outs: -o FILE; else the standard output */
int imp_quiet;                          /* the compiler driver: no chatter */
word imp_err;

static jmp_buf fin;                     /* where a fatal error unwinds to: the end of impk_main */
static int ac;
static char **av;

void impsetargs(int argc, char **argv) { ac = argc; av = argv; }
long impargc(long z) { (void)z; return ac; }
char *impargv(long i) { return av[i]; }
long impbyte(const char *p, long i) { return (unsigned char)p[i]; }
long impstreq(const char *a, const char *b) { return strcmp(a, b) == 0; }
long impisquiet(long z) { (void)z; return imp_quiet; }
long imphaslist(long z) { (void)z; return listing != NULL; }
void impoutstr(const char *s) { fputs(s, stderr); }
void impoutchr(long c) { fputc((int)c, stderr); }
long impgetch(long z) { (void)z; return getchar(); }
long impsystem(const char *cmd) { return system(cmd); }

FILE *
imp_out(void)
{
    return listing != NULL ? listing : stdout;
}

long
impopenlist(const char *path)
{
    listing = fopen(path, "w");
    if (listing == NULL) {
        perror(path);
        return -1;
    }
    return 0;
}

long
impcloselist(long z)
{
    (void)z;
    if (listing != NULL)
        fclose(listing);
    listing = NULL;
    return 0;
}

long
impopenir(const char *path)
{
    FILE *f = strcmp(path, "-") == 0 ? stdout : fopen(path, "w");

    if (f == NULL) {
        perror(path);
        return -1;
    }
    trans_open(f);
    return 0;
}

long impsettarget(const char *name) { return ir_settarget(name); }

/* does ERROR(N,E) say anything? (the driver of the compiler leaves out the advisory about a production that is there already) */
long
impnoisy(long n, const char *e)
{
    return !(imp_quiet && n == 2 && strncmp(e, "PRODUCTION DUPLICATES", 21) == 0);
}

void
imp_error(int n, const char *e)
{
    word nn = n, zero = 0, *ep = e != NULL && *e != '\0' ? (word *)(void *)e : &zero;

    impl_error(&nn, ep);
}

void
imp_msg(const char *m)
{
    impl_msg((word *)(void *)m);
}

void imp_fatal_handler(void (*fn)(void)) { (void)fn; }

/* the PRINT statements of the IMP modules go where the listing goes */
extern FILE *(*imp_defout)(void);

__attribute__((constructor)) static void
setdefout(void)
{
    imp_defout = imp_out;
}

long
impgetc(FILE *f)
{
    return fgetc(f);
}

/* ERROR(0,..): the compilation ends here (GO TO FIN of the original) */
long
impfatal(long z)
{
    (void)z;
    longjmp(fin, 1);
}

/* the driver program of the tape (src/impm.imp): 0 if all went well */
extern word impl_impk(word *);

int
impk_main(int argc, char **argv)
{
    word z = 0;
    int rc;

    impsetargs(argc, argv);
    if (setjmp(fin) == 0)
        return (int)impl_impk(&z);
    rc = 1;
    impcloselist(0);
    return rc;
}

long
impfclose(FILE *f)
{
    return fclose(f);
}

/*
 * The low segment of the tape's free storage grows in place (CORE sets its size and nothing moves), so the addresses of the arrays, and the byte pointers
 * into them, stay.  Here the address space for the arena is reserved once, without access, and CORE makes more of it usable.
 */
#ifndef MAP_ANON
#define MAP_ANON MAP_ANONYMOUS
#endif
#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif
static char *core_base;
static size_t core_reserved, core_usable;

long
impcorebase(long z)
{
    size_t r = sizeof(void *) > 4 ? (size_t)1 << 34 : (size_t)1 << 28;
    void *p;

    (void)z;
    if (core_base != NULL)
        return (long)core_base;
    for (; r >= ((size_t)1 << 24); r >>= 1)
        if ((p = mmap(NULL, r, PROT_NONE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0)) != MAP_FAILED) {
            core_base = p;
            core_reserved = r;
            return (long)core_base;
        }
    return 0;
}

/* make the first nbytes of the arena usable (zero to begin with); 1 for success */
long
impcoreset(long nbytes)
{
    size_t want = ((size_t)nbytes + 65535) & ~(size_t)65535;

    if (core_base == NULL || want > core_reserved)
        return 0;
    if (want > core_usable) {
        if (mprotect(core_base + core_usable, want - core_usable, PROT_READ | PROT_WRITE) != 0)
            return 0;
        core_usable = want;
    }
    return 1;
}
