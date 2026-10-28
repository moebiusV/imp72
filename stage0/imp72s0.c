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
 * the pcc IR of the program to a file (- is standard output); cc1 of pcc
 * turns it into assembly.  -T names the machine the IR is lowered for (amd64,
 * the default, or aarch64): the registers of the procedure call standard.
 *
 * The same program is also the compiler, imp72stage0 (a link to it, or a
 * copy: the name decides):
 *
 *     imp72stage0 [-C] [-S | -c] [-o OUT] [-s LETTERS] FILE.imp ...
 *
 * It reads the syntax of the language (syntax.imc) and the extension ffi.imc,
 * compiles the files with the kernel above to pcc IR, and makes the IR into
 * an executable (default), an object (-c) or assembly (-S) with pcc's cc1, or
 * with -C into C for the host's compiler (-S then leaves the C); where cc1 was
 * not built the C back end is the only one.  It finds its parts relative to
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

static int
run(const char *cmd)
{
    return system(cmd) == 0 ? 0 : 1;
}

/* The optimizations of pass 2 of pcc, all on for the compiler with the code generator of src/pccir (IMP72CC1FLAGS= with nothing turns them off); the seed's
 * translator keeps its doubles in temps, which -xtemps and -xssa mishandle on the i386's x87 stack, so it has the rest. */
#define CC1OPT_SEED "-xdeljumps -xdce -xccp -xscp -xtailcall"


static int
driver_main(int argc, char **argv)
{
    char dir[PATH_MAX], root[PATH_MAX], tmp[PATH_MAX], ir[PATH_MAX + 8], cfile[PATH_MAX + 8],
        sfile[PATH_MAX + 8], ofile[PATH_MAX + 8], cmd[8 * PATH_MAX], base[PATH_MAX];
    char syntax[PATH_MAX], ffi[PATH_MAX], cc1[PATH_MAX], ir2c[PATH_MAX], lib[PATH_MAX];
    const char *outname = NULL, *sw = NULL, *backend, *bname = NULL;
    int jit = 0;
    int stop = 'x', cback = 0, i = 1, rc = 0, nfiles, k;
    char **kargv;
    size_t blen;

    while (i < argc && argv[i][0] == '-' && argv[i][1] != '\0') {
        if (strcmp(argv[i], "-C") == 0)
            cback = 1;
        else if (strcmp(argv[i], "-S") == 0)
            stop = 'S';
        else if (strcmp(argv[i], "-c") == 0)
            stop = 'o';
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            outname = argv[++i];
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
            sw = argv[++i];
        else if (strncmp(argv[i], "--backend=", 10) == 0)
            bname = argv[i] + 10;
        else if (strcmp(argv[i], "--run") == 0)
            jit = 1;
        else {
            fprintf(stderr, "imp72stage0: unknown option %s\n", argv[i]);
            return 2;
        }
        i++;
    }
    if (jit && !llvmhave(0)) {
        fputs("imp72stage0: --run needs the LLVM libraries (this compiler was built without them)\n", stderr);
        return 2;
    }
    if (i >= argc) {
        fputs("usage: imp72stage0 [-C] [-S | -c] [-o OUT] [-s LETTERS] FILE.imp ...\n", stderr);
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
        snprintf(cc1, sizeof cc1, "%s/build/pcc/cc/ccom/cc1", root);
        snprintf(ir2c, sizeof ir2c, "%s/build/bin/ir2c", root);
        snprintf(lib, sizeof lib, "%s/build/lib/libimp72.a", root);
    } else {                                    /* installed */
        snprintf(syntax, sizeof syntax, "%s/share/imp72/syntax.imc", root);
        snprintf(ffi, sizeof ffi, "%s/share/imp72/ffi.imc", root);
        snprintf(cc1, sizeof cc1, "%s/lib/imp72/cc1", root);
        snprintf(ir2c, sizeof ir2c, "%s/lib/imp72/ir2c", root);
        snprintf(lib, sizeof lib, "%s/lib/imp72/libimp72.a", root);
    }
    override(syntax, sizeof syntax, "IMP72SYNTAX");
    override(ffi, sizeof ffi, "IMP72FFI");
    override(cc1, sizeof cc1, "CC1");
    override(ir2c, sizeof ir2c, "IR2C");
    override(lib, sizeof lib, "IMP72LIB");
    /* the back end: --backend=NAME or IMP72BACKEND (llvm, c, pcc); else LLVM, which is in the process (no file, no other program but the linker) where this
     * compiler was built with the LLVM libraries, else the C one, else pcc where cc1 was built */
    backend = envor("IMP72BACKEND", "");
    if (*backend == '\0')
        backend = llvmhave(0) ? "llvm" : exists(ir2c) ? "c" : exists(cc1) ? "pcc" : "c";
    if (bname != NULL)
        backend = bname;
    if (jit)
        backend = "llvm";
    if (cback)
        backend = "c";
    if (strcmp(backend, "pcc") != 0 && strcmp(backend, "c") != 0 && strcmp(backend, "llvm") != 0) {
        fprintf(stderr, "imp72: no back end %s (pcc, c or llvm)\n", backend);
        return 2;
    }
    if (strcmp(backend, "llvm") == 0 && !llvmhave(0)) {
        fputs("imp72: this compiler was built without the LLVM libraries: --backend=c (or pcc)\n", stderr);
        return 2;
    }
    if (strcmp(backend, "pcc") == 0 && !exists(cc1)) {
        fprintf(stderr, "imp72: no pass 2 of pcc (%s): --backend=c or --backend=llvm\n", cc1);
        return 1;
    }

    snprintf(base, sizeof base, "%s", argv[i]);
    blen = strlen(base);
    if (blen > 4 && strcmp(base + blen - 4, ".imp") == 0)
        base[blen - 4] = '\0';

    snprintf(tmp, sizeof tmp, "%s/imp72stage0.XXXXXX", envor("TMPDIR", "/tmp"));
    if (mkdtemp(tmp) == NULL) {
        perror("mkdtemp");
        return 1;
    }
    snprintf(ir, sizeof ir, "%s/p.ir", tmp);
    snprintf(cfile, sizeof cfile, "%s/p.c", tmp);
    snprintf(sfile, sizeof sfile, "%s/p.s", tmp);
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
    if (strcmp(backend, "pcc") == 0) {          /* cc1 of the machine we are on */
#if defined(__aarch64__) || defined(__arm64__)
        kargv[k++] = "-T";
        kargv[k++] = "aarch64";
#elif defined(__i386__)
        kargv[k++] = "-T";
        kargv[k++] = "i386";
#elif defined(__arm__)
        kargv[k++] = "-T";
        kargv[k++] = "arm";
#endif
    }
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

    if (strcmp(backend, "c") == 0) {
        snprintf(cmd, sizeof cmd, "%s < %s > %s", qs(ir2c), qs(ir), qs(cfile));
        if (run(cmd)) { rc = 1; goto done; }
        if (stop == 'S') {
            snprintf(cmd, sizeof cmd, "cp %s %s", qs(cfile), qs(outname ? outname : strcat(base, ".c")));
        } else if (stop == 'o') {
            snprintf(cmd, sizeof cmd, "cc -std=gnu99 -w %s -c -o %s %s", envor("IMP_CFLAGS", ""),
                qs(outname ? outname : strcat(base, ".o")), qs(cfile));
        } else {
            snprintf(cmd, sizeof cmd, "cc -std=gnu99 -w %s -c -o %s %s && cc -o %s %s %s", envor("IMP_CFLAGS", ""),
                qs(ofile), qs(cfile), qs(outname ? outname : base), qs(ofile), qs(lib));
        }
        rc = run(cmd);
        goto done;
    }
    if (strcmp(backend, "llvm") == 0) {             /* LLVM in the process */
        if (jit) {                                  /* compile and run: the main program is imp_main */
            rc = llvmrun(ir) != 0;
            fflush(NULL);
        } else if (stop == 'S')
            rc = llvmsave(ir, outname ? outname : strcat(base, ".ll")) != 0;
        else if (stop == 'o')
            rc = llvmobj(ir, outname ? outname : strcat(base, ".o")) != 0;
        else if (llvmobj(ir, ofile) != 0)
            rc = 1;
        else {
            snprintf(cmd, sizeof cmd, "cc -o %s %s %s", qs(outname ? outname : base), qs(ofile), qs(lib));
            rc = run(cmd);
        }
        goto done;
    }
    snprintf(cmd, sizeof cmd, "%s %s < %s > %s", qs(cc1), (getenv("IMP72CC1FLAGS") != NULL ? getenv("IMP72CC1FLAGS") : CC1OPT_SEED), qs(ir), qs(sfile));
    if (run(cmd)) { rc = 1; goto done; }
    if (stop == 'S')
        snprintf(cmd, sizeof cmd, "cp %s %s", qs(sfile), qs(outname ? outname : strcat(base, ".s")));
    else if (stop == 'o')
        snprintf(cmd, sizeof cmd, "cc -c -o %s -x assembler %s", qs(outname ? outname : strcat(base, ".o")), qs(sfile));
    else
        snprintf(cmd, sizeof cmd, "cc -c -o %s -x assembler %s && cc -no-pie -o %s %s %s", qs(ofile), qs(sfile),
            qs(outname ? outname : base), qs(ofile), qs(lib));
    rc = run(cmd);
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
