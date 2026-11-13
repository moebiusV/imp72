/* winport.h -- what the seed needs of Windows where the C library has no POSIX (stage0/imphost-win.c): memory files, the program's own path */
#ifndef WINPORT_H
#define WINPORT_H
#include <stddef.h>
#include <stdio.h>

FILE *winmemfile(void);                         /* an anonymous temporary file for reading and writing, gone when it is closed (open_memstream and fmemopen are not Windows') */
FILE *winmemfrom(const char *p, size_t n);      /* such a file holding the n bytes at p, rewound for reading */
int winmemslurp(FILE *f, char **buf, size_t *len);      /* what was written to it: *buf (malloc, and a NUL after the *len bytes); 0 for success; the file is closed */
FILE *winopen_memstream(char **buf, size_t *len);      /* open_memstream: a file whose bytes are in *buf (malloc, a NUL after the *len bytes) once it is closed with winfclose */
int winfclose(FILE *f);                         /* fclose, which for such a file first makes *buf and *len what was written */
int winexepath(char *out, size_t n);            /* the path of this program, with / for \; 0 for success */
#endif
