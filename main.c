#include "precompiler.h"

#include <stdio.h>
#include <string.h>

static void usage(void) {
    fprintf(stderr, "Uso: myPreCompiler -i <file_input.c> [-o <file_output>] [-v]\n");
}

int main(int argc, char **argv) {
    const char *in = NULL;
    const char *out = NULL;
    int verbose = 0;

    for (int i = 1; i < argc; ++i) {
        if (!strncmp(argv[i], "--in=", 5)) {
            in = argv[i] + 5;
        } else if (!strncmp(argv[i], "--out=", 6)) {
            out = argv[i] + 6;
        } else if (!strcmp(argv[i], "--verbose")) {
            verbose = 1;
        } else if (!strncmp(argv[i], "--in", 4)) {
            in = ++i < argc ? argv[i] : NULL;
        } else if (!strncmp(argv[i], "--out", 5)) {
            out = ++i < argc ? argv[i] : NULL;
        }
        ...to be continued
    }
}