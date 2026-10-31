/*
 * llvmorc.c -- the LLVM back end in the process: the module of LLVM IR that ir2ll_run (tools/ir2ll.c) writes becomes an object file with the
 * target machine of the host, or is compiled by ORC (LLJIT) and run, without a clang or a file in between.  Built when the LLVM C API is there
 * (-DHAVE_LLVM; tools/direct.sh looks for llvm-config).  The runtime library and the C library are found in the program itself, which is linked
 * with -rdynamic for the purpose.
 */
#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_LLVM
#include <llvm-c/Core.h>
#include <llvm-c/Error.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/LLJIT.h>
#include <llvm-c/Orc.h>
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
        fputs("imp72: LLVM has no code generator for this machine\n", stderr);
        return 1;
    }
    ninit = 1;
    return 0;
}

static int
error(const char *what, LLVMErrorRef e)
{
    char *m = LLVMGetErrorMessage(e);

    fprintf(stderr, "imp72: LLVM: %s: %s\n", what, m);
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
        fprintf(stderr, "imp72: LLVM: the module does not parse: %s\n", msg != NULL ? msg : "");
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
    if (getenv("IMP72LLVMINFO") != NULL)                            /* what the host says (tools/ci-host.sh records it in every CI run) */
        fprintf(stderr, "imp72-host: triple=%s cpu=%s features=%s\n", triple, *cpu, *feat);
    if (strncmp(triple, "riscv64", 7) != 0)
        return 0;
    if (strstr(*feat, "+d") == NULL) {
        *cpu = "generic-rv64";
        *feat = "+m,+a,+f,+d,+c";
    } else {
        char *f = *feat, *t, *o = malloc(strlen(f) + 1);

        if (o != NULL) {
            *o = '\0';
            for (t = strtok(f, ","); t != NULL; t = strtok(NULL, ","))
                if (*t != '-') {
                    if (*o != '\0')
                        strcat(o, ",");
                    strcat(o, t);
                }
            *feat = o;
        }
    }
    return 1;
}

/* the module m for the host, optimised, as an object file */
int
llvm_object(const char *ir, size_t n, const char *path)
{
    LLVMContextRef ctx;
    LLVMModuleRef m;
    LLVMTargetRef tgt;
    LLVMTargetMachineRef tm;
    LLVMTargetDataRef dl;
    LLVMPassBuilderOptionsRef pbo;
    LLVMErrorRef e;
    char *triple, *cpu, *feat, *msg = NULL;
    int rc = 1;

    if (init())
        return 1;
    ctx = LLVMContextCreate();
    if ((m = parse(ctx, ir, n)) == NULL)
        goto done;
    triple = LLVMGetDefaultTargetTriple();
    hostcpu(triple, &cpu, &feat);
    if (LLVMGetTargetFromTriple(triple, &tgt, &msg)) {
        fprintf(stderr, "imp72: LLVM: %s\n", msg);
        LLVMDisposeMessage(msg);
        goto done;
    }
    tm = LLVMCreateTargetMachine(tgt, triple, cpu, feat, LLVMCodeGenLevelDefault, LLVMRelocPIC, LLVMCodeModelDefault);
    LLVMSetTarget(m, triple);
    dl = LLVMCreateTargetDataLayout(tm);
    LLVMSetModuleDataLayout(m, dl);
    pbo = LLVMCreatePassBuilderOptions();
    if ((e = LLVMRunPasses(m, "default<O1>", tm, pbo)) != NULL) {
        error("the optimiser", e);
        goto done;
    }
    LLVMDisposePassBuilderOptions(pbo);
    if (LLVMTargetMachineEmitToFile(tm, m, (char *)path, LLVMObjectFile, &msg)) {
        fprintf(stderr, "imp72: LLVM: %s: %s\n", path, msg);
        LLVMDisposeMessage(msg);
        goto done;
    }
    rc = 0;
done:
    return rc;
}

/* the module compiled by ORC and its function entry (no arguments) called; the result of the call in *result */
int
llvm_run(const char *ir, size_t n, const char *entry, long *result)
{
    LLVMOrcLLJITRef jit;
    LLVMOrcJITDylibRef jd;
    LLVMOrcDefinitionGeneratorRef gen;
    LLVMOrcThreadSafeContextRef tsc;
    LLVMOrcThreadSafeModuleRef tsm;
    LLVMOrcExecutorAddress addr;
    LLVMModuleRef m;
    LLVMErrorRef e;

    if (init())
        return 1;
    {
        LLVMOrcLLJITBuilderRef lb = NULL;
        char *triple = LLVMGetDefaultTargetTriple(), *cpu, *feat;
        LLVMTargetRef tgt;
        char *msg = NULL;

        if (hostcpu(triple, &cpu, &feat) && !LLVMGetTargetFromTriple(triple, &tgt, &msg)) {      /* the JIT detects the host itself, and would find no +d */
            LLVMTargetMachineRef tm = LLVMCreateTargetMachine(tgt, triple, cpu, feat, LLVMCodeGenLevelDefault, LLVMRelocPIC, LLVMCodeModelJITDefault);

            lb = LLVMOrcCreateLLJITBuilder();
            LLVMOrcLLJITBuilderSetJITTargetMachineBuilder(lb, LLVMOrcJITTargetMachineBuilderCreateFromTargetMachine(tm));
        }
        if ((e = LLVMOrcCreateLLJIT(&jit, lb)) != NULL)
            return error("the JIT", e);
    }
    jd = LLVMOrcLLJITGetMainJITDylib(jit);
    if ((e = LLVMOrcCreateDynamicLibrarySearchGeneratorForProcess(&gen, LLVMOrcLLJITGetGlobalPrefix(jit), NULL, NULL)) != NULL)
        return error("the symbols of the program", e);
    LLVMOrcJITDylibAddGenerator(jd, gen);
#ifdef HAVE_ORC_FROM_CONTEXT                                      /* newer LLVM: the context is made first, then given to ORC */
    {
        LLVMContextRef ctx = LLVMContextCreate();

        if ((m = parse(ctx, ir, n)) == NULL)
            return 1;
        tsc = LLVMOrcCreateNewThreadSafeContextFromLLVMContext(ctx);
    }
#else
    tsc = LLVMOrcCreateNewThreadSafeContext();
    if ((m = parse(LLVMOrcThreadSafeContextGetContext(tsc), ir, n)) == NULL)
        return 1;
#endif
    tsm = LLVMOrcCreateNewThreadSafeModule(m, tsc);
    LLVMOrcDisposeThreadSafeContext(tsc);
    if ((e = LLVMOrcLLJITAddLLVMIRModule(jit, jd, tsm)) != NULL)
        return error("adding the module", e);
    if ((e = LLVMOrcLLJITLookup(jit, &addr, entry)) != NULL)
        return error(entry, e);
    *result = ((long (*)(void))(size_t)addr)();
    return 0;
}

extern int ir2ll_run(FILE *in, FILE *out, int p32);                 /* tools/ir2ll.c */

/* the module of LLVM IR for the IR in the file ir, as text in *buf (n bytes) */
static int
module(const char *ir, char **buf, size_t *n)
{
    FILE *irf = fopen(ir, "r"), *mem;
    int rc;

    if (irf == NULL || (mem = open_memstream(buf, n)) == NULL) {
        perror(ir);
        return 1;
    }
    rc = ir2ll_run(irf, mem, (int)(sizeof(void *) == 4));
    fclose(mem);
    fclose(irf);
    return rc;
}

/* The entries of the driver (src/driver.imp), which calls them with CCALL, so no underscores: 0 for success. */
long llvmhave(long z) { (void)z; return 1; }

long
llvmobj(const char *ir, const char *out)                            /* the object of the program */
{
    char *buf = NULL;
    size_t n = 0;

    return module(ir, &buf, &n) != 0 || llvm_object(buf, n, out) != 0;
}

long
llvmsave(const char *ir, const char *out)                           /* the module as text (-S) */
{
    char *buf = NULL;
    size_t n = 0;
    FILE *o;

    if (module(ir, &buf, &n) != 0)
        return 1;
    return (o = fopen(out, "w")) == NULL || fwrite(buf, 1, n, o) != n || fclose(o) != 0;
}

long
llvmrun(const char *ir)                                             /* compile with ORC and run: the main program is impmain */
{
    char *buf = NULL;
    size_t n = 0;
    long r = 0;

    return module(ir, &buf, &n) != 0 || llvm_run(buf, n, "impmain", &r) != 0;
}

#else
long llvmhave(long z) { (void)z; return 0; }
long llvmobj(const char *ir, const char *out) { (void)ir; (void)out; return 1; }
long llvmsave(const char *ir, const char *out) { (void)ir; (void)out; return 1; }
long llvmrun(const char *ir) { (void)ir; return 1; }
#endif

/* lld in the process is stage0/impld.cpp, built where lld is installed (-DHAVE_LLD, tools/llvm.sh); without it the driver links with the system's cc */
#ifndef HAVE_LLD
long impldhave(long z) { (void)z; return 0; }
long impld(long argc, const char **argv) { (void)argc; (void)argv; return 1; }
#endif
