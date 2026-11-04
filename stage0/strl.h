/*
 * strl.h -- strlcpy and strlcat: the C library's where it has them (the BSDs, macOS, musl, glibc 2.38 and later), else those of strlcpy.c and strlcat.c
 * (OpenBSD's, ISC licence), which strl.c compiles into libstage0.a.  The parentheses keep a macro of the same name (a fortified string.h) from expanding.
 */
#ifndef STRL_H
#define STRL_H
#include <stddef.h>

size_t (strlcpy)(char *, const char *, size_t);
size_t (strlcat)(char *, const char *, size_t);

#endif
