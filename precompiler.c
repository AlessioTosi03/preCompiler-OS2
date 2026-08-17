#include "precompiler.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *name; int line, ok_name, ok_type, used; } Var;
typedef struct { char *msg; int line; } Err;
typedef struct { Var *v; size_t n, cap; } Vars;
typedef struct { Err *e; size_t n, cap; } Errs;
typedef struct { char **s; size_t n, cap; } Strs;
typedef struct { char *buf; size_t len, cap; } Buf;

static char *xstrdup(const char *s) { size_t n = strlen(s) + 1; char *p = malloc(n); if (p) memcpy(p, s, n); return p; }

static int grow(void **ptr, size_t *cap, size_t elem) {
    size_t next = *cap ? *cap * 2 : 8;
    void *tmp = realloc(*ptr, next * elem);
    if (!tmp) return 0;
    *ptr = tmp;
    *cap = next;
    return 1;
}

static int is_ident_char(int c) { return isalnum(c) || c == '_'; }

static size_t split_lines(char *text, char ***out) {
    char **lines = NULL;
    size_t n = 0, cap = 0;
    char *start = text;
    for (char *p = text; ; ++p) {
        if (*p == '\n' || *p == '\0') {
            char saved = *p;
            *p = 0;
            if (n == cap && !grow((void **)&lines, &cap, sizeof *lines)) break;
            lines[n++] = xstrdup(trim(start));
            if (saved == '\0') break;
            start = p + 1;
        }
    }
    *out = lines;
    return n;
}

static int find_main(char **lines, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        char *m = strstr(lines[i], "main");
        if (m && (m == lines[i] || !is_ident_char((unsigned char)m[-1])) && !is_ident_char((unsigned char)m[4])) return (int)i;
    }
    return -1;
}

int precompiler_run(const char *input_path, const char *output_path, int verbose) {
    char *text = read_all(input_path);
    if (!text) { fprintf(stderr, "Errore: impossibile leggere il file di input.\n"); return 1; }

    char **lines = NULL;
    size_t line_count = split_lines(text, &lines);
    free(text);
    if (!line_count) { fprintf(stderr, "Errore: file vuoto o non valido.\n"); free(lines); return 1; }

    int m = find_main(lines, line_count);
    if (m < 0) { fprintf(stderr, "Errore: impossibile individuare la funzione main.\n"); for (size_t i = 0; i < line_count; ++i) free(lines[i]); free(lines); return 1; }

}