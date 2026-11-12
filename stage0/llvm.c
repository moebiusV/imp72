/*
 * llvm.c -- the LLVM back end in the process (the seed's; src/llvm.imc is the same in IMP): the module of LLVM IR that ir2ll_run (tools/ir2ll.c)
 * writes becomes an object file, assembly or IR text, with the target machine of the host or of --target, without a clang or a file in between.
 * Built when the LLVM C API is there (-DHAVE_LLVM; tools/direct.sh looks for llvm-config).
 */
#define _GNU_SOURCE 1
#include "strl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_LLVM
#include <llvm-c/Core.h>
#include <llvm-c/Error.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>

static int ninit;

static int
init(void)
{
    if (ninit)
        return 0;
    if (LLVMInitializeNativeTarget() || LLVMInitializeNativeAsmPrinter() || LLVMInitializeNativeAsmParser()) {
        fputs("imp: LLVM has no code generator for this machine\n", stderr);
        return 1;
    }
    ninit = 1;
    return 0;
}

static int
error(const char *what, LLVMErrorRef e)
{
    char *m = LLVMGetErrorMessage(e);

    fprintf(stderr, "imp: LLVM: %s: %s\n", what, m);
    LLVMDisposeErrorMessage(m);
    return 1;
}

/* the module of the text ir (n bytes) in context ctx */
static LLVMModuleRef
parse(LLVMContextRef ctx, const char *ir, size_t n)
{
    LLVMMemoryBufferRef mb = LLVMCreateMemoryBufferWithMemoryRangeCopy(ir, n, "module");
    LLVMModuleRef m = NULL;
    char *msg = NULL;

    if (LLVMParseIRInContext(ctx, mb, &m, &msg)) {                  /* (the buffer is consumed) */
        fprintf(stderr, "imp: LLVM: the module does not parse: %s\n", msg != NULL ? msg : "");
        LLVMDisposeMessage(msg);
        return NULL;
    }
    return m;
}

/* the processor and features of the host; 1 if they are not LLVM's own to take.  riscv64 under an emulator does not say what it has (no +d), and then the code would be soft
 * float: rv64gc, what every Linux distribution is built for.  And where it does say, the list has features that LLVM does not know ("-zicfiss"), each a warning on the
 * standard error every time a target machine is made from it: the ones that only switch a feature off are left out. */
static int
hostcpu(const char *triple, char **cpu, char **feat)
{
    *cpu = LLVMGetHostCPUName();
    *feat = LLVMGetHostCPUFeatures();
    if (getenv("IMPLLVMINFO") != NULL)                            /* what the host says (tools/ci-host.sh records it in every CI run) */
        fprintf(stderr, "imp-host: triple=%s cpu=%s features=%s\n", triple, *cpu, *feat);
    if (strncmp(triple, "riscv64", 7) != 0)
        return 0;
    if (strstr(*feat, "+d") == NULL) {
        *cpu = "generic-rv64";
        *feat = "+m,+a,+f,+d,+c";
    } else {
        char *f = *feat, *t, *o = malloc(strlen(f) + 1);
        size_t osz = strlen(f) + 1;

        if (o != NULL) {
            *o = '\0';
            for (t = strtok(f, ","); t != NULL; t = strtok(NULL, ","))
                if (*t != '-') {
                    if (*o != '\0')
                        strlcat(o, ",", osz);
                    strlcat(o, t, osz);
                }
            *feat = o;
        }
    }
    return 1;
}

/* the optimization level: 0 to 3 for -O0 to -O3, 4 for -Os (the default), 5 for -Oz.  The seed has no flag: IMPOPT in the environment (-O0 ... -Oz, or the letter alone)
 * sets it, as it does for imp (src/driver.imc), so that a test can run the whole suite at another level */
static int
optlevel(void)
{
    const char *e = getenv("IMPOPT");

    if (e == NULL || *e == '\0')
        return 4;
    if (e[0] == '-' && e[1] == 'O')
        e += 2;
    if (*e == '\0')
        return 1;
    if (e[1] == '\0') {
        if (*e >= '0' && *e <= '3')
            return *e - '0';
        if (*e == 's')
            return 4;
        if (*e == 'z')
            return 5;
    }
    return 4;
}

/* -Os and -Oz: each function the module defines gets the attribute optsize (-Oz: and minsize), which the passes and the code generator read */
static void
sizeattr(LLVMContextRef ctx, LLVMModuleRef m, int level)
{
    LLVMAttributeRef a = LLVMCreateEnumAttribute(ctx, LLVMGetEnumAttributeKindForName("optsize", 7), 0);
    LLVMAttributeRef b = LLVMCreateEnumAttribute(ctx, LLVMGetEnumAttributeKindForName("minsize", 7), 0);
    LLVMValueRef f;

    for (f = LLVMGetFirstFunction(m); f != NULL; f = LLVMGetNextFunction(f))
        if (LLVMCountBasicBlocks(f) != 0) {
            LLVMAddAttributeAtIndex(f, LLVMAttributeFunctionIndex, a);
            if (level == 5)
                LLVMAddAttributeAtIndex(f, LLVMAttributeFunctionIndex, b);
        }
}

/* the module of the text ir (n bytes) for the host, optimised, as an object file: in the file path, or in memory (path NULL) at *obj, *objn (malloc) */
static int
object(const char *ir, size_t n, const char *path, char **obj, size_t *objn, LLVMCodeGenFileType ft)
{
    LLVMContextRef ctx;
    LLVMModuleRef m;
    LLVMTargetRef tgt;
    LLVMTargetMachineRef tm;
    LLVMTargetDataRef dl;
    LLVMPassBuilderOptionsRef pbo;
    LLVMErrorRef e;
    LLVMMemoryBufferRef mb;
    char *triple, *cpu, *feat, *msg = NULL;
    int rc = 1;
    int level = optlevel();
    static const char *const pipeline[] = { "default<O0>", "default<O1>", "default<O2>", "default<O3>", "default<O2>", "default<O2>" };   /* -Os and -Oz: O2 with the optsize and minsize attributes: LLVM 23 has no default<Os> or default<Oz> */

    if (init())
        return 1;
    ctx = LLVMContextCreate();
    if ((m = parse(ctx, ir, n)) == NULL)
        goto done;
    triple = LLVMNormalizeTargetTriple(LLVMGetDefaultTargetTriple());
    hostcpu(triple, &cpu, &feat);
    if (LLVMGetTargetFromTriple(triple, &tgt, &msg)) {
        fprintf(stderr, "imp: LLVM: %s\n", msg);
        LLVMDisposeMessage(msg);
        goto done;
    }
    tm = LLVMCreateTargetMachine(tgt, triple, cpu, feat, level < 4 ? (LLVMCodeGenOptLevel)level : LLVMCodeGenLevelDefault, LLVMRelocPIC, LLVMCodeModelDefault);
    LLVMSetTarget(m, triple);
    dl = LLVMCreateTargetDataLayout(tm);
    LLVMSetModuleDataLayout(m, dl);
    if (level > 3)
        sizeattr(ctx, m, level);
    pbo = LLVMCreatePassBuilderOptions();
    if ((e = LLVMRunPasses(m, pipeline[level], tm, pbo)) != NULL) {
        error("the optimiser", e);
        goto done;
    }
    LLVMDisposePassBuilderOptions(pbo);
    if (path != NULL) {
        if (LLVMTargetMachineEmitToFile(tm, m, (char *)path, ft, &msg)) {
            fprintf(stderr, "imp: LLVM: %s: %s\n", path, msg);
            LLVMDisposeMessage(msg);
            goto done;
        }
    } else {
        if (LLVMTargetMachineEmitToMemoryBuffer(tm, m, ft, &msg, &mb)) {
            fprintf(stderr, "imp: LLVM: %s\n", msg);
            LLVMDisposeMessage(msg);
            goto done;
        }
        *objn = LLVMGetBufferSize(mb);
        if ((*obj = malloc(*objn + 1)) == NULL) {
            LLVMDisposeMemoryBuffer(mb);
            goto done;
        }
        memcpy(*obj, LLVMGetBufferStart(mb), *objn);
        LLVMDisposeMemoryBuffer(mb);
    }
    rc = 0;
done:
    return rc;
}

extern int ir2ll_run(FILE *in, FILE *out, int p32);                 /* tools/ir2ll.c */

/* the module of LLVM IR for the IR text at ir (n bytes), as text in *buf (n bytes at *len), all in memory */
static int
module(const char *ir, size_t n, char **buf, size_t *len)
{
    FILE *irf, *mem;
    int rc;

    irf = n > 0 ? fmemopen((void *)ir, n, "r") : fopen("/dev/null", "r");
    if (irf == NULL || (mem = open_memstream(buf, len)) == NULL) {
        perror("imp: the IR in memory");
        return 1;
    }
    rc = ir2ll_run(irf, mem, (int)(sizeof(void *) == 4));
    fclose(mem);
    fclose(irf);
    return rc;
}

/* The entries of the driver (src/driver.imc), which calls them with CCALL, so no underscores: 0 for success.  The IR text of the kernel is in memory (ir, n bytes). */
long llvmhave(long z) { (void)z; return 1; }

long
llvmobj(const char *ir, size_t n, const char *out)                  /* the object of the program, in the file out (-c) */
{
    char *buf = NULL;
    size_t len = 0;

    return module(ir, n, &buf, &len) != 0 || object(buf, len, out, NULL, NULL, LLVMObjectFile) != 0;
}

long
llvmasm(const char *ir, size_t n, const char *out)                  /* the assembly of the target, in the file out (-S) */
{
    char *buf = NULL;
    size_t len = 0;

    return module(ir, n, &buf, &len) != 0 || object(buf, len, out, NULL, NULL, LLVMAssemblyFile) != 0;
}

long
llvmobjmem(const char *ir, size_t n, char **obj, size_t *objn)      /* the object of the program, in memory (malloc) */
{
    char *buf = NULL;
    size_t len = 0;

    return module(ir, n, &buf, &len) != 0 || object(buf, len, NULL, obj, objn, LLVMObjectFile) != 0;
}

long
llvmsave(const char *ir, size_t n, const char *out)                 /* the module as LLVM IR text (-Sir) */
{
    char *buf = NULL;
    size_t len = 0;
    FILE *o;

    if (module(ir, n, &buf, &len) != 0)
        return 1;
    return (o = fopen(out, "w")) == NULL || fwrite(buf, 1, len, o) != len || fclose(o) != 0;
}

#else
long llvmhave(long z) { (void)z; return 0; }
long llvmobj(const char *ir, size_t n, const char *out) { (void)ir; (void)n; (void)out; return 1; }
long llvmobjmem(const char *ir, size_t n, char **obj, size_t *objn) { (void)ir; (void)n; (void)obj; (void)objn; return 1; }
long llvmsave(const char *ir, size_t n, const char *out) { (void)ir; (void)n; (void)out; return 1; }
long llvmasm(const char *ir, size_t n, const char *out) { (void)ir; (void)n; (void)out; return 1; }
#endif

/* lld in the process is stage0/impld.cpp, built where lld is installed (-DHAVE_LLD, tools/llvm.sh); without it the driver links with the system's cc */
#ifndef HAVE_LLD
long impldhave(long z) { (void)z; return 0; }
long impld(long argc, const char **argv) { (void)argc; (void)argv; return 1; }
#endif
