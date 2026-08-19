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

static void push_type(Strs *a, const char *s) {
    for (size_t i = 0; i < a->n; ++i) if (!strcmp(a->s[i], s)) return;
    if (a->n == a->cap && !grow((void **)&a->s, &a->cap, sizeof *a->s)) return;
    a->s[a->n++] = xstrdup(s);
}

static int has_type(const Strs *a, const char *s) { for (size_t i = 0; i < a->n; ++i) if (!strcmp(a->s[i], s)) return 1; return 0; }
static int is_ident_start(int c) { return isalpha(c) || c == '_'; }
static int is_ident_char(int c) { return isalnum(c) || c == '_'; }
static char *trim(char *s) { while (isspace((unsigned char)*s)) ++s; char *e = s + strlen(s); while (e > s && isspace((unsigned char)e[-1])) --e; *e = 0; return s; }
static int is_kw(const char *s) {
    static const char *k[] = {"auto","break","case","char","const","continue","default","do","double","else","enum","extern","float","for","goto","if","inline","int","long","register","restrict","return","short","signed","sizeof","static","struct","switch","typedef","union","unsigned","void","volatile","while","_Bool","_Complex","_Imaginary"};
    for (size_t i = 0; i < sizeof k / sizeof *k; ++i) if (!strcmp(s, k[i])) return 1;
    return 0;
}

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n <= 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    char *b = malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)n, f) != (size_t)n || fclose(f) != 0) { free(b); return NULL; }
    b[n] = 0;
    return b;
}

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

static int valid_type_prefix(const char *prefix, Strs *types) {
    char tmp[128];
    size_t n = 0;
    for (const char *p = prefix; *p && n + 1 < sizeof tmp; ++p) if (*p != '*' && !isspace((unsigned char)*p)) tmp[n++] = *p;
    tmp[n] = 0;
    if (!tmp[0]) return 0;
    if (has_type(types, tmp)) return 1;
    if (!strcmp(tmp, "int") || !strcmp(tmp, "char") || !strcmp(tmp, "float") || !strcmp(tmp, "double") || !strcmp(tmp, "void") || !strcmp(tmp, "_Bool")) return 1;
    if (!strcmp(tmp, "signed") || !strcmp(tmp, "unsigned") || !strcmp(tmp, "short") || !strcmp(tmp, "long") || !strcmp(tmp, "longlong") || !strcmp(tmp, "signedint") || !strcmp(tmp, "unsignedint")) return 1;
    return !is_kw(tmp);
}

static int extract_name(const char *s, char *name, size_t cap, size_t *pos) {
    size_t end = strlen(s);
    while (end && isspace((unsigned char)s[end - 1])) --end;
    while (end && !is_ident_char((unsigned char)s[end - 1])) --end;
    size_t start = end;
    while (start && is_ident_char((unsigned char)s[start - 1])) --start;
    if (start == end || !is_ident_start((unsigned char)s[start])) return 0;
    size_t n = end - start;
    if (n >= cap) return 0;
    memcpy(name, s + start, n);
    name[n] = 0;
    if (pos) *pos = start;
    return 1;
}

static int find_main(char **lines, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        char *m = strstr(lines[i], "main");
        if (m && (m == lines[i] || !is_ident_char((unsigned char)m[-1])) && !is_ident_char((unsigned char)m[4])) return (int)i;
    }
    return -1;
}

static void strip_comments(char *s, int *block) {
    char *r = s, *w = s;
    while (*r) {
        if (*block) { if (r[0] == '*' && r[1] == '/') { *block = 0; r += 2; } else ++r; continue; }
        if (r[0] == '/' && r[1] == '/') break;
        if (r[0] == '/' && r[1] == '*') { *block = 1; r += 2; continue; }
        *w++ = *r++;
    }
    *w = 0;
}

static void add_err(Errs *a, int line, const char *fmt, ...) {
    if (a->n == a->cap && !grow((void **)&a->e, &a->cap, sizeof *a->e)) return;
    char tmp[256];
    va_list ap; va_start(ap, fmt); vsnprintf(tmp, sizeof tmp, fmt, ap); va_end(ap);
    a->e[a->n].line = line;
    a->e[a->n].msg = xstrdup(tmp);
    ++a->n;
}

static void add_var(Vars *a, const char *name, int line, int ok_name, int ok_type) {
    if (a->n == a->cap && !grow((void **)&a->v, &a->cap, sizeof *a->v)) return;
    a->v[a->n++] = (Var){ xstrdup(name), line, ok_name, ok_type, 0 };
}

static int line_ends_semicolon(const char *s) { size_t n = strlen(s); while (n && isspace((unsigned char)s[n - 1])) --n; return n && s[n - 1] == ';'; }

static void parse_decl(char *line, int line_no, Strs *types, Vars *vars, Errs *errs) {
    char *semi = strrchr(line, ';');
    if (semi) *semi = 0;
    line = trim(line);
    if (!*line || !strncmp(line, "typedef", 7)) return;

    char *parts[32];
    size_t pc = 0;
    int depth = 0;
    char *start = line;
    for (char *p = line; ; ++p) {
        if (*p == '(' || *p == '[') ++depth;
        else if (*p == ')' || *p == ']') --depth;
        if ((*p == ',' && !depth) || !*p) {
            if (pc < 32) { *p = 0; parts[pc++] = trim(start); }
            start = p + 1;
            if (!*p) break;
        }
    }

    char name[64];
    size_t pos = 0;
    if (!extract_name(parts[0], name, sizeof name, &pos)) { add_err(errs, line_no, "identificatore di variabile non valido"); add_var(vars, "<invalid>", line_no, 0, 0); return; }
    char prefix[128];
    snprintf(prefix, sizeof prefix, "%.*s", (int)pos, parts[0]);
    int ok_type = valid_type_prefix(prefix, types);
    if (!ok_type) add_err(errs, line_no, "tipo di dato non valido: %s", prefix);

    for (size_t i = 0; i < pc; ++i) {
        if (!extract_name(parts[i], name, sizeof name, NULL) || is_kw(name)) { add_err(errs, line_no, "identificatore di variabile non valido: %s", name[0] ? name : "<invalid>"); add_var(vars, name[0] ? name : "<invalid>", line_no, 0, ok_type); }
        else add_var(vars, name, line_no, 1, ok_type);
    }
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

    Vars vars = {0}; Errs errs = {0}; Strs types = {0};
    int block = 0;

    for (int i = 0; i < m; ++i) {
        char *dup = xstrdup(lines[i]);
        strip_comments(dup, &block);
        char *line = trim(dup);
        if (*line) {
            if (!strncmp(line, "typedef", 7)) {
                char *semi = strrchr(line, ';'); if (semi) *semi = 0;
                char *p = line + 7; while (isspace((unsigned char)*p)) ++p;
                char tmp[64]; size_t pos = 0;
                if (extract_name(p, tmp, sizeof tmp, &pos)) push_type(&types, tmp);
            } else if (line_ends_semicolon(line)) {
                parse_decl(line, i + 1, &types, &vars, &errs);
            }
        }
        free(dup);
    }
}