/*
 * imp72s0.c -- the stage 0 driver.  The part of decus/impm.imc that matters
 * here: read the bootstrap syntax with RSYN, then compile the rest of the
 * file, and any other files named, with the syntax built so far.
 *
 *     imp72s0 [-s LETTERS] [-o FILE] [-c IRFILE] [-T TARGET] SYNTAX [FILE ...]
 *
 * -s sets compiler switches, each letter of LETTERS the switch of that name
 * (the original's /A /B ... on the command line); the ones that mean
 * something here are G (trace the parser), I (print the directory), N (print
 * the connectivity matrices with the graph), P (trace semantics execution),
 * M (trace tree matching), S (print the semantics array), T (print the code
 * tree), X (print the syntax graph), L (list the source), Z (statistics).
 * Dumps go to the file given with -o, else to standard output.  -c writes
 * the IR of the program to a file (- is standard output); LLVM turns it into
 * the object.  -T names the machine the IR is lowered for (amd64, the default,
 * or aarch64): the registers of the procedure call standard.
 *
 * The same program is also the compiler, imp72stage0 (a link to it, or a
 * copy: the name decides):
 *
 *     imp72stage0 [-S | -c] [--run] [-o OUT] [-s LETTERS] FILE.imp ...
 *
 * It reads the syntax of the language (syntax.imc) and the extension ffi.imc,
 * compiles the files with the kernel above to IR, and makes the IR, with LLVM
 * in this process, into an executable (default), an object (-c), LLVM IR (-S)
 * or a run (--run, with ORC).  It finds its parts relative to
 * itself: in the source tree (stage0/imp72stage0, decus/, build/) or installed
 * (PREFIX/bin/imp72stage0, PREFIX/lib/imp72/, PREFIX/share/imp72/).
 */
/* the compiler driver below uses POSIX (readlink, realpath, mkdtemp) */
#define _XOPEN_SOURCE 700
#ifdef __APPLE__
#define _DARWIN_C_SOURCE        /* macOS hides mkdtemp under _XOPEN_SOURCE alone */
#endif
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "imp.h"
#include "ir.h"


/* the LLVM back end in the process (stage0/llvmorc.c); llvmhave tells whether the compiler was built with the LLVM libraries */
extern long llvmhave(long), llvmobj(const char *ir, const char *out), llvmsave(const char *ir, const char *out), llvmrun(const char *ir);

static void
cotree_hook(word k)
{
    cotree(k);
}

static void
usage(void)
{
    fputs("usage: imp72s0 [-s LETTERS] [-o FILE] [-c IRFILE] [-T TARGET] SYNTAX [FILE ...]\n", stderr);
    exit(2);
}

static int
kernel_main(int argc, char **argv)
{
    int i = 1, nfiles;
    const char *out = NULL, *irout = NULL;
    char **files;

    while (i < argc && argv[i][0] == '-' && argv[i][1] != '\0') {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            const char *p;

            for (p = argv[++i]; *p; p++)
                if (isalpha((unsigned char)*p))
                    finam5 |= (word)1 << (toupper((unsigned char)*p) - 'A');
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            out = argv[++i];
        else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc)
            irout = argv[++i];
        else if (strcmp(argv[i], "-T") == 0 && i + 1 < argc) {
            if (!ir_settarget(argv[++i])) {
                fprintf(stderr, "imp72s0: no target %s\n", argv[i]);
                return 2;
            }
        } else
            usage();
        i++;
    }
    if (argc - i < 1)
        usage();
    files = argv + i;
    nfiles = argc - i;
    if (out != NULL && (listing = fopen(out, "w")) == NULL) {
        perror(out);
        return 1;
    }

    if (irout != NULL) {
        FILE *f = strcmp(irout, "-") == 0 ? stdout : fopen(irout, "w");

        if (f == NULL) {
            perror(irout);
            return 1;
        }
        trans_open(f);
    }
    sem_init();
    if (lexin(files[0]) != 0) {
        fprintf(stderr, "CAN'T FIND FILE: %s\n", files[0]);
        return 1;
    }
    rsyn();
    imp_msg("SYNTAX BOOTSTRAPPED");

    for (i = 0; i < nfiles; i++) {
        if (i > 0 && lexin(files[i]) != 0) {
            fprintf(stderr, "? FILE NOT FOUND: %s\n", files[i]);
            return 1;
        }
        stinit();
        cotree(0);
        parse_cotree = cotree_hook;
        parse("#PG");
        ckdone();
        if (finam5 & SW_TTYLIST)
            pstats();
        if (finam5 & SW_GPRINT)
            gprint();
        if (finam5 & SW_TPRINT)
            tprint();
        if (finam5 & SW_SEMPER)
            semper();
        if (finam5 & SW_DSPILL)
            dstats();
    }
    if (listing != NULL)
        fclose(listing);
    return (finam5 & SW_ERRORS) ? 1 : 0;
}


/* ---- the compiler: the kernel, then a back end ----------------------------- */

static int
exists(const char *path)
{
    return access(path, X_OK) == 0 || access(path, R_OK) == 0;
}

/* the directory of the running program */
static int
exe_dir(const char *argv0, char *dir, size_t n)
{
    char buf[PATH_MAX], *slash;
    ssize_t k = readlink("/proc/self/exe", buf, sizeof buf - 1);

    if (k > 0)
        buf[k] = '\0';
    else if (strchr(argv0, '/') != NULL) {
        if (realpath(argv0, buf) == NULL)
            return -1;
    } else {                            /* found through PATH */
        const char *p = getenv("PATH");
        char cand[PATH_MAX];

        buf[0] = '\0';
        while (p != NULL && *p) {
            const char *e = strchr(p, ':');
            size_t len = e ? (size_t)(e - p) : strlen(p);

            snprintf(cand, sizeof cand, "%.*s/%s", (int)len, p, argv0);
            if (access(cand, X_OK) == 0 && realpath(cand, buf) != NULL)
                break;
            buf[0] = '\0';
            p = e ? e + 1 : NULL;
        }
        if (buf[0] == '\0')
            return -1;
    }
    slash = strrchr(buf, '/');
    if (slash == NULL)
        return -1;
    *slash = '\0';
    snprintf(dir, n, "%s", buf);
    return 0;
}

static const char *
envor(const char *name, const char *dflt)
{
    const char *v = getenv(name);

    return v != NULL && *v ? v : dflt;
}

/* replace a path by the environment variable's value, if it is set */
static void
override(char *path, size_t n, const char *name)
{
    const char *v = getenv(name);

    if (v != NULL && *v)
        snprintf(path, n, "%s", v);
}

/* run a command line made from a format; quote the file names with qs() */
static char *
qs(const char *s)
{
    static char ring[8][PATH_MAX * 2];
    static int k;
    char *o = ring[k = (k + 1) % 8];
    size_t m = 0;

    o[m++] = '\'';
    for (; *s && m < sizeof ring[0] - 6; s++) {
        if (*s == '\'') {
            memcpy(o + m, "'\\''", 4);
            m += 4;
        } else
            o[m++] = *s;
    }
    o[m++] = '\'';
    o[m] = '\0';
    return o;
}

/*
 * The link: lld in this process (stage0/impld.cpp), the same way as src/driver.imp (LNK) does it for imp72, with the same files and the same arguments for the system and the
 * architecture this is; there is no cc.  IMP72LDSHOW=1 prints the arguments of ld, and a test requires this driver and imp72 to print the same.  This is the C of what
 * LNK, LNKLIN, LNKBSD and LNKMAC are in IMP: change the one, change the other.
 */
extern long impld(long argc, const char **argv);
extern long imprunld(long argc, long argv);
extern long impldhave(long z);
extern const char *imphostos(long z);
extern const char *imphostarch(long z);
extern long impfindlib(long out, long dir, long prefix);
extern long imphostinterp(long out);
extern long imphostlibc(long out);
extern long imphostcrtdir(long out);

static const char *ldv[40];
static int ldn;
static char c1p[4096], c2p[4096], c3p[4096], cbp[4096], cep[4096], lcp[4096], rtp[4096], libd[4096], dlp[4096], mad[4096];

static void
ladd(const char *a)
{
    ldv[ldn++] = a;
}

static int
havef(char *buf, const char *dir, const char *name)
{
    snprintf(buf, 4096, "%s/%s", dir, name);
    return exists(buf);
}

static int
runld(void)
{
    int i;

    ldv[ldn] = NULL;
    if (getenv("IMP72LDSHOW") != NULL) {
        for (i = 0; i < ldn; i++)
            fprintf(stderr, "%s ", ldv[i]);
        fputc('\n', stderr);
    }
    if (!impldhave(0))                          /* no lld library: lld as a program, by the name given (OpenBSD's own linker is lld, and it has no library of it) */
        return imprunld(ldn, (long)(intptr_t)ldv) != 0;
    return impld(ldn, ldv) != 0;
}

/* the Debian multiarch directory, the dynamic linker of glibc and that of musl, by architecture */
static const struct {
    const char *arch, *tri, *dlg, *dlm;
} archtab[] = {
    { "x86_64", "x86_64-linux-gnu", "/lib64/ld-linux-x86-64.so.2", "/lib/ld-musl-x86_64.so.1" },
    { "i386", "i386-linux-gnu", "/lib/ld-linux.so.2", "/lib/ld-musl-i386.so.1" },
    { "arm", "arm-linux-gnueabihf", "/lib/ld-linux-armhf.so.3", "/lib/ld-musl-armhf.so.1" },
    { "aarch64", "aarch64-linux-gnu", "/lib/ld-linux-aarch64.so.1", "/lib/ld-musl-aarch64.so.1" },
    { "ppc64le", "powerpc64le-linux-gnu", "/lib64/ld64.so.2", "/lib/ld-musl-powerpc64le.so.1" },
    { "s390x", "s390x-linux-gnu", "/lib/ld64.so.1", "/lib/ld-musl-s390x.so.1" },
    { "riscv64", "riscv64-linux-gnu", "/lib/ld-linux-riscv64-lp64d.so.1", "/lib/ld-musl-riscv64.so.1" },
    { "loongarch64", "loongarch64-linux-gnu", "/lib64/ld-linux-loongarch-lp64d.so.1", "/lib/ld-musl-loongarch64.so.1" },
};

static int
trydir(const char *d)
{
    snprintf(c1p, sizeof c1p, "%s/crt1.o", d);
    if (!exists(c1p))
        return 0;
    snprintf(c2p, sizeof c2p, "%s/crti.o", d);
    snprintf(c3p, sizeof c3p, "%s/crtn.o", d);
    snprintf(libd, sizeof libd, "%s", d);
    return 1;
}

static int
trydl(const char *p)
{
    if (!exists(p))
        return 0;
    snprintf(dlp, sizeof dlp, "%s", p);
    return 1;
}

/* 9: not a system and an architecture whose files are known */
static int
lnklin(const char *ha, const char *out, const char *ofile, const char *lib)
{
    size_t k;
    int ok = 0, wh;

    if (imphostcrtdir((long)(intptr_t)mad) && imphostinterp((long)(intptr_t)dlp))       /* what this program's own loader found */
        ok = trydir(mad);
    if (!ok) {                                  /* else the table of the architectures (a program with no loader of its own) */
        for (k = 0; k < sizeof archtab / sizeof archtab[0] && strcmp(ha, archtab[k].arch) != 0; k++)
            ;
        if (k == sizeof archtab / sizeof archtab[0])
            return 9;
        snprintf(mad, sizeof mad, "/usr/lib/%s", archtab[k].tri);
        wh = 1;
        ok = trydir(mad);
        if (!ok)
            ok = trydir("/usr/lib64");
        if (!ok) {
            wh = 2;
            ok = trydir("/usr/lib");
        }
        if (!ok)
            return 9;
        if (wh == 1)
            ok = trydl(archtab[k].dlg) || trydl(archtab[k].dlm);
        else
            ok = trydl(archtab[k].dlm) || trydl(archtab[k].dlg);
        if (!ok)
            return 9;
    }
    ladd("ld.lld"); ladd("-o"); ladd(out); ladd("-dynamic-linker"); ladd(dlp);
    ladd(c1p); ladd(c2p); ladd(ofile); ladd(lib); ladd("-L"); ladd(libd); ladd("-lc"); ladd(c3p);
    return runld();
}

static int
lnkbsd(const char *os, const char *ha, const char *out, const char *ofile, const char *lib)
{
    const char *dl = NULL, *fc0 = NULL;
    int wh = 0, obsd = 0;

    if (strcmp(os, "freebsd") == 0) {
        dl = "/libexec/ld-elf.so.1"; fc0 = "crt1.o"; wh = 1;
    } else if (strcmp(os, "netbsd") == 0) {
        dl = "/usr/libexec/ld.elf_so"; fc0 = "crt0.o"; wh = 1;
    } else if (strcmp(os, "openbsd") == 0) {
        dl = "/usr/libexec/ld.so"; fc0 = "crt0.o"; wh = 0; obsd = 1;
    }
    if (dl == NULL || (strcmp(ha, "x86_64") != 0 && strcmp(ha, "aarch64") != 0))
        return 9;
    if (!havef(c1p, "/usr/lib", fc0) || !havef(cbp, "/usr/lib", "crtbegin.o") || !havef(cep, "/usr/lib", "crtend.o"))
        return 9;
    if (wh && (!havef(c2p, "/usr/lib", "crti.o") || !havef(c3p, "/usr/lib", "crtn.o")))
        return 9;
    if (obsd && !imphostlibc((long)(intptr_t)lcp) && !impfindlib((long)(intptr_t)lcp, (long)(intptr_t)"/usr/lib", (long)(intptr_t)"libc.so."))
        return 9;
    if (!imphostinterp((long)(intptr_t)dlp))    /* the loader this program has, else the system's */
        snprintf(dlp, sizeof dlp, "%s", dl);
    havef(rtp, "/usr/lib", "libcompiler_rt.a");
    ladd("ld.lld"); ladd("--eh-frame-hdr"); ladd("-dynamic-linker"); ladd(dlp); ladd("-o"); ladd(out);
    ladd(c1p);
    if (wh)
        ladd(c2p);
    ladd(cbp); ladd(ofile); ladd(lib);
    if (obsd)
        ladd(lcp);
    else {
        ladd("-L"); ladd("/usr/lib"); ladd("-lc");
    }
    if (exists(rtp))
        ladd(rtp);
    ladd(cep);
    if (wh)
        ladd(c3p);
    return runld();
}

static int
lnkmac(const char *ha, const char *out, const char *ofile, const char *lib)
{
    const char *sdk, *ar = NULL, *mv = NULL;

    if (strcmp(ha, "aarch64") == 0) {
        ar = "arm64"; mv = "11.0";
    } else if (strcmp(ha, "x86_64") == 0) {
        ar = "x86_64"; mv = "10.13";
    }
    if (ar == NULL)
        return 9;
    sdk = getenv("SDKROOT");
    if (sdk == NULL || *sdk == '\0') {
        sdk = NULL;
        snprintf(libd, sizeof libd, "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk");
        if (exists(libd))
            sdk = libd;
    }
    if (sdk == NULL) {
        snprintf(libd, sizeof libd, "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk");
        if (exists(libd))
            sdk = libd;
    }
    if (sdk == NULL)
        return 9;
    ladd("ld64.lld"); ladd("-arch"); ladd(ar); ladd("-platform_version"); ladd("macos"); ladd(mv); ladd(mv);
    ladd("-syslibroot"); ladd(sdk); ladd("-o"); ladd(out); ladd(ofile); ladd(lib); ladd("-lSystem");
    return runld();
}

static int
link_exe(const char *out, const char *ofile, const char *lib)
{
    const char *os = imphostos(0), *ha = imphostarch(0);
    int rc;

    ldn = 0;
    if (strcmp(os, "linux") == 0)
        rc = lnklin(ha, out, ofile, lib);
    else if (strcmp(os, "darwin") == 0)
        rc = lnkmac(ha, out, ofile, lib);
    else
        rc = lnkbsd(os, ha, out, ofile, lib);
    if (rc == 9) {
        fprintf(stderr, "imp72stage0: no way to link on this system and architecture: %s %s\n", os, ha);
        return 1;
    }
    return rc;
}

static int
driver_main(int argc, char **argv)
{
    char dir[PATH_MAX], root[PATH_MAX], tmp[PATH_MAX], ir[PATH_MAX + 8],
        ofile[PATH_MAX + 8], cmd[8 * PATH_MAX], base[PATH_MAX];
    char syntax[PATH_MAX], ffi[PATH_MAX], lib[PATH_MAX], tmpbase[PATH_MAX];
    const char *outname = NULL, *sw = NULL;
    int jit = 0;
    int stop = 'x', i = 1, rc = 0, nfiles, k;
    char **kargv;
    size_t blen;

    while (i < argc && argv[i][0] == '-' && argv[i][1] != '\0') {
        if (strcmp(argv[i], "-S") == 0)
            stop = 'S';
        else if (strcmp(argv[i], "-c") == 0)
            stop = 'o';
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            outname = argv[++i];
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
            sw = argv[++i];
        else if (strcmp(argv[i], "--run") == 0)
            jit = 1;
        else {
            fprintf(stderr, "imp72stage0: unknown option %s\n", argv[i]);
            return 2;
        }
        i++;
    }
    if (!llvmhave(0)) {
        fputs("imp72stage0: this compiler was built without the LLVM libraries, which it needs\n", stderr);
        return 2;
    }
    if (i >= argc) {
        fputs("usage: imp72stage0 [-S | -c] [--run] [-o OUT] [-s LETTERS] FILE.imp ...\n", stderr);
        return 2;
    }
    nfiles = argc - i;

    /* where the parts are */
    if (exe_dir(argv[0], dir, sizeof dir) != 0) {
        fputs("imp72stage0: cannot find myself\n", stderr);
        return 1;
    }
    if (strlen(dir) + 4 >= sizeof root)
        return 1;
    strcpy(root, dir);
    strcat(root, "/..");
    snprintf(syntax, sizeof syntax, "%s/decus/syntax.imc", root);
    if (exists(syntax)) {                       /* the source tree */
        snprintf(ffi, sizeof ffi, "%s/lib/ffi.imc", root);
        snprintf(lib, sizeof lib, "%s/build/lib/libimp72.a", root);
        snprintf(tmpbase, sizeof tmpbase, "%s/tmp", root);     /* the tree's own (gitignored) tmp/ */
    } else {                                    /* installed */
        snprintf(syntax, sizeof syntax, "%s/share/imp72/syntax.imc", root);
        snprintf(ffi, sizeof ffi, "%s/share/imp72/ffi.imc", root);
        snprintf(lib, sizeof lib, "%s/lib/imp72/libimp72.a", root);
        snprintf(tmpbase, sizeof tmpbase, ".");                /* installed: where the compiler is run */
    }
    override(syntax, sizeof syntax, "IMP72SYNTAX");
    override(ffi, sizeof ffi, "IMP72FFI");
    override(lib, sizeof lib, "IMP72LIB");
    snprintf(base, sizeof base, "%s", argv[i]);
    blen = strlen(base);
    if (blen > 4 && strcmp(base + blen - 4, ".imp") == 0)
        base[blen - 4] = '\0';

    mkdir(tmpbase, 0777);                       /* TMPDIR, else tmp/ of the tree or the current directory: never /tmp */
    snprintf(tmp, sizeof tmp, "%s/imp72stage0.XXXXXX", envor("TMPDIR", tmpbase));
    if (mkdtemp(tmp) == NULL) {
        perror("mkdtemp");
        return 1;
    }
    snprintf(ir, sizeof ir, "%s/p.ir", tmp);
    snprintf(ofile, sizeof ofile, "%s/p.o", tmp);

    /* the kernel, in this process */
    kargv = calloc((size_t)nfiles + 10, sizeof *kargv);
    k = 0;
    kargv[k++] = "imp72s0";
    if (sw != NULL) {
        kargv[k++] = "-s";
        kargv[k++] = (char *)sw;
        dup2(2, 1);                             /* the dumps go with the messages */
    } else
        imp_quiet = 1;
    kargv[k++] = "-c";
    kargv[k++] = ir;
    kargv[k++] = syntax;
    kargv[k++] = ffi;
    for (; nfiles > 0; nfiles--, i++)
        kargv[k++] = argv[i];
    if (kernel_main(k, kargv) != 0) {
        rc = 1;
        goto done;
    }
    fflush(NULL);

    {                                               /* LLVM in the process */
        if (jit) {                                  /* compile and run: the main program is imp_main */
            rc = llvmrun(ir) != 0;
            fflush(NULL);
        } else if (stop == 'S')
            rc = llvmsave(ir, outname ? outname : strcat(base, ".ll")) != 0;
        else if (stop == 'o')
            rc = llvmobj(ir, outname ? outname : strcat(base, ".o")) != 0;
        else if (llvmobj(ir, ofile) != 0)
            rc = 1;
        else
            rc = link_exe(outname ? outname : base, ofile, lib);
        goto done;
    }
done:
    snprintf(cmd, sizeof cmd, "rm -rf %s", qs(tmp));
    (void)!system(cmd);
    return rc;
}

int
main(int argc, char **argv)
{
    const char *name = strrchr(argv[0], '/');

    name = name ? name + 1 : argv[0];
    if (strncmp(name, "imp72stage", 10) == 0)
        return driver_main(argc, argv);
    return kernel_main(argc, argv);
}
