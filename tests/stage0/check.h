/* Minimal test harness shared by the stage 0 unit tests. */
#ifndef CHECK_H
#define CHECK_H
#include <stdio.h>
#include <stdlib.h>
static int check_failed;
#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "%s:%d: FAILED: %s\n", __FILE__, __LINE__, #c); \
    check_failed = 1; } } while (0)
#define DONE() do { if (check_failed) return 1; puts("ok"); return 0; } while (0)
#endif
