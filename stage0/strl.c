/*
 * strl.c -- strlcpy.c and strlcat.c of OpenBSD, compiled when the C library has no strlcpy and strlcat (HAVE_STRL is set by the Makefile when tools/hasstrl.sh finds them).
 */
#ifndef HAVE_STRL
#include "strlcpy.c"
#include "strlcat.c"
#else
typedef int imp_strl_unused;
#endif
