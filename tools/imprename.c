/*
 * imprename.c -- the part of nm and objcopy that the build of the compilers uses (tools/direct.sh), for the object files of every system the compiler runs on: ELF (32 and 64 bit,
 * little and big endian) and Mach-O (64 bit).  No binutils, no llvm-objcopy.
 *
 *     imprename -d OBJECT                 print the names of the global symbols the object defines in its text, data and bss and its common symbols (what nm shows as T, D, B and C),
 *                                         one a line, without the underscore that Mach-O puts first
 *     imprename -f MAP OBJECT OUT         write OBJECT to OUT with the symbols named in MAP renamed; MAP has a line "OLD NEW" for each: a symbol OLD, defined or not, is NEW
 *
 * The new names are written at the end of the file and the string table of the symbols is made to point there; nothing else in the object changes (relocations name symbols by number).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
die(const char *msg, const char *arg)
{
    fprintf(stderr, "imprename: %s%s%s\n", msg, arg ? ": " : "", arg ? arg : "");
    exit(1);
}

static uint32_t
rd32(const uint8_t *p, int be)
{
    return be ? ((uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]) : ((uint32_t)p[3] << 24 | (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0]);
}

static uint16_t
rd16(const uint8_t *p, int be)
{
    return be ? (uint16_t)(p[0] << 8 | p[1]) : (uint16_t)(p[1] << 8 | p[0]);
}

static uint64_t
rd64(const uint8_t *p, int be)
{
    uint64_t a = rd32(p, be), b = rd32(p + 4, be);

    return be ? a << 32 | b : b << 32 | a;
}

static void
wr32(uint8_t *p, uint32_t v, int be)
{
    int i;

    for (i = 0; i < 4; i++)
        p[be ? 3 - i : i] = (uint8_t)(v >> (8 * i));
}

static void
wr64(uint8_t *p, uint64_t v, int be)
{
    wr32(be ? p + 4 : p, (uint32_t)v, be);
    wr32(be ? p : p + 4, (uint32_t)(v >> 32), be);
}

static uint8_t *
slurp(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    long n;
    uint8_t *p;

    if (f == NULL)
        die("cannot read", path);
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    p = calloc((size_t)n + 16, 1);
    if (p == NULL || (n > 0 && fread(p, 1, (size_t)n, f) != (size_t)n))
        die("cannot read", path);
    fclose(f);
    *size = (size_t)n;
    return p;
}

/* ---- the map of the new names, sorted by the old one ---- */
struct pair {
    char *from, *to;
};
static struct pair *map;
static size_t nmap;

static int
paircmp(const void *a, const void *b)
{
    return strcmp(((const struct pair *)a)->from, ((const struct pair *)b)->from);
}

static void
readmap(const char *path)
{
    size_t sz, cap = 0;
    uint8_t *d = slurp(path, &sz);
    char *p = (char *)d, *end = p + sz;

    while (p < end) {
        char *nl = memchr(p, '\n', (size_t)(end - p)), *sp;

        if (nl == NULL)
            nl = end;
        *nl = '\0';
        if ((sp = strchr(p, ' ')) != NULL && sp > p && sp[1] != '\0') {
            *sp = '\0';
            if (nmap == cap) {
                cap = cap ? cap * 2 : 1024;
                map = realloc(map, cap * sizeof *map);
                if (map == NULL)
                    die("out of memory", NULL);
            }
            map[nmap].from = p;
            map[nmap].to = sp + 1;
            nmap++;
        }
        p = nl + 1;
    }
    qsort(map, nmap, sizeof *map, paircmp);
}

static const char *
newname(const char *name)
{
    struct pair key, *r;

    if (nmap == 0)
        return NULL;
    key.from = (char *)name;
    key.to = NULL;
    r = bsearch(&key, map, nmap, sizeof *map, paircmp);
    return r ? r->to : NULL;
}

/* ---- the new string table: the old strings and the new names after them ---- */
static uint8_t *strs;
static size_t nstrs, capstrs;

static void
strs_init(const uint8_t *old, size_t n)
{
    capstrs = n + 4096;
    strs = malloc(capstrs);
    if (strs == NULL)
        die("out of memory", NULL);
    memcpy(strs, old, n);
    nstrs = n;
}

static uint32_t
strs_add(const char *s, const char *prefix)
{
    size_t need = strlen(prefix) + strlen(s) + 1, at = nstrs;

    if (nstrs + need > capstrs) {
        capstrs = (nstrs + need) * 2;
        strs = realloc(strs, capstrs);
        if (strs == NULL)
            die("out of memory", NULL);
    }
    sprintf((char *)strs + at, "%s%s", prefix, s);
    nstrs += need;
    return (uint32_t)at;
}

static void
write_out(const char *path, const uint8_t *d, size_t sz, size_t pad)
{
    FILE *f = fopen(path, "wb");

    if (f == NULL)
        die("cannot write", path);
    if (fwrite(d, 1, sz, f) != sz || (pad > 0 && fwrite("\0\0\0\0\0\0\0\0", 1, pad, f) != pad) || fwrite(strs, 1, nstrs, f) != nstrs)
        die("cannot write", path);
    fclose(f);
}

/* ---- ELF ---- */
struct elf {
    uint8_t *d;
    size_t sz;
    int is64, be;
    uint64_t shoff;
    unsigned shentsize, shnum;
};

static uint8_t *
elf_sh(struct elf *e, unsigned i)
{
    return e->d + e->shoff + (uint64_t)i * e->shentsize;
}

static uint32_t
elf_shtype(struct elf *e, unsigned i)
{
    return rd32(elf_sh(e, i) + 4, e->be);
}

static uint64_t
elf_shflags(struct elf *e, unsigned i)
{
    return e->is64 ? rd64(elf_sh(e, i) + 8, e->be) : rd32(elf_sh(e, i) + 8, e->be);
}

static uint64_t
elf_shoffset(struct elf *e, unsigned i)
{
    return e->is64 ? rd64(elf_sh(e, i) + 24, e->be) : rd32(elf_sh(e, i) + 16, e->be);
}

static uint64_t
elf_shsize(struct elf *e, unsigned i)
{
    return e->is64 ? rd64(elf_sh(e, i) + 32, e->be) : rd32(elf_sh(e, i) + 20, e->be);
}

static uint32_t
elf_shlink(struct elf *e, unsigned i)
{
    return rd32(elf_sh(e, i) + (e->is64 ? 40 : 24), e->be);
}

static void
elf_open(struct elf *e, uint8_t *d, size_t sz)
{
    e->d = d;
    e->sz = sz;
    if (sz < 52 || memcmp(d, "\177ELF", 4) != 0)
        die("not an ELF object", NULL);
    e->is64 = d[4] == 2;
    e->be = d[5] == 2;
    if (e->is64) {
        e->shoff = rd64(d + 0x28, e->be);
        e->shentsize = rd16(d + 0x3a, e->be);
        e->shnum = rd16(d + 0x3c, e->be);
    } else {
        e->shoff = rd32(d + 0x20, e->be);
        e->shentsize = rd16(d + 0x2e, e->be);
        e->shnum = rd16(d + 0x30, e->be);
    }
    if (e->shoff == 0 || e->shoff + (uint64_t)e->shnum * e->shentsize > sz)
        die("no section headers", NULL);
}

/* the first symbol table: its section, its strings' section */
static int
elf_symtab(struct elf *e, unsigned *sec, unsigned *strsec)
{
    unsigned i;

    for (i = 0; i < e->shnum; i++)
        if (elf_shtype(e, i) == 2) {
            *sec = i;
            *strsec = elf_shlink(e, i);
            return 1;
        }
    return 0;
}

static void
elf_list(uint8_t *d, size_t sz)
{
    struct elf e;
    unsigned st, ss;
    uint64_t n, i, ent;
    const uint8_t *syms;
    const char *str;

    elf_open(&e, d, sz);
    if (!elf_symtab(&e, &st, &ss))
        return;
    ent = e.is64 ? 24 : 16;
    n = elf_shsize(&e, st) / ent;
    syms = d + elf_shoffset(&e, st);
    str = (const char *)d + elf_shoffset(&e, ss);
    for (i = 1; i < n; i++) {
        const uint8_t *s = syms + i * ent;
        unsigned info = e.is64 ? s[4] : s[12], type = info & 15, bind = info >> 4;
        unsigned shndx = rd16(s + (e.is64 ? 6 : 14), e.be);
        uint32_t nm = rd32(s, e.be);
        int ok;

        if (nm == 0 || bind != 1 || type == 3 || type == 4 || shndx == 0)
            continue;                                   /* nm's T, D, B and C are the global (not weak) symbols that are defined, in text, data and bss, or common */
        if (shndx == 0xfff2)
            ok = 1;
        else if (shndx >= 0xff00 || shndx >= e.shnum)
            ok = 0;
        else {
            uint64_t fl = elf_shflags(&e, shndx);

            ok = (fl & 4) || ((fl & 2) && (fl & 1));    /* executable, or allocated and writable */
        }
        if (ok)
            printf("%s\n", str + nm);
    }
}

static void
elf_rename(uint8_t *d, size_t sz, const char *outpath)
{
    struct elf e;
    unsigned st, ss;
    uint64_t n, i, ent;
    uint8_t *syms;
    const char *str;
    uint64_t stroff, strsz;
    uint8_t *shs;
    size_t pad;

    elf_open(&e, d, sz);
    if (!elf_symtab(&e, &st, &ss))
        die("no symbol table", NULL);
    ent = e.is64 ? 24 : 16;
    n = elf_shsize(&e, st) / ent;
    syms = d + elf_shoffset(&e, st);
    stroff = elf_shoffset(&e, ss);
    strsz = elf_shsize(&e, ss);
    str = (const char *)d + stroff;
    strs_init(d + stroff, (size_t)strsz);
    for (i = 1; i < n; i++) {
        uint8_t *s = syms + i * ent;
        uint32_t nm = rd32(s, e.be);
        const char *to;

        if (nm == 0 || nm >= strsz)
            continue;
        if ((to = newname(str + nm)) != NULL)
            wr32(s, strs_add(to, "") + 0, e.be);        /* the offset in the new table: the new strings follow the old ones, whose offsets do not change */
    }
    /* the new table goes at the end of the file, aligned to 8 */
    pad = (8 - sz % 8) % 8;
    shs = elf_sh(&e, ss);
    if (e.is64) {
        wr64(shs + 24, sz + pad, e.be);
        wr64(shs + 32, nstrs, e.be);
    } else {
        wr32(shs + 16, (uint32_t)(sz + pad), e.be);
        wr32(shs + 20, (uint32_t)nstrs, e.be);
    }
    write_out(outpath, d, sz, pad);
}

/* ---- Mach-O (64 bit, little endian) ---- */
#define MH_MAGIC_64 0xfeedfacfu
#define LC_SYMTAB 2

static void
macho_symtab(uint8_t *d, size_t sz, uint32_t **lc)
{
    uint32_t ncmds, i;
    size_t off = 32;

    if (sz < 32 || rd32(d, 0) != MH_MAGIC_64)
        die("not a 64 bit Mach-O object", NULL);
    ncmds = rd32(d + 16, 0);
    *lc = NULL;
    for (i = 0; i < ncmds && off + 8 <= sz; i++) {
        uint32_t cmd = rd32(d + off, 0), size = rd32(d + off + 4, 0);

        if (cmd == LC_SYMTAB) {
            *lc = (uint32_t *)(void *)(d + off);        /* symoff, nsyms, stroff, strsize follow cmd and cmdsize */
            return;
        }
        if (size == 0)
            break;
        off += size;
    }
    die("no symbol table", NULL);
}

static void
macho_list(uint8_t *d, size_t sz)
{
    uint32_t *lc, i;
    uint32_t symoff, nsyms, stroff;

    macho_symtab(d, sz, &lc);
    symoff = rd32((uint8_t *)(lc + 2), 0);
    nsyms = rd32((uint8_t *)(lc + 3), 0);
    stroff = rd32((uint8_t *)(lc + 4), 0);
    for (i = 0; i < nsyms; i++) {
        const uint8_t *s = d + symoff + (size_t)i * 16;
        uint32_t strx = rd32(s, 0);
        unsigned type = s[4];
        const char *name = (const char *)d + stroff + strx;
        int sect = (type & 0x0e) == 0x0e, common = (type & 0x0e) == 0 && rd64(s + 8, 0) != 0;

        if (!(type & 1) || type & 0xe0 || (!sect && !common) || *name == '\0')
            continue;                                   /* external, not a debugging entry, defined in a section or common */
        printf("%s\n", *name == '_' ? name + 1 : name);
    }
}

static void
macho_rename(uint8_t *d, size_t sz, const char *outpath)
{
    uint32_t *lc, i;
    uint32_t symoff, nsyms, stroff, strsize;
    size_t pad;

    macho_symtab(d, sz, &lc);
    symoff = rd32((uint8_t *)(lc + 2), 0);
    nsyms = rd32((uint8_t *)(lc + 3), 0);
    stroff = rd32((uint8_t *)(lc + 4), 0);
    strsize = rd32((uint8_t *)(lc + 5), 0);
    strs_init(d + stroff, strsize);
    for (i = 0; i < nsyms; i++) {
        uint8_t *s = d + symoff + (size_t)i * 16;
        uint32_t strx = rd32(s, 0);
        unsigned type = s[4];
        const char *name = (const char *)d + stroff + strx, *to;

        if (!(type & 1) || type & 0xe0 || *name != '_' || strx >= strsize)
            continue;
        if ((to = newname(name + 1)) != NULL)
            wr32(s, strs_add(to, "_"), 0);
    }
    pad = (8 - sz % 8) % 8;
    wr32((uint8_t *)(lc + 4), (uint32_t)(sz + pad), 0);
    wr32((uint8_t *)(lc + 5), (uint32_t)nstrs, 0);
    write_out(outpath, d, sz, pad);
}

int
main(int argc, char **argv)
{
    size_t sz;
    uint8_t *d;
    int macho;

    if (argc == 3 && strcmp(argv[1], "-d") == 0) {
        d = slurp(argv[2], &sz);
        macho = sz >= 4 && rd32(d, 0) == MH_MAGIC_64;
        if (macho)
            macho_list(d, sz);
        else
            elf_list(d, sz);
        return 0;
    }
    if (argc == 5 && strcmp(argv[1], "-f") == 0) {
        readmap(argv[2]);
        d = slurp(argv[3], &sz);
        macho = sz >= 4 && rd32(d, 0) == MH_MAGIC_64;
        if (macho)
            macho_rename(d, sz, argv[4]);
        else
            elf_rename(d, sz, argv[4]);
        return 0;
    }
    fputs("usage: imprename -d OBJECT | imprename -f MAP OBJECT OUT\n", stderr);
    return 2;
}
