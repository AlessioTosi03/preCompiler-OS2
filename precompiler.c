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
    if (!f){
        fprintf(stderr, "Can't open input file '%s'.\n", path);
        return NULL;
    };
    if (fseek(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "Failed to seek the end of file '%s'.\n", path);
        fclose(f);
        return NULL;
    }
    long n = ftell(f);
    if (n < 0) {
        fprintf(stderr, "Failed to determine file size for '%s'.\n", path);
        fclose(f);
        return NULL;
    }
    if (n==0) {
        fprintf(stderr, "File '%s' is empty.\n", path);
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fprintf(stderr, "Failed to seek back the start of file '%s'.\n", path);
        fclose(f);
        return NULL:
    }
    char *b = malloc((size_t)n + 1);
    if (!b) {
        fprintf(stderr, "Failed to allocate memory for '%s'.\n", path);
        fclose(f);
        return NULL;
    }
    if (fread(b, 1, (size_t)n, f) != (size_t)n) {
        fprintf(stderr, "Failed to read file '%s'.\n", path);
        free(b);
        return NULL;
    }
    if (fclose(f) != 0) {
        fprintf(stderr, "Failed to close input file '%s'.\n", path);
    }
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

static int typeish_first_word(const char *s, Strs *types) {
    while (isspace((unsigned char)*s)) ++s;
    if (!*s || *s == '{' || *s == '}') return 0;
    if (!strncmp(s, "typedef", 7) && !is_ident_char((unsigned char)s[7])) return 1;
    char word[64]; size_t n = 0;
    while (*s && !is_ident_start((unsigned char)*s)) { if (*s == ';') return 0; ++s; }
    while (is_ident_char((unsigned char)*s) && n + 1 < sizeof word) word[n++] = *s++;
    word[n] = 0;
    if (!word[0]) return 0;
    return has_type(types, word) || !strcmp(word, "auto") || !strcmp(word, "register") || !strcmp(word, "static") || !strcmp(word, "extern") || !strcmp(word, "inline") || !strcmp(word, "const") || !strcmp(word, "volatile") || !strcmp(word, "restrict") || is_kw(word);
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

static void strip_strings(char *s) {
    int in_str=0;
    int in_char=0;

    for (char *p = s; *p; ++p) {
        //toggle string literal context (double quotes) if not inside a character literal
        if (*p == '"' && !in_char) {
            in_str = !in_str;
            *p = ' ';
            continue;
        }

        //toggle character literal context (single quotes) if not inside a character literal
        if (*p == '\'' && !in_str) {
            in_char = !in_char;
            *p= ' ';
            continue;
        }

        //mark content inside literals with spaces to prevent false matches
        if (in_str || in_char) {
            *p = ' ';
        }
    }
}

static void mark_used(char *line, Vars *vars) {
    strip_strings(line);
    /* for (char *p = line; *p; ++p) {
        if (!is_ident_start((unsigned char)*p)) continue;
        char *s = p++;
        while (is_ident_char((unsigned char)*p)) ++p;
        char save = *p;
        *p = 0;
        for (size_t i = 0; i < vars->n; ++i) if (vars->v[i].ok_name && !strcmp(vars->v[i].name, s)) vars->v[i].used = 1;
        *p = save;
        --p;
    }*/

    char *p=line;
    while (*p) {
        //Skip characters that cannot start a valid C identifier
        if (!is_ident_start((unsigned char)*p)) {
            ++p;
            continue;
        }

        //save the starting adress of the potentional identifier
        char *s=p;

        //advance until the end of the identifier
        while (*p && is_ident_char((unsigned char)*p)) ++p;

        // Temporarily null-terminate the string token
        char save = *p;
        *p =0;

        if (!is_kw(s)) {
            for (size_t i = 0; i < vars->n; ++i) {
                if (vars->v[i].ok_name && !strcmp(vars->v[i].name, s)) { vars->v[i].used = 1; break; }
            }
        }
        //restore the original character
        *p=save;
        // Continuation: 'p' is now positioned at the first character
        // past the identifier, so the loop naturally proceeds forward
    }
}

static int decl_candidate(const char *s, Strs *types) {
    while (isspace((unsigned char)*s)) ++s;
    if (!*s || *s == '{' || *s == '}') return 0;
    if (!strncmp(s, "typedef", 7) && !is_ident_char((unsigned char)s[7])) return 1;
    return typeish_first_word(s, types);
}

static void generate_report(const char *input_path, const char *output_path, int verbose, Vars *vars, Errs *errs) {
    int invalid_names_count = 0;
    int invalid_types_count = 0;
    int unused_vars_count = 0;

    for (size_t i = 0; i < vars->n; ++i) {
        if (!vars->v[i].ok_name) invalid_names_count++;
        if (!vars->v[i].ok_type) invalid_types_count++;
        if (!vars->v[i].used) {
            unused_vars_count++;
            add_err(errs, vars->v[i].line, "unused variable: %s", vars->v[i].name);
        }
    }

    Buf report = {0};
#define PRINT_REP(...) do { \
        char _tmp[512]; \
        int _len = snprintf(_tmp, sizeof _tmp, __VA_ARGS__); \
        if (_len > 0) { \
            while (report.len + (size_t)_len + 1 > report.cap) grow((void**)&report.buf, &report.cap, 1); \
            memcpy(report.buf + report.len, _tmp, (size_t)_len); \
            report.len += (size_t)_len; \
            report.buf[report.len] = 0; \
        } \
    } while(0)

    PRINT_REP("=== MYPRECOMPILER PROCESSING STATISTICS ===\n");
    PRINT_REP("Analyzed file: %s\n", input_path);
    PRINT_REP("Total variables checked: %d\n", (int)vars->n);
    PRINT_REP("Total errors detected: %d\n", (int)errs->n);
    PRINT_REP(" - Invalid variable names: %d\n", invalid_names_count);
    PRINT_REP(" - Invalid data types: %d\n", invalid_types_count);
    PRINT_REP(" - Unused variables: %d\n\n", unused_vars_count);

    PRINT_REP("--- DETECTED ERRORS LIST ---\n");
    if (errs->n == 0) {
        PRINT_REP("No errors found.\n");
    } else {
        for (size_t i = 0; i < errs->n; ++i) {
            PRINT_REP("[Line %d] Error: %s\n", errs->e[i].line, errs->e[i].msg);
        }
    }

    if (report.buf) {
        if (output_path) {
            FILE *out_f = fopen(output_path, "w");
            if (!out_f) {
                fprintf(stderr, "Could not open output file: %s\n", output_path);
            } else {
                if (fputs(report.buf, out_f) == EOF) {
                    fprintf(stderr, "Could not write to output file: %s\n", output_path);
                }
                if (fclose(out_f) !=0) {
                    fprintf(stderr, "Could not close output file: %s\n", output_path);
                }
            }
        }

        if (verbose || !output_path) {
            fputs(report.buf, stdout);
        }
    }


    free(report.buf);
}

static void free_resources(char **lines, size_t line_count, Vars *vars, Errs *errs, Strs *types) {
    for (size_t i = 0; i < line_count; ++i) free(lines[i]);
    free(lines);
    for (size_t i = 0; i < vars->n; ++i) free (vars->v[i].name);
    free(vars->v);
    for (size_t i = 0; i < errs->n; ++i) free (errs->e[i].msg);
    free(errs->e);
    for (size_t i = 0; i < types->n; ++i) free (types->s[i]);
    free(types->s);
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

    // picks up all lines before the main function and checks for typedefs and variable declarations
    block = 0;
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

    // picks up all lines after the main function and checks for variable declarations and usage
    block =0;
    int in_decl = 1;
    for (size_t i = (size_t)m + 1; i < line_count; ++i) {
        char *dup = xstrdup(lines[i]);
        strip_comments(dup, &block);
        char *line = trim(dup);
        if (!*line || !strcmp(line, "{") || !strcmp(line, "}")) { free(dup); continue; }
        if (in_decl && decl_candidate(line, &types) && line_ends_semicolon(line)) parse_decl(line, (int)i + 1, &types, &vars, &errs);
        else in_decl = 0;
        free(dup);
    }

    // marks all variables that are used in the code after the main function
    block = 0;
    for (size_t i = 0; i < line_count; ++i) {
        int decl_line = 0;
        for (size_t j = 0; j < vars.n; ++j) if ((int)i + 1 == vars.v[j].line) { decl_line = 1; break; }
        if (decl_line) continue;
        char *dup = xstrdup(lines[i]);
        strip_comments(dup, &block);
        mark_used(dup, &vars);
        free(dup);
    }

    generate_report(input_path, output_path, verbose, &vars, &errs);
    free_resources(lines, line_count, &vars, &errs, &types);

    return 0;

}