/*
 * image.c -- the switches U and V of the tape's driver: save the compiler, with the syntax and the declarations it has compiled, so that a later run
 * starts from there.  On TOPS-10 the monitor's SSAVE writes the core image of the compiler; here the language the compiler has learned is written to a
 * file: the free storage (the directory, the syntax graph, the code tree, the semantics array: all the tables), the variables of the modules of the
 * compiler (they are listed in the section impvars, a record for each, by the compiler that compiled the modules: ir_varrec) and the few tables of
 * the C side.  The image is for the compiler that made it: the variables are matched by name and the file says if the list is not the same.
 * The driver module (impm) is not in the image, nor the file that the lexer has open, and the arena is put back at the address it had, so
 * the pointers into it that the modules keep (the lexer's) are good.
 */
#define _GNU_SOURCE 1
#include <link.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "imp.h"

struct impvar {
    void *addr;
    uintptr_t size;
    const char *name;
};
extern struct impvar __start_impvars[] __attribute__((weak));
extern struct impvar __stop_impvars[] __attribute__((weak));

extern void *imp_core_info(size_t *usable);
extern int imp_core_at(void *base, size_t nbytes);
extern word symc, nsymc;                         /* src/pccir/ix.imp's cache of the assembler names, by directory index */

#define MAGIC "IMP72IMG"
#define VERSION 4

struct imghdr {
    char magic[8];
    uint32_t version, ptrsize;
    uint64_t build, base, used, nvars, nreloc;
};

static struct impvar *vars;                         /* every variable once, with its biggest size */
static size_t nvars;

/*
 * A name that a module uses and does not define is a variable of the module to the compiler, whatever it is (a routine of another module, say), and has its
 * record: only what lies in the writable part of the program is a variable.
 */
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

static int
skipped(const char *name)                           /* not kept in the image */
{
    return strncmp(name, "impm:", 5) == 0 || strncmp(name, "ix:", 3) == 0 || strcmp(name, "demit:usedfs") == 0
        || strcmp(name, "dir:rtn") == 0 || strcmp(name, "dir:nrtn") == 0 || strcmp(name, "dir:crtn") == 0 || strcmp(name, "lex:srcfp") == 0
        || strcmp(name, "free:relp") == 0;
}

static void
collect(void)
{
    struct impvar *r;
    size_t i, n = (size_t)(__stop_impvars - __start_impvars);

    free(vars);
    vars = calloc(n + 1, sizeof *vars);
    nvars = 0;
    for (r = __start_impvars; r < __stop_impvars; r++) {
        if (!writable(r))
            continue;
        for (i = 0; i < nvars; i++)
            if (vars[i].addr == r->addr) {
                if (r->size > vars[i].size)
                    vars[i].size = r->size;
                break;
            }
        if (i == nvars)
            vars[nvars++] = *r;
    }
}

static word zero_;
extern word impl_semcount(word *), impl_semname(word *), impl_semremap(word *, word *);       /* src/dir.imp's table of the semantic routines */

static const char *
semname(word k)
{
    word kk = k;

    return (const char *)(uintptr_t)impl_semname(&kk);
}

static struct impvar *
find(const char *name)
{
    size_t i;

    for (i = 0; i < nvars; i++)
        if (strcmp(vars[i].name, name) == 0)
            return &vars[i];
    return NULL;
}

/* a variable of a module known by its symbol alone: a COMMON one is kept under the first module that names it */
static struct impvar *
findsym(const char *sym)
{
    size_t i, n = strlen(sym), m;

    for (i = 0; i < nvars; i++)
        if ((m = strlen(vars[i].name)) > n && vars[i].name[m - n - 1] == ':' && strcmp(vars[i].name + m - n, sym) == 0)
            return &vars[i];
    return NULL;
}

static uint64_t
build(void)                                         /* what says the lists of variables are the same */
{
    uint64_t h = 1469598103934665603ULL;
    size_t i;
    const char *p;

    for (i = 0; i < nvars; i++) {
        for (p = vars[i].name; *p; p++)
            h = (h ^ (unsigned char)*p) * 1099511628211ULL;
        h = (h ^ (uint64_t)vars[i].size) * 1099511628211ULL;
    }
    return h;
}

static uint64_t
varword(const char *name)
{
    struct impvar *v = findsym(name);
    int64_t x = 0;

    if (v != NULL)
        memcpy(&x, v->addr, sizeof x < v->size ? sizeof x : v->size);
    return (uint64_t)x;
}

static void
setword(const char *name, uint64_t w)
{
    struct impvar *v = findsym(name);
    int64_t x = (int64_t)w;

    if (v != NULL)
        memcpy(v->addr, &x, sizeof x < v->size ? sizeof x : v->size);
}

/* an address inside a variable: its name and the offset in it */
static struct impvar *
owner(uintptr_t a, uintptr_t *off)
{
    size_t i;

    for (i = 0; i < nvars; i++)
        if (a >= (uintptr_t)vars[i].addr && a < (uintptr_t)vars[i].addr + vars[i].size) {
            *off = a - (uintptr_t)vars[i].addr;
            return &vars[i];
        }
    return NULL;
}


long
impimgsave(const char *path)
{
    struct imghdr h;
    size_t used, i, usable;
    void *base = imp_core_info(&usable);
    uint64_t nreloc;
    unsigned char *relp;
    FILE *f;

    if (__start_impvars == NULL || __start_impvars == __stop_impvars || base == NULL) {
        fputs("imp72: this compiler has no table of its variables to save (the modules were not compiled for it)\n", stderr);
        return 0;
    }
    collect();
    nreloc = varword("nreloc");
    relp = (unsigned char *)(uintptr_t)varword("relp");
    used = (size_t)(varword("fsize") * 8);       /* the words of the arena that FINIT and CORE have made */
    if (used > usable)
        used = usable;
    memset(&h, 0, sizeof h);
    memcpy(h.magic, MAGIC, 8);
    h.version = VERSION;
    h.ptrsize = (uint32_t)sizeof(void *);
    h.build = build();
    h.base = (uint64_t)(uintptr_t)base;
    h.used = used;
    for (i = 0; i < nvars; i++)
        if (!skipped(vars[i].name))
            h.nvars++;
    h.nreloc = nreloc;
    if ((f = fopen(path, "wb")) == NULL) {
        perror(path);
        return 0;
    }
    fwrite(&h, sizeof h, 1, f);
    for (i = 0; i < nvars; i++)
        if (!skipped(vars[i].name)) {
            uint32_t len = (uint32_t)strlen(vars[i].name) + 1;
            uint64_t size = vars[i].size;

            fwrite(&len, sizeof len, 1, f);
            fwrite(vars[i].name, 1, len, f);
            fwrite(&size, sizeof size, 1, f);
            fwrite(vars[i].addr, 1, vars[i].size, f);
        }
    fwrite(base, 1, used, f);
    {                                               /* the routines in the order of their ids, which the tables carry */
        uint64_t n = (uint64_t)impl_semcount(&zero_);
        uint64_t k;

        fwrite(&n, sizeof n, 1, f);
        for (k = 0; k < n; k++) {
            uint32_t len = (uint32_t)strlen(semname((word)k)) + 1;

            fwrite(&len, sizeof len, 1, f);
            fwrite(semname((word)k), 1, len, f);
        }
    }
    for (i = 0; i < nreloc; i++) {                  /* the registry of the arrays that move: the variable that points at each, as a name and an offset */
        uint64_t a, s;
        uintptr_t off = 0;
        struct impvar *o;
        uint32_t len;

        memcpy(&a, relp + 16 * i, 8);
        memcpy(&s, relp + 16 * i + 8, 8);
        o = a ? owner((uintptr_t)a, &off) : NULL;
        if (a != 0 && o == NULL)
            fprintf(stderr, "imp72: a pointer variable of the free storage (slot %lu, at %#lx) is not one the image knows\n", (unsigned long)i, (unsigned long)a);
        len = o ? (uint32_t)strlen(o->name) + 1 : 0;
        fwrite(&len, sizeof len, 1, f);
        if (o != NULL) {
            uint64_t off64 = off;

            fwrite(o->name, 1, len, f);
            fwrite(&off64, sizeof off64, 1, f);
        }
        fwrite(&s, sizeof s, 1, f);
    }
    if (fclose(f) != 0) {
        perror(path);
        return 0;
    }
    return 1;
}

long
impimgload(const char *path)
{
    struct imghdr h;
    FILE *f;
    size_t i;
    unsigned char *arena, *relp;

    if (__start_impvars == NULL || __start_impvars == __stop_impvars) {
        fputs("imp72: this compiler cannot load an image\n", stderr);
        return 0;
    }
    collect();
    if ((f = fopen(path, "rb")) == NULL) {
        perror(path);
        return 0;
    }
    if (fread(&h, sizeof h, 1, f) != 1 || memcmp(h.magic, MAGIC, 8) != 0 || h.version != VERSION || h.ptrsize != sizeof(void *)) {
        fprintf(stderr, "imp72: %s is not an image of this kind of compiler\n", path);
        fclose(f);
        return 0;
    }
    if (h.build != build()) {
        fprintf(stderr, "imp72: %s was saved by another build of the compiler\n", path);
        fclose(f);
        return 0;
    }
    if (!imp_core_at((void *)(uintptr_t)h.base, (size_t)h.used)) {
        fprintf(stderr, "imp72: cannot put the free storage of %s where it was saved\n", path);
        fclose(f);
        return 0;
    }
    for (i = 0; i < h.nvars; i++) {                 /* the variables */
        uint32_t len;
        uint64_t size;
        char name[256];
        struct impvar *v;
        unsigned char *buf;

        if (fread(&len, sizeof len, 1, f) != 1 || len > sizeof name || fread(name, 1, len, f) != len || fread(&size, sizeof size, 1, f) != 1)
            goto bad;
        buf = malloc((size_t)size + 1);
        if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
            free(buf);
            goto bad;
        }
        if ((v = find(name)) != NULL && v->size == size)
            memcpy(v->addr, buf, (size_t)size);
        free(buf);
    }
    arena = (unsigned char *)(uintptr_t)h.base;
    if (fread(arena, 1, (size_t)h.used, f) != (size_t)h.used)
        goto bad;
    if (symc != 0)                                   /* the directory is another: the names are made again */
        memset((void *)(uintptr_t)symc, 0, (size_t)nsymc * 8);
    {
        uint64_t n, k;
        char **names;

        if (fread(&n, sizeof n, 1, f) != 1 || n > 100000)
            goto bad;
        names = calloc((size_t)n + 1, sizeof *names);
        for (k = 0; k < n; k++) {
            uint32_t len;

            if (fread(&len, sizeof len, 1, f) != 1 || len > 256)
                goto bad;
            names[k] = malloc(len);
            if (fread(names[k], 1, len, f) != len)
                goto bad;
        }
        {
            word nw = (word)n, nm = (word)(uintptr_t)names;

            if (!impl_semremap(&nm, &nw)) {
                fclose(f);
                return 0;
            }
        }
    }
    relp = malloc(16 * ((size_t)h.nreloc + 1));
    memset(relp, 0, 16 * ((size_t)h.nreloc + 1));
    for (i = 0; i < h.nreloc; i++) {
        uint32_t len;
        char name[256];
        uint64_t off, s, a = 0;
        struct impvar *v;

        if (fread(&len, sizeof len, 1, f) != 1)
            goto bad;
        if (len > 0) {
            if (len > sizeof name || fread(name, 1, len, f) != len || fread(&off, sizeof off, 1, f) != 1)
                goto bad;
            if ((v = find(name)) != NULL)
                a = (uint64_t)((uintptr_t)v->addr + (uintptr_t)off);
        }
        if (fread(&s, sizeof s, 1, f) != 1)
            goto bad;
        memcpy(relp + 16 * i, &a, 8);
        memcpy(relp + 16 * i + 8, &s, 8);
    }
    setword("relp", (uint64_t)(uintptr_t)relp);
    setword("nreloc", h.nreloc);
    fclose(f);
    return 1;
bad:
    fprintf(stderr, "imp72: %s is damaged\n", path);
    fclose(f);
    return 0;
}
