#ifndef PRECOMPILER_H
#define PRECOMPILER_H

#include <stddef.h>

/* --- Data Structures --- */

typedef struct {
    char *name;
    int line;
    int ok_name;
    int ok_type;
    int used;
} Var;

typedef struct {
    char *msg;
    int line;
} Err;
typedef struct {
    Var *v;
    size_t n;
    size_t cap;
} Vars;
typedef struct {
    Err *e;
    size_t n;
    size_t cap;
} Errs;
typedef struct {
    char **s;
    size_t n;
    size_t cap;
} Strs;
typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} Buf;

/* --- Public Function Prototypes --- */

/**
 * Runs the analysis on the input C file and generates the processing statistics report.
 * @param input_path  Path to the C input file
 * @param output_path Path to the output file (NULL to direct output to stdout)
 * @param verbose     If non-zero (1), prints processing statistics to stdout
 * @return 0 on success, 1 on error
 */

int precompiler_run(const char *input_path, const char *output_path, int verbose);

#endif /* PRECOMPILER_H*/