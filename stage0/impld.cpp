/*
 * impld.cpp -- lld in the process, for the IMP driver (src/driver.imp calls IMPLD with CCALL).  lld has no C interface, so this is the one C++ file of
 * the tree: it only gives lld's drivers a plain C name.  impld(argc, argv) links as the program named by argv[0] would (ld.lld for ELF, ld64.lld for Mach-O)
 * with that command line and returns its status; impldhave says this compiler has lld (src/llvm.imp has the same two names for a build without it).
 * The drivers of both formats are in, wherever the compiler runs, so that it can link for a system of the other format when it is given that system's files
 * (--sysroot, --runtime): lld links for any target, and each driver for several architectures.
 */
#include <vector>
#include "lld/Common/Driver.h"
#include "llvm/Support/raw_ostream.h"

#ifdef HAVE_LLD_ELF
LLD_HAS_DRIVER(elf)
#endif
#ifdef HAVE_LLD_MACHO
LLD_HAS_DRIVER(macho)
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
    std::vector<lld::DriverDef> drivers;                /* the first argument names the flavour: ld.lld, ld64.lld */

#ifdef HAVE_LLD_ELF
    drivers.push_back({lld::Gnu, &lld::elf::link});
#endif
#ifdef HAVE_LLD_MACHO
    drivers.push_back({lld::Darwin, &lld::macho::link});
#endif
    lld::Result r = lld::lldMain(args, llvm::outs(), llvm::errs(), drivers);

    return r.retCode;
}
