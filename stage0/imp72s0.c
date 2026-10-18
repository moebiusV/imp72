/*
 * imp72s0.c -- the stage 0 driver.  The part of decus/impm.imc that matters
 * here: read the bootstrap syntax with RSYN, then compile the rest of the
 * file, and any other files named, with the syntax built so far.
 *
 *     imp72s0 [-s LETTERS] [-o FILE] [-c IRFILE] SYNTAX [FILE ...]
 *
 * -s sets compiler switches, each letter of LETTERS the switch of that name
 * (the original's /A /B ... on the command line); the ones that mean
 * something here are G (trace the parser), I (print the directory), N (print
 * the connectivity matrices with the graph), P (trace semantics execution),
 * M (trace tree matching), S (print the semantics array), T (print the code
 * tree), X (print the syntax graph), L (list the source), Z (statistics).
 * Dumps go to the file given with -o, else to standard output.  -c writes
 * the pcc IR of the program to a file (- is standard output); cc1 of pcc
 * turns it into assembly.
 *
 * Today this reads syntax files only: the semantic routines that generate
 * code are not ported, so a program that uses one stops with an error.
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "imp.h"

static void
cotree_hook(word k)
{
    cotree(k);
}

static void
usage(void)
{
    fputs("usage: imp72s0 [-s LETTERS] [-o FILE] [-c IRFILE] SYNTAX [FILE ...]\n", stderr);
    exit(2);
}

int
main(int argc, char **argv)
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
        else
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
