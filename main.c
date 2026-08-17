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
        } else if (argv[i][0] == '-' && argv[i][1] != '-' && argv[i][1] != '\0') {
            for (size_t j = 1; argv[i][j] != '\0'; ++j) {
                char option = argv[i][j];
                if (argv[i][j] == 'v') {
                    verbose = 1;
                } else if (option == 'i' || option == 'o') {
                    const char *value = argv[i][j + 1] ? argv[i] + j + 1 : (++i < argc ? argv[i] : NULL);
                    if (!value) {
                        usage();
                        return 1;
                    }
                    if (option == 'i') {
                        in = value;
                    } else {
                        out = value;
                    }
                    break;
                } else {
                    usage();
                    return 1;
                }
            }
        } else {
            usage();
            return 1;
        }
    }

    if (!in) {
        usage();
        return 1;
    }
    return precompiler_run(in, out, verbose);
}