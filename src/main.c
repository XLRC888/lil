#include "lil.h"

FuncDef funcs[MAX_FUNCS];
int func_count;

Value eval_expr(ASTNode *n);
int exec_stmt(ASTNode *n);

char *sdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (!p) fatal("out of memory");
    memcpy(p, s, n);
    return p;
}

char *sdupn(const char *s, size_t n) {
    char *p = malloc(n + 1);
    if (!p) fatal("out of memory");
    memcpy(p, s, n);
    p[n] = 0;
    return p;
}

void *safe_alloc(size_t sz) {
    void *p = calloc(1, sz);
    if (!p) fatal("out of memory");
    return p;
}

void run_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "error: cannot open '%s': %s\n", path, strerror(errno)); return; }
    memset(lib_imported, 0, sizeof(lib_imported));
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *src = malloc(sz + 1);
    if (!src) { fclose(f); fprintf(stderr, "out of memory\n"); return; }
    long got = fread(src, 1, sz, f);
    fclose(f);
    if (got < sz) fprintf(stderr, "warning: short read from '%s'\n", path);
    src[got] = 0;

    compiled_header = 0;
    mcm_forge_mode = 0;
    mcm_flat_mode = 0;
    if (java_output_filename) { free(java_output_filename); java_output_filename = NULL; }
    char *psrc = src;
    while (*psrc == ' ' || *psrc == '\t' || *psrc == '\n' || *psrc == '\r') psrc++;
    if (psrc[0] == '[') {
        if (strncmp(psrc, "[compiled]", 10) == 0
            && (psrc[10] == '\n' || psrc[10] == '\r' || psrc[10] == 0)) {
            compiled_header = 1;
            psrc = psrc + 10;
            while (*psrc == '\n' || *psrc == '\r') psrc++;
        } else if (strncmp(psrc, "[MCMForge]", 10) == 0
            && (psrc[10] == '\n' || psrc[10] == '\r' || psrc[10] == 0)) {
            mcm_forge_mode = 1;
            compiled_header = 1;
            psrc = psrc + 10;
            while (*psrc == '\n' || *psrc == '\r') psrc++;
        }
    }
    if (mcm_forge_mode) {
        while (*psrc == ' ' || *psrc == '\t' || *psrc == '\n' || *psrc == '\r') psrc++;
        if (psrc[0] == '(' && psrc[1] == '[') {
            char *cp = psrc + 2;
            int start = 0;
            while (cp < src + got && *cp != ']' && *cp != '\n') cp++;
            if (*cp == ']' && cp > psrc + 2) {
                start = 2;
                int end = cp - psrc;
                java_output_filename = sdupn(psrc + start, end - start);
                cp++;
                if (*cp == ')') cp++;
                psrc = cp;
                while (*psrc == '\n' || *psrc == '\r') psrc++;
            }
        }
        if (!java_output_filename) {
            const char *dot = strrchr(path, '.');
            if (dot) {
                size_t n = dot - path;
                char *base = malloc(n + 6);
                if (!base) { fprintf(stderr, "out of memory\n"); return; }
                memcpy(base, path, n); base[n] = 0;
                strcat(base, ".java");
                java_output_filename = base;
            } else {
                java_output_filename = sdup("output.java");
            }
        }
    }
    if (compiled_header && !compile_mode) {
        if (mcm_forge_mode)
            fprintf(stderr, "error: code is in MCMForge mode (use -j to compile)\n");
        else
            fprintf(stderr, "error: code is in compiled mode (use -c to compile)\n");
        free(src);
        return;
    }

    if (setjmp(error_jmp)) { free(src); src = NULL; return; }

    lex_init(psrc);
    lex_next();
    scope_depth = 0;
    anon_counter = 0;

    ASTNode *prog = parse_program();
    program_root = prog;

    code_len = 0; const_len = 0; fb_len = 0;    fixup_len = 0; cur_loop = 0; for_counter = 0;
    compile_prog(prog->data.block.stmts, prog->data.block.count);
    vm_run();
    free(src);
}

void repl(void) {
    memset(lib_imported, 0, sizeof(lib_imported));
    printf("lil v%s - type 'exit' to quit\n", VERSION);
    char buf[BUF_SIZE];

    while (1) {
        printf("> ");
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) break;
        size_t len = strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = 0;

        if (len == 0) continue;
        if (!strcmp(buf, "exit")) break;

        if (setjmp(error_jmp)) continue;

        scope_depth = 0;
        anon_counter = 0;
        lex_init(buf);
        lex_next();
        while (lex_cur.type != TOK_EOF) {
            ASTNode *s = parse_stmt();
            if (s->type != NODE_EMPTY) {
                int r = exec_stmt(s);
                if (r == 1) return;
            }
            if (lex_cur.type == TOK_NEWLINE) lex_next();
        }
    }
}

int main(int argc, char **argv) {
    error_occurred = 0;
    in_try = 0;
    compile_mode = 0;
    mcm_forge_mode = 0;
    mcm_flat_mode = 0;
    java_output_filename = NULL;
    const char *inpath = NULL;
    const char *outpath = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            printf("usage: lil [options] [file.lil]\n");
            printf("  no args      start REPL\n");
            printf("  file.lil     execute file\n");
            printf("  -c file.lil  compile to standalone binary\n");
            printf("  -j file.lil  compile to Java (MCMForge)\n");
            printf("  -jc file.lil compile to Java, flat output (no src/ tree)\n");
            printf("  -o output    output filename/directory (default: a.out)\n");
            return 0;
        } else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--compile")) {
            compile_mode = 1;
        } else if (!strcmp(argv[i], "-j") || !strcmp(argv[i], "--java")) {
            compile_mode = 1;
            mcm_forge_mode = 1;
        } else if (!strcmp(argv[i], "-jc") || !strcmp(argv[i], "--java-flat")) {
            compile_mode = 1;
            mcm_forge_mode = 1;
            mcm_flat_mode = 1;
        } else if (!strcmp(argv[i], "-o")) {
            if (i + 1 < argc) outpath = argv[++i];
            else { fprintf(stderr, "error: -o requires an argument\n"); return 1; }
        } else if (argv[i][0] != '-') {
            inpath = argv[i];
        } else {
            fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }
    if (compile_mode) {
        if (!inpath) { fprintf(stderr, "error: no input file\n"); return 1; }
        if (mcm_forge_mode) {
            if (!outpath) {
                fprintf(stderr, "error: MCMForge mode requires -o <dir>/ for output directory\n");
                fprintf(stderr, "  use -o <dir>/   for full mod source tree\n");
                fprintf(stderr, "  use -jc -o <dir>/  for flat output (no src/ tree)\n");
                return 1;
            }
            return generate_java(inpath, outpath);
        }
        if (!outpath) {
            const char *dot = inpath ? strrchr(inpath, '.') : NULL;
            if (dot) {
                size_t n = dot - inpath;
                char *buf = malloc(n + 1);
                if (!buf) { fprintf(stderr, "out of memory\n"); return 1; }
                memcpy(buf, inpath, n); buf[n] = 0;
                outpath = buf;
            } else {
                outpath = "a.out";
            }
        }
        return generate_c(inpath, outpath);
    }
    if (inpath) {
        run_file(inpath);
    } else {
        repl();
    }
    return error_occurred ? 1 : 0;
}
