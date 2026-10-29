/*
 * imp72sys.c -- what the runtime library of IMP72 programs and the IMP compiler ask of the C library and the system that C alone can say: the standard
 * files, the arena's address space (the flags of mmap are the system's), the section that holds the compiler's own variables, a call through a
 * function pointer, and the cells that carry the arguments of a runtime call.  The runtime itself (PRINT, READ, FINI, CALLI, BLT, IFIX, the shifts,
 * the byte pointers) is IMP: lib/imp72io.imp and lib/imp72rt.imp.
 */
#define _GNU_SOURCE 1
#define _DEFAULT_SOURCE 1
#define _BSD_SOURCE 1                   /* (before glibc 2.19 the name of it) */
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>

/* the cells that carry the arguments of a call of the runtime: the compiled code stores them there and passes their addresses (IMP takes its arguments by reference) */
int64_t impca[4];

/* the standard files, which an IMP program cannot name */
long impstdout(long z) { (void)z; return (long)(intptr_t)stdout; }
long impstdin(long z) { (void)z; return (long)(intptr_t)stdin; }

/* where PRINT goes when it has no file: the function (an IMP routine, or any that returns a FILE *) the driver says, else the standard output (lib/imp72io.imp) */
extern long rtldefout;

void impsetdefout(void *f) { rtldefout = (long)(intptr_t)f; }
long impcallout(long f) { return (long)(intptr_t)((void *(*)(void))(intptr_t)f)(); }

/* the arena of the IMP compiler's free storage (src/free.imp, CORE), which has to be here for the system's constants: */
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
/* NetBSD with PaX MPROTECT: the most a mapping may ever become is what it is made with */
#ifdef PROT_MPROTECT
#define ARENA_PROT (PROT_NONE | PROT_MPROTECT(PROT_READ | PROT_WRITE))
#else
#define ARENA_PROT PROT_NONE
#endif
static char *core_base;
static size_t core_reserved, core_usable;

long
impcorebase(long z)
{
    size_t r = sizeof(void *) > 4 ? (size_t)1 << 34 : (size_t)1 << 28;
    /* the same place every time, so that an image of the compiler (U and V, image.c) can be put back where its pointers into the arena are good */
    void *hint = sizeof(void *) > 4 ? (void *)((size_t)1 << 44) : (void *)(size_t)0x20000000;
    void *p;

    (void)z;
    if (core_base != NULL)
        return (long)core_base;
    for (; r >= ((size_t)1 << 24); r >>= 1)
        if ((p = mmap(hint, r, ARENA_PROT, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0)) != MAP_FAILED) {
            core_base = p;
            core_reserved = r;
            if (getenv("IMP72COREDEBUG") != NULL)
                fprintf(stderr, "core: asked for %zu bytes at %p, got them at %p\n", r, hint, p);
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

/* the arena, for the image (src/image.imp): how much of it is usable; and, to load an image, the arena at its old place with at least nbytes usable */
long impcoreused(long z) { (void)z; return (long)core_usable; }

long
impcoreat(long base, long nbytes)
{
    if (impcorebase(0) == 0 || (long)core_base != base) {
        if (getenv("IMP72COREDEBUG") != NULL)
            fprintf(stderr, "core: the image has the arena at %#lx, it is at %p\n", base, (void *)core_base);
        return 0;
    }
    if (impcoreset(nbytes) == 0) {
        if (getenv("IMP72COREDEBUG") != NULL)
            fprintf(stderr, "core: %ld bytes of the arena could not be made usable (reserved %zu)\n", nbytes, core_reserved);
        return 0;
    }
    return 1;
}

long impptrsize(long z) { (void)z; return (long)sizeof(void *); }

/* an element of an argument vector, a char *[], and the storing of one, whatever the size of a pointer (src/driver.imp and src/impm.imp read and build the vectors with
 * these).  The element is stored in the 64 bit word at OUT, zero extended: a long returned to CCALL is sign extended, which makes a pointer above 2 GB, as the strings of
 * the arguments are on a 32 bit machine, a word that the byte pointers of IMP take for something else */
long imparget(long out, long v, long i) { *(int64_t *)(intptr_t)out = (int64_t)(uintptr_t)((char **)(intptr_t)v)[i]; return 0; }
long impargset(long v, long i, long p) { ((char **)(intptr_t)v)[i] = (char *)(intptr_t)p; return 0; }

/*
 * The variables of the compiler's own modules (src/pccir/ix.imp IRVARREC): a record of the address, the size and the name, MODULE:SYMBOL, in the section impvars,
 * which the linker bounds with __start_impvars and __stop_impvars.  Only what lies in the writable part of the program is a variable (a name that a module uses
 * and does not define is a variable of the module to the compiler, whatever it is, a routine of another module, say); each variable once, with its biggest size.
 * impvarlist(out, max) writes up to max records of three words (address, size, pointer to the name) and returns how many there are, or -1 if there is no table.
 */
#ifdef __ELF__
#include <link.h>
struct impvar {
    void *addr;
    uintptr_t size;
    const char *name;
};
extern struct impvar __start_impvars[] __attribute__((weak));
extern struct impvar __stop_impvars[] __attribute__((weak));

static struct { uintptr_t lo, hi; } wr[16];
static int nwr;

static int
phdr(struct dl_phdr_info *info, size_t size, void *data)
{
    int i;

    (void)size;
    (void)data;
    for (i = 0; i < info->dlpi_phnum && nwr < 16; i++)
        if (info->dlpi_phdr[i].p_type == PT_LOAD && (info->dlpi_phdr[i].p_flags & PF_W)) {
            wr[nwr].lo = (uintptr_t)info->dlpi_addr + info->dlpi_phdr[i].p_vaddr;
            wr[nwr].hi = wr[nwr].lo + info->dlpi_phdr[i].p_memsz;
            nwr++;
        }
    return 1;                                       /* the first object is the program */
}

static int
writable(const struct impvar *v)
{
    int i;

    if (nwr == 0)
        dl_iterate_phdr(phdr, NULL);
    for (i = 0; i < nwr; i++)
        if ((uintptr_t)v->addr >= wr[i].lo && (uintptr_t)v->addr + v->size <= wr[i].hi)
            return 1;
    return 0;
}

long
impvarlist(int64_t *out, long max)
{
    struct impvar *r;
    long n = 0, i;

    if (__start_impvars == NULL || __start_impvars == __stop_impvars)
        return -1;
    for (r = __start_impvars; r < __stop_impvars; r++) {
        if (!writable(r))
            continue;
        for (i = 0; i < n; i++)
            if (out != NULL && out[3 * i] == (int64_t)(uintptr_t)r->addr) {
                if ((int64_t)r->size > out[3 * i + 1])
                    out[3 * i + 1] = (int64_t)r->size;
                break;
            }
        if (out == NULL) {                          /* counting: the same test, on the records seen so far */
            struct impvar *q;

            for (q = __start_impvars; q < r; q++)
                if (q->addr == r->addr && writable(q))
                    break;
            if (q != r)
                continue;
            n++;
        } else if (i == n && n < max) {
            out[3 * n] = (int64_t)(uintptr_t)r->addr;
            out[3 * n + 1] = (int64_t)r->size;
            out[3 * n + 2] = (int64_t)(uintptr_t)r->name;
            n++;
        }
    }
    return n;
}
#else
long impvarlist(int64_t *out, long max) { (void)out; (void)max; return -1; }
#endif

/* fclose for the IMP compiler: its tape routine FCLOSE (src/free.imp) is the name a CCALL of the C library's would meet */
long
impfclose(FILE *f)
{
    return fclose(f);
}

/* a call through an address, for the table of semantic routines of the IMP compiler (src/dir.imp SEMCALL): the routine is an IMP routine, which takes its
 * arguments by reference; it gets pointers to a copy of the five parameters, which it may change */
int64_t
impsemcall(void *fn, const int64_t *pars)
{
    int64_t a[5];

    memcpy(a, pars, sizeof a);
    return ((int64_t (*)(int64_t *, int64_t *, int64_t *, int64_t *, int64_t *))fn)(&a[0], &a[1], &a[2], &a[3], &a[4]);
}

