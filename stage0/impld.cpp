/*
 * impld.cpp -- lld in the process, for the IMP driver (src/driver.imp calls IMPLD with CCALL).  lld has no C interface, so this is the one C++ file of
 * the tree: it only gives lld's driver a plain C name.  impld(argc, argv) links as ld.lld would with that command line and returns its status;
 * impldhave says this compiler has it (llvmorc.c has the same two names for a build without lld).
 */
#include <vector>
#include "lld/Common/Driver.h"
#include "llvm/Support/raw_ostream.h"

#ifdef __APPLE__
LLD_HAS_DRIVER(macho)
#else
LLD_HAS_DRIVER(elf)
#endif

extern "C" long
impldhave(long z)
{
    (void)z;
    return 1;
}

extern "C" long
impld(long argc, const char **argv)
{
    std::vector<const char *> args(argv, argv + argc);
#ifdef __APPLE__                                /* the first argument names the flavour: ld64.lld */
    lld::Result r = lld::lldMain(args, llvm::outs(), llvm::errs(), {{lld::Darwin, &lld::macho::link}});
#else
    lld::Result r = lld::lldMain(args, llvm::outs(), llvm::errs(), {{lld::Gnu, &lld::elf::link}});
#endif

    return r.retCode;
}
