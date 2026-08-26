#include "precompiler.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//Safe string duplication wrapper
static char *xstrdup(const char *s) { size_t n = strlen(s) + 1; char *p = malloc(n); if (p) memcpy(p, s, n); return p; }

//Dynamically grows dynamic array buffers (2x capacity growth strategy)
static int grow(void **ptr, size_t *cap, size_t elem) {
    size_t next = *cap ? *cap * 2 : 8;
    void *tmp = realloc(*ptr, next * elem);
    if (!tmp) return 0;
    *ptr = tmp;
    *cap = next;
    return 1;
}

//Appends a unique custom type identifier to the collection
static void push_type(Strs *a, const char *s) {
    for (size_t i = 0; i < a->n; ++i) if (!strcmp(a->s[i], s)) return;
    if (a->n == a->cap && !grow((void **)&a->s, &a->cap, sizeof *a->s)) return;
    a->s[a->n++] = xstrdup(s);
}

//Checks if a character can start a C identifier
static int is_ident_start(int c) { return isalpha(c) || c == '_'; }

//Checks if a character is valid within a C identifier
static int is_ident_char(int c) { return isalnum(c) || c == '_'; }

//Trims leading and trailing whitespace characters in-space
static char *trim(char *s) { while (isspace((unsigned char)*s)) ++s; char *e = s + strlen(s); while (e > s && isspace((unsigned char)e[-1])) --e; *e = 0; return s; }

//Checks if a given string matches standard C keywords
static int is_kw(const char *s) {
    static const char *k[] = {"auto","break","case","char","const","continue","default","do","double","else","enum","extern","float","for","goto","if","inline","int","long","register","restrict","return","short","signed","sizeof","static","struct","switch","typedef","union","unsigned","void","volatile","while","_Bool","_Complex","_Imaginary"};
    for (size_t i = 0; i < sizeof k / sizeof *k; ++i) if (!strcmp(s, k[i])) return 1;
    return 0;
}

//Reads the entire contents of a file into a dynamically allocated buffer
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
        return NULL;
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
        fclose(f);
        return NULL;
    }
    if (fclose(f) != 0) {
        fprintf(stderr, "Failed to close input file '%s'.\n", path);
    }
    b[n] = 0;
    return b;
}

//Splits full raw source code text into an array of individual line strings
static size_t split_lines(char *text, char ***out) {
    char **lines = NULL;
    size_t n = 0;
    size_t cap = 0;
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

//Add an error entry to the error collection list
static void add_err(Errs *a, int line, const char *msg) {
    if (!a || !msg) return;
    if (a->n == a->cap && !grow((void **)&a->e, &a->cap, sizeof(*a->e))) {
        return;
    }
    a->e[a->n].line = line;
    a->e[a->n].msg = xstrdup(msg);
    a->n++;
}

//Records a parsed variable alongside validation state flags
static void add_var(Vars *a, const char *name, int line, int ok_name, int ok_type) {
    if (a->n == a->cap && !grow((void **)&a->v, &a->cap, sizeof *a->v)) return;
    a->v[a->n++] = (Var){ xstrdup(name), line, ok_name, ok_type, 0 };
}

//Extracts the rightmost C identifier token from a given string fragment
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

//Locates the zero-based index of the main function signature
static int find_main(char **lines, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (strstr(lines[i], "main")) {
            return (int)i;
        }
    }
    return -1;
}

//Strips single.line (//) and multiline (/* */) comments while maintaining state
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

//Verifies if a code statement terminates with a semicolon
static int line_ends_semicolon(const char *s) {
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) --n;
    return n && s[n - 1] == ';';
}

//Split a declaration statement into type specifiers and variable list segments
static size_t split_decl_parts(char *line, char **parts, size_t max_parts) {
    size_t pc = 0;
    line = trim(line);

    char *p = line;
    while (*p && !isspace((unsigned char)*p)) ++p;

    char first_word[64] = {0};
    size_t len = p - line;
    if (len < sizeof(first_word)) {
        strncpy(first_word, line, len);
        first_word[len] = '\0';
    }

    //Handle compound multi-word types
    if (strcmp(first_word, "unsigned") == 0 || strcmp(first_word, "signed") == 0 ||
        strcmp(first_word, "long") == 0 || strcmp(first_word, "short") == 0) {
        while (*p && isspace((unsigned char)*p)) ++p;
        while (*p && !isspace((unsigned char)*p)) ++p;
        }

    if (*p) {
        *p = '\0';
        parts[pc++] = trim(line);
        p++;
    } else {
        return 0;
    }


    char *start = p;
    int depth = 0;
    for (;; ++p) {
        if (*p == '(' || *p == '[') ++depth;
        else if (*p == ')' || *p == ']') --depth;

        if ((*p == ',' && !depth) || !*p) {
            char saved = *p;
            *p = '\0';
            char *trimmed = trim(start);
            if (*trimmed && pc < max_parts) {
                parts[pc++] = trimmed;
            }
            if (!saved) break;
            start = p + 1;
        }
    }
    return pc;
}

//Validates whether a given string follows C variable naming conventions
static int is_valid_c_identifier(const char *s) {
    if (!s || !*s) return 0;


    if (!isalpha((unsigned char)s[0]) && s[0] != '_') return 0;


    for (size_t i = 1; s[i] != '\0'; ++i) {
        if (!isalnum((unsigned char)s[i]) && s[i] != '_') return 0;
    }

    return 1;
}

//Process individual variable names, stripping initializers and indirection dereferences
static void process_decl_vars(char **parts, size_t pc, int ok_type, Vars *vars, Errs *errs, int line_no) {
    for (size_t i = 1; i < pc; ++i) {
        char *raw_name = trim(parts[i]);

        //Strip inline initialization assignments
        char *eq = strchr(raw_name, '=');
        if (eq) *eq = '\0';
        raw_name = trim(raw_name);

        //Strip leading pointer dereference operators
        while (*raw_name == '*') {
            raw_name++;
            raw_name = trim(raw_name);
        }

        int valid_name = is_valid_c_identifier(raw_name) && !is_kw(raw_name);

        if (!valid_name) {
            char err_msg[256];
            snprintf(err_msg, sizeof(err_msg), "variable identifier not valid: %s", raw_name);
            add_err(errs, line_no, err_msg);
        }

        add_var(vars, raw_name, line_no, valid_name, ok_type);
    }
}

//Lookup table for built-in primitive C data types and combinations
int is_builtin_type(const char *type) {
    static const char *types[] = {
        "int", "char", "float", "double", "void",
        "short", "long", "signed", "unsigned",
        "unsigned int", "signed int",
        "short int", "unsigned short", "unsigned short int",
        "long int", "signed long", "unsigned long", "unsigned long int",
        "long long", "long long int", "unsigned long long",
        "float", "double", "long double",
        NULL
    };

    for (int i = 0; types[i] != NULL; ++i) {
        if (strcmp(types[i], type) == 0) {
            return 1;
        }
    }
    return 0;
}

//Parses and validates a line recognized as a variable declaration statement
static void parse_decl(char *line, int line_no, Strs *types, Vars *vars, Errs *errs) {
    line = trim(line);
    if (!*line || !strncmp(line, "typedef", 7)) return;
    if (strstr(line, "+=") || strstr(line, "-=") || strstr(line, "*=") || strstr(line, "/=")) return;
    if (strncmp(line, "for", 3) == 0 || strncmp(line, "while", 5) == 0 || strncmp(line, "if", 2) == 0) return;
    if (strncmp(line, "return", 6) == 0) return;

    char *semi = strrchr(line, ';');
    if (!semi) return;
    *semi = 0;
    line = trim(line);

    char *parts[32];
    size_t pc = split_decl_parts(line, parts, 32);
    if (pc < 2) return;

    char *trimmed_type = parts[0];

    for (size_t i = 0; i < vars->n; ++i) {
        if (strcmp(vars->v[i].name, trimmed_type) == 0) return;
    }

    int is_known = is_builtin_type(trimmed_type);
    if (!is_known && types) {
        for (size_t i = 0; i < types->n; ++i) {
            if (strcmp(types->s[i], trimmed_type) == 0) {
                is_known = 1;
                break;
            }
        }
    }

    int ok_type = is_known;

    if (!ok_type) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Type not valid: %s", trimmed_type);
        add_err(errs, line_no, err_msg);
    }


    process_decl_vars(parts, pc, ok_type, vars, errs, line_no);
}

//Replaces double and single quote contents with whitespace to prevent false positive symbol tracking
static void strip_strings(char *s) {
    int in_str=0;
    int in_char=0;

    for (char *p = s; *p; ++p) {
        //toggle string literal context (double quotes)
        if (*p == '"' && !in_char) {
            in_str = !in_str;
            *p = ' ';
            continue;
        }

        //toggle character literal context (single quotes)
        if (*p == '\'' && !in_str) {
            in_char = !in_char;
            *p= ' ';
            continue;
        }

        //mark content inside string or character literals
        if (in_str || in_char) {
            *p = ' ';
        }
    }
}

//Verifiers token occurrences against declared identifiers and marks used variables
static void check_and_mark_var(const char *name, Vars *vars) {
    if (is_kw(name)) return;

    for (size_t i = 0; i < vars->n; ++i) {
        if (vars->v[i].ok_name && strcmp(vars->v[i].name, name) == 0) {
            vars->v[i].used = 1;
            break;
        }
    }
}

//Scans executionable expressions to mark declared variables as used
static void mark_used(char *line, Vars *vars) {
    strip_strings(line);

    char *p = line;
    while (*p) {
        if (!is_ident_start((unsigned char)*p)) {
            ++p;
            continue;
        }

        char *s = p;
        while (*p && is_ident_char((unsigned char)*p)) {
            ++p;
        }

        char save = *p;
        *p = '\0';

        check_and_mark_var(s, vars);

        *p = save;
    }
}

//Evaluates whether a line structure fits candidate criteria for variable declarations
static int decl_candidate(const char *s, Strs *types) {
    (void)types;
    while (isspace((unsigned char)*s)) ++s;
    if (!*s || *s == '{' || *s == '}') return 0;

    // ignore core control flow structure statements
    if (!strncmp(s, "return", 6) && !is_ident_char((unsigned char)s[6])) return 0;
    if (!strncmp(s, "if", 2) && !is_ident_char((unsigned char)s[2])) return 0;
    if (!strncmp(s, "while", 5) && !is_ident_char((unsigned char)s[5])) return 0;
    if (!strncmp(s, "for", 3) && !is_ident_char((unsigned char)s[3])) return 0;

    // Must contain at least two words roken prior to a semicolon or assignment operator
    int words = 0, in_word = 0;
    for (const char *p = s; *p && *p != ';' && *p != '='; ++p) {
        if (!isspace((unsigned char)*p)) {
            if (!in_word) { in_word = 1; words++; }
        } else {
            in_word = 0;
        }
    }
    return words >= 2;
}

//Safely appends formatted text into the dynamic report output buffer
static int report_append_str(Buf *b, const char *str) {
    if (!str) return 0;
    size_t len = strlen(str);

    while (b->len + len + 1 > b->cap) {
        if (!grow((void **)&b->buf, &b->cap, sizeof(char))) {
            return 0;
        }
    }

    memcpy(b->buf + b->len, str, len);
    b->len += len;
    b->buf[b->len] = '\0';
    return 1;
}

//Writes generated summary reports to an output file or standard output streams
static void write_report_output(const char *output_path, int verbose, const char *text) {
    if (output_path) {
        FILE *out_f = fopen(output_path, "w");
        if (!out_f) {
            fprintf(stderr, "Could not open output file: %s\n", output_path);
        } else {
            if (fputs(text, out_f) == EOF) {
                fprintf(stderr, "Could not write to output file: %s\n", output_path);
            }
            if (fclose(out_f) != 0) {
                fprintf(stderr, "Could not close output file: %s\n", output_path);
            }
        }
    }

    if (verbose || !output_path) {
        fputs(text, stdout);
    }
}

//Builds analysis metric summaries and formats total error logs
static void generate_report(const char *input_path, const char *output_path, int verbose, Vars *vars, Errs *errs) {
    int invalid_names_count = 0;
    int invalid_types_count = 0;
    int unused_vars_count = 0;
    char line_buf[512];

    for (size_t i = 0; i < vars->n; ++i) {
        if (!vars->v[i].ok_name) {
            invalid_names_count++;
        }
        if (!vars->v[i].ok_type) {
            invalid_types_count++;
        }

        //Flag variables that are fully valid but left unreferenced
        if (vars->v[i].ok_name && vars->v[i].ok_type && !vars->v[i].used) {
            unused_vars_count++;
            char err_msg[256];
            snprintf(err_msg, sizeof(err_msg), "unused variable: %s", vars->v[i].name);
            add_err(errs, vars->v[i].line, err_msg);
        }
    }

    Buf report = {0};

    report_append_str(&report, "=== MYPRECOMPILER PROCESSING STATISTICS ===\n");

    snprintf(line_buf, sizeof(line_buf), "Analyzed file: %s\n", input_path);
    report_append_str(&report, line_buf);

    snprintf(line_buf, sizeof(line_buf), "Total variables checked: %d\n", (int)vars->n);
    report_append_str(&report, line_buf);

    snprintf(line_buf, sizeof(line_buf), "Total errors detected: %d\n", (int)errs->n);
    report_append_str(&report, line_buf);

    snprintf(line_buf, sizeof(line_buf), " - Invalid variable names: %d\n", invalid_names_count);
    report_append_str(&report, line_buf);

    snprintf(line_buf, sizeof(line_buf), " - Invalid data types: %d\n", invalid_types_count);
    report_append_str(&report, line_buf);

    snprintf(line_buf, sizeof(line_buf), " - Unused variables: %d\n\n", unused_vars_count);
    report_append_str(&report, line_buf);

    report_append_str(&report, "--- DETECTED ERRORS LIST ---\n");
    if (errs->n == 0) {
        report_append_str(&report, "No errors found.\n");
    } else {
        for (size_t i = 0; i < errs->n; ++i) {
            snprintf(line_buf, sizeof(line_buf), "[Line %d] Error: %s\n", errs->e[i].line, errs->e[i].msg);
            report_append_str(&report, line_buf);
        }
    }

    if (report.buf) {
        write_report_output(output_path, verbose, report.buf);
    }

    free(report.buf);
}

//Deallocates all dynamic structures, string references and container buffers
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

//Parses custom type alias declarations definied via typedef statements
static void parse_typedef(const char *line, Strs *types) {
    char *dup = xstrdup(line);
    char *semi = strrchr(dup, ';');
    if (semi) *semi = 0;

    char *p = dup + 7;
    while (isspace((unsigned char)*p)) ++p;

    char tmp[64];
    size_t pos = 0;
    if (extract_name(p, tmp, sizeof(tmp), &pos)) {
        push_type(types, tmp);
    }
    free(dup);
}

//Analyzes global definitions and declarations located above the main entry point
static void parse_pre_main(char **lines, int m, Strs *types, Vars *vars, Errs *errs) {
    int block = 0;
    for (int i = 0; i < m; ++i) {
        char *dup = xstrdup(lines[i]);
        strip_comments(dup, &block);
        char *line = trim(dup);

        if (!*line) {
            free(dup);
            continue;
        }

        if (strncmp(line, "typedef", 7) == 0) {
            parse_typedef(line, types);
        } else if (line_ends_semicolon(line)) {
            parse_decl(line, i + 1, types, vars, errs);
        }

        free(dup);
    }
}

//Analyzes local declarations enclosed inside or placed below the main entry point
static void parse_post_main(char **lines, size_t line_count, int m, Strs *types, Vars *vars, Errs *errs) {
    int block = 0;
    size_t start_index = (m >= 0) ? (size_t)m + 1 : 0;

    for (size_t i = start_index; i < line_count; ++i) {
        char *dup = xstrdup(lines[i]);
        strip_comments(dup, &block);
        char *line = trim(dup);

        // skip structural braces or empty statements
        if (*line && strcmp(line, "{") != 0 && strcmp(line, "}") != 0) {
            // Stop parsing local declarations once return statements is hit
            if (!strncmp(line, "return", 6) && !is_ident_char((unsigned char)line[6])) {
                free(dup);
                break;
            }

            if (line_ends_semicolon(line) && decl_candidate(line, types)) {
                parse_decl(line, (int)i + 1, types, vars, errs);
            }
        }

        free(dup);
    }
}

//Traverses all non-declaration lines to mark referenced variables as active
static void mark_all_used(char **lines, size_t line_count, Vars *vars) {
    int block = 0;
    for (size_t i = 0; i < line_count; ++i) {
        int decl_line = 0;
        for (size_t j = 0; j < vars->n; ++j) {
            if ((int)i + 1 == vars->v[j].line) {
                decl_line = 1;
                break;
            }
        }
        if (decl_line) continue;

        char *dup = xstrdup(lines[i]);
        strip_comments(dup, &block);
        mark_used(dup, vars);
        free(dup);
    }
}

//Main execution entry point for precompiler validation logic
int precompiler_run(const char *input_path, const char *output_path, int verbose) {
    char *text = read_all(input_path);
    if (!text) {
        fprintf(stderr, "Can't read input file.\n");
        return 1;
    }

    char **lines = NULL;
    size_t line_count = split_lines(text, &lines);
    free(text);
    if (!line_count) {
        fprintf(stderr, "File empty or not valid.\n");
        free(lines);
        return 1;
    }

    int m = find_main(lines, line_count);
    if (m < 0) {
        fprintf(stderr, "Can't find the main function.\n");
        free_resources(lines, line_count, NULL, NULL, NULL);
        return 1;
    }

    Vars vars = {0};
    Errs errs = {0};
    Strs types = {0};

    parse_pre_main(lines, m, &types, &vars, &errs);
    parse_post_main(lines, line_count, m, &types, &vars, &errs);
    mark_all_used(lines, line_count, &vars);

    generate_report(input_path, output_path, verbose, &vars, &errs);
    free_resources(lines, line_count, &vars, &errs, &types);

    return 0;
}