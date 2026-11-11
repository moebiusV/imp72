/*
 * lvtriple.cpp -- the check of a --target triple by LLVM's own parser (src/llvm.imc: LVTRIOK calls imp_triple_check with CCALL).  The C API cannot be asked which
 * operating systems, environments and object formats llvm::Triple knows, so this file, with impld.cpp the only C++ of the tree, asks the class: the set of names
 * follows the installed LLVM.  Whether this LLVM has a code generator for the architecture is the C API's business (LLVMGetTargetFromTriple); the architecture is
 * looked at here too, so that the message names the part that is wrong.  The vendor is free text and is not looked at.
 *
 * imp_triple_check(raw, norm, why, whylen): raw is what the user wrote, norm is LLVMNormalizeTargetTriple(raw).  Returns 0 if the triple is acceptable, else 1 and a
 * short reason in why.  Where the LLVM C++ headers are not installed (a system LLVM with the C API headers alone) the same function only checks the characters.
 */
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if defined(__has_include)
#  if __has_include("llvm/TargetParser/Triple.h")
#    include "llvm/TargetParser/Triple.h"
#    define IMP_HAVE_TRIPLE 1
#  elif __has_include("llvm/ADT/Triple.h")
#    include "llvm/ADT/Triple.h"
#    define IMP_HAVE_TRIPLE 1
#  endif
#endif

static int charsok(const char *s)
{
    return std::strspn(s, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-") == std::strlen(s);
}

static void reason(char *why, int whylen, const char *fmt, const std::string &a)
{
    if (whylen > 0) std::snprintf(why, (size_t)whylen, fmt, a.c_str());
}

#ifdef IMP_HAVE_TRIPLE

static std::vector<std::string> parts(const std::string &s)
{
    std::vector<std::string> v;
    std::string c;
    for (size_t i = 0; i <= s.size(); i++) {
        if (i == s.size() || s[i] == '-') { v.push_back(c); c.clear(); }
        else c += s[i];
    }
    return v;
}

/* is C, as a part after the operating system, an environment or an object format that LLVM knows: the environment is parsed after the OS; the format is
   parsed in the fifth place whatever the OS, so a name it knows gives the same format for an OS whose default is ELF and for one whose default is COFF,
   while a name it does not know leaves each default */
static bool knownenv(const std::string &c)
{
    return llvm::Triple("x86_64-pc-linux-" + c).getEnvironment() != llvm::Triple::UnknownEnvironment;
}

static bool knownformat(const std::string &c)
{
    return llvm::Triple("x86_64-pc-linux-gnu-" + c).getObjectFormat() == llvm::Triple("x86_64-pc-windows-msvc-" + c).getObjectFormat();
}

extern "C" int imp_triple_check(const char *raw, const char *norm, char *why, int whylen)
{
    if (!charsok(raw) || !charsok(norm)) { reason(why, whylen, "%s", "characters a triple cannot have"); return 1; }
    llvm::Triple T{std::string(norm)};
    std::vector<std::string> n = parts(norm), r = parts(raw);
    if (T.getArch() == llvm::Triple::UnknownArch) { reason(why, whylen, "unknown architecture \"%s\"", n[0]); return 1; }
    if (n.size() < 3) { reason(why, whylen, "%s", "no operating system"); return 1; }
    if (T.getOS() == llvm::Triple::UnknownOS) {
        /* unknown and none are systems when the user wrote them (normalisation writes unknown for a part that was left out) */
        bool typed = false;
        if (n[2] == "unknown" || n[2] == "none") {
            typed = r.size() >= 3;
            for (size_t i = 0; i < r.size(); i++) if (i != 1 && r[i].empty()) typed = false;
            bool seen = false;
            for (size_t i = 1; i < r.size(); i++) if (r[i] == n[2]) seen = true;
            typed = typed && seen;
            /* a bare-metal triple (arm-none-eabi, thumbv7em-none-eabi, riscv32-unknown-elf) has no system: normalisation writes unknown, and the environment or the object format says what it is */
            if (!typed && n.size() >= 4 && (knownenv(n[3]) || knownformat(n[3]))) typed = true;
        }
        if (!typed) { reason(why, whylen, "unknown operating system \"%s\"", n[2]); return 1; }
    }
    for (size_t i = 3; i < n.size() && i < 5; i++) {
        const std::string &c = n[i];
        if (c == "unknown" || c == "none") continue;
        if (!knownenv(c) && !knownformat(c)) { reason(why, whylen, "unknown environment or object format \"%s\"", c); return 1; }
    }
    if (n.size() > 5) { reason(why, whylen, "too many parts after \"%s\"", n[4]); return 1; }
    return 0;
}

#else

extern "C" int imp_triple_check(const char *raw, const char *norm, char *why, int whylen)
{
    if (!charsok(raw) || !charsok(norm)) { reason(why, whylen, "%s", "characters a triple cannot have"); return 1; }
    return 0;
}

#endif
