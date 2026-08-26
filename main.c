#include "precompiler.h"

#include <stdio.h>
#include <string.h>

static void usage(void) {
    fprintf(stderr, "Use: myPreCompiler -i <file_input.c> [-o <file_output>] [-v]\n");
    fprintf(stderr, "Long options: --in <file_input.c> [--out <file_output>] [--verbose]\n");
}

static int parse_short_value(int argc, char **argv, int *i, size_t j, const char **target) {
    const char *value = argv[*i][j + 1] ? argv[*i] + j + 1 : NULL;
    if (!value) {
        if (*i + 1 < argc) {
            *i += 1;
            value = argv[*i];
        } else {
            return 0;
        }
    }
    *target = value;
    return 1;
}

static int parse_short_opts(int argc, char **argv, int *i, const char **in, const char **out, int *verbose) {
    for (size_t j = 1; argv[*i][j] != '\0'; ++j) {
        char option = argv[*i][j];
        if (option == 'v') {
            *verbose = 1;
        } else if (option == 'i') {
            return parse_short_value(argc, argv, i, j, in);
        } else if (option == 'o') {
            return parse_short_value(argc, argv, i, j, out);
        } else {
            return 0;
        }
    }
    return 1;
}

static int parse_long_opts(int argc, char **argv, int *i, const char **in, const char **out, int *verbose) {
    const char *arg = argv[*i];

    if (!strncmp(arg, "--in=", 5)) {
        *in = arg + 5;
    } else if (!strncmp(arg, "--out=", 6)) {
        *out = arg + 6;
    } else if (!strcmp(arg, "--verbose")) {
        *verbose = 1;
    } else if (!strcmp(arg, "--in")) {
        if (*i + 1 < argc) {
            *i += 1;
            *in = argv[*i];
        } else {
            return 0;
        }
    } else if (!strcmp(arg, "--out")) {
        if (*i + 1 < argc) {
            *i += 1;
            *out = argv[*i];
        } else {
            return 0;
        }
    } else {
        return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    const char *in = NULL;
    const char *out = NULL;
    int verbose = 0;

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (argv[i][1] != '-' && argv[i][1] != '\0') {
                if (!parse_short_opts(argc, argv, &i, &in, &out, &verbose)) {
                    usage();
                    return 1;
                }
            } else if (!parse_long_opts(argc, argv, &i, &in, &out, &verbose)) {
                usage();
                return 1;
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