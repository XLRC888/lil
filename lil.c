#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <setjmp.h>
#include <time.h>
#include <sys/time.h>

#define VERSION "0.1.0"
#define MAX_VARS 1024
#define BUF_SIZE 4096

typedef enum { TOK_NUM, TOK_STR, TOK_ID, TOK_PRINT, TOK_INPUT, TOK_IF, TOK_ELSE, TOK_ELIF,
    TOK_WHILE, TOK_FOR, TOK_TO, TOK_EXIT, TOK_PLUS, TOK_MINUS, TOK_STAR,
    TOK_SLASH, TOK_MOD, TOK_EQ, TOK_NE, TOK_LT, TOK_GT, TOK_LE, TOK_GE, TOK_ASSIGN,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE, TOK_COMMA,
    TOK_SEMI, TOK_NEWLINE, TOK_EOF, TOK_AND, TOK_OR, TOK_NOT,
    TOK_LOOP, TOK_STOP, TOK_INCLUDE, TOK_FUNCTION,
    TOK_HAS, TOK_NOCASE, TOK_ANYWHERE, TOK_WORD,
    TOK_LBRACKET, TOK_RBRACKET,
    TOK_TEMPLATE, TOK_DOLLAR_ID } TokenType;

typedef struct {
    TokenType type;
    int line;
    union { double num; char *str; } val;
} Token;

typedef enum { VAL_NUM, VAL_STR } ValType;

typedef struct {
    ValType type;
    union { double num; char *str; } data;
} Value;

typedef enum { NODE_NUM, NODE_STR, NODE_ID, NODE_BINOP, NODE_UNARY,
    NODE_ASSIGN, NODE_PRINT, NODE_INPUT, NODE_IF, NODE_WHILE,
    NODE_FORTO, NODE_BLOCK, NODE_EXIT, NODE_EMPTY,
    NODE_LOOP, NODE_STOP, NODE_INCLUDE, NODE_FUNCTION,
    NODE_TEMPLATE } NodeType;

typedef struct ASTNode {
    NodeType type;
    int line;
    union {
        double num;
        char *str;
        char *id;
        struct { int op; struct ASTNode *left, *right; } binop;
        struct { int op; struct ASTNode *operand; } unary;
        struct { char *name; struct ASTNode *value; } assign;
        struct { struct ASTNode **exprs; int count; int cap; } print;
        struct { char *name; char *prompt; } input;
        struct { struct ASTNode *cond, *then, *els; int flags; int has_mode;
            struct ASTNode *has_item; struct ASTNode **has_items; int has_nitems; } if_stmt;
        struct { struct ASTNode *cond, *body; } while_stmt;
        struct { char *var; struct ASTNode *start, *end, *body; } forto;
        struct { struct ASTNode *body; } loop;
        struct { char *name; } include;
        struct { char *lib; char **args; int argc; } funcall;
        struct { char *raw; } templ;
        struct { struct ASTNode **stmts; int count, cap; } block;
    } data;
} ASTNode;

typedef struct {
    char *name;
    Value val;
} Var;

static jmp_buf error_jmp;
static int error_occurred;

static Token lex_scan(void);
static Token lex_peek_next(void);
static ASTNode *parse_expr(void);
static ASTNode *parse_stmt(void);
static ASTNode *parse_block(void);
static Value eval_expr(ASTNode *n);
static int exec_stmt(ASTNode *n);
static double math_parse_expr(const char **p);
static double math_parse_term(const char **p);
static double math_parse_factor(const char **p);

static void fatal(const char *fmt, ...) {
    error_occurred = 1;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    fflush(stderr);
    longjmp(error_jmp, 1);
}

static char *sdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (!p) fatal("out of memory");
    memcpy(p, s, n);
    return p;
}

static char *sdupn(const char *s, size_t n) {
    char *p = malloc(n + 1);
    if (!p) fatal("out of memory");
    memcpy(p, s, n);
    p[n] = 0;
    return p;
}

static void *safe_alloc(size_t sz) {
    void *p = calloc(1, sz);
    if (!p) fatal("out of memory");
    return p;
}

static Var vars[MAX_VARS];
static int var_count;

static int var_find(const char *name) {
    for (int i = 0; i < var_count; i++)
        if (!strcmp(vars[i].name, name)) return i;
    return -1;
}

static Value var_get(const char *name) {
    int i = var_find(name);
    if (i < 0) { Value v = {VAL_NUM, {.num=0}}; return v; }
    return vars[i].val;
}

static void var_set(const char *name, Value v) {
    int i = var_find(name);
    if (i >= 0) {
        if (vars[i].val.type == VAL_STR) free(vars[i].val.data.str);
        vars[i].val = v;
    } else {
        if (var_count >= MAX_VARS) fatal("too many variables");
        vars[var_count].name = sdup(name);
        vars[var_count].val = v;
        var_count++;
    }
}

static Value make_num(double n) {
    Value v = {VAL_NUM, {.num=n}};
    return v;
}

static Value make_str(const char *s) {
    Value v = {VAL_STR, {.str=sdup(s)}};
    return v;
}

static char *val_tostr(Value v) {
    if (v.type == VAL_STR) return sdup(v.data.str);
    char buf[128];
    double d = v.data.num;
    if (d == (long)d) snprintf(buf, sizeof(buf), "%ld", (long)d);
    else snprintf(buf, sizeof(buf), "%.10g", d);
    return sdup(buf);
}

static double val_tonum(Value v) {
    if (v.type == VAL_NUM) return v.data.num;
    char *end;
    double d = strtod(v.data.str, &end);
    if (*end) fatal("cannot convert '%s' to number", v.data.str);
    return d;
}

static Token lex_cur;
static Token lex_peek;
static int lex_has_peek;
static const char *lex_src;
static int lex_pos;
static int lex_len;
static int lex_line;

static Token lex_next(void) {
    if (lex_has_peek) {
        lex_has_peek = 0;
        return lex_cur = lex_peek;
    }
    return lex_cur = lex_scan();
}

static Token lex_peek_next(void) {
    if (!lex_has_peek) {
        lex_peek = lex_scan();
        lex_has_peek = 1;
    }
    return lex_peek;
}

static Token lex_scan(void) {
    Token t;
    t.line = lex_line;
    while (lex_pos < lex_len) {
        char c = lex_src[lex_pos];
        if (c == ' ' || c == '\t' || c == '\r') { lex_pos++; continue; }
        if (c == '\n') { lex_pos++; lex_line++; t.type = TOK_NEWLINE; return t; }

        if (c == '#') {
            while (lex_pos < lex_len && lex_src[lex_pos] != '\n') lex_pos++;
            continue;
        }

        if (c == '"') {
            lex_pos++;
            int start = lex_pos;
            while (lex_pos < lex_len && lex_src[lex_pos] != '"') {
                if (lex_src[lex_pos] == '\n') lex_line++;
                lex_pos++;
            }
            if (lex_pos >= lex_len) fatal("line %d: unterminated string", lex_line);
            t.type = TOK_STR;
            t.val.str = sdupn(lex_src + start, lex_pos - start);
            lex_pos++;
            return t;
        }

        if (c == '`') {
            lex_pos++;
            int start = lex_pos;
            while (lex_pos < lex_len && lex_src[lex_pos] != '`') {
                if (lex_src[lex_pos] == '\n') lex_line++;
                lex_pos++;
            }
            if (lex_pos >= lex_len) fatal("line %d: unterminated template", lex_line);
            t.type = TOK_TEMPLATE;
            t.val.str = sdupn(lex_src + start, lex_pos - start);
            lex_pos++;
            return t;
        }

        if (isdigit(c)) {
            char *end;
            t.val.num = strtod(lex_src + lex_pos, &end);
            if (end == lex_src + lex_pos) { lex_pos++; continue; }
            t.type = TOK_NUM;
            lex_pos = end - lex_src;
            return t;
        }

        if (isalpha(c) || c == '_') {
            int start = lex_pos;
            while (lex_pos < lex_len && (isalnum(lex_src[lex_pos]) || lex_src[lex_pos] == '_')) lex_pos++;
            size_t n = lex_pos - start;
            char *word = sdupn(lex_src + start, n);

            if (!strcmp(word, "print"))   { free(word); t.type = TOK_PRINT; return t; }
            if (!strcmp(word, "input"))   { free(word); t.type = TOK_INPUT; return t; }
            if (!strcmp(word, "if"))      { free(word); t.type = TOK_IF; return t; }
            if (!strcmp(word, "else"))    { free(word); t.type = TOK_ELSE; return t; }
            if (!strcmp(word, "elif"))    { free(word); t.type = TOK_ELIF; return t; }
            if (!strcmp(word, "while"))   { free(word); t.type = TOK_WHILE; return t; }
            if (!strcmp(word, "for"))     { free(word); t.type = TOK_FOR; return t; }
            if (!strcmp(word, "to"))      { free(word); t.type = TOK_TO; return t; }
            if (!strcmp(word, "exit"))    { free(word); t.type = TOK_EXIT; return t; }
            if (!strcmp(word, "and"))     { free(word); t.type = TOK_AND; return t; }
            if (!strcmp(word, "or"))      { free(word); t.type = TOK_OR; return t; }
            if (!strcmp(word, "not"))     { free(word); t.type = TOK_NOT; return t; }
            if (!strcmp(word, "loop"))    { free(word); t.type = TOK_LOOP; return t; }
            if (!strcmp(word, "stop"))    { free(word); t.type = TOK_STOP; return t; }
            if (!strcmp(word, "include")) { free(word); t.type = TOK_INCLUDE; return t; }
            if (!strcmp(word, "function")) { free(word); t.type = TOK_FUNCTION; return t; }
            if (!strcmp(word, "has"))     { free(word); t.type = TOK_HAS; return t; }
            if (!strcmp(word, "nocase"))  { free(word); t.type = TOK_NOCASE; return t; }
            if (!strcmp(word, "anywhere")) { free(word); t.type = TOK_ANYWHERE; return t; }
            if (!strcmp(word, "word"))    { free(word); t.type = TOK_WORD; return t; }

            t.type = TOK_ID;
            t.val.str = word;
            return t;
        }

        lex_pos++;
        switch (c) {
            case '+': t.type = TOK_PLUS; return t;
            case '-': t.type = TOK_MINUS; return t;
            case '*': t.type = TOK_STAR; return t;
            case '/': t.type = TOK_SLASH; return t;
            case '%': t.type = TOK_MOD; return t;
            case '(': t.type = TOK_LPAREN; return t;
            case ')': t.type = TOK_RPAREN; return t;
            case '{': t.type = TOK_LBRACE; return t;
            case '}': t.type = TOK_RBRACE; return t;
            case '[': t.type = TOK_LBRACKET; return t;
            case ']': t.type = TOK_RBRACKET; return t;
            case ',': t.type = TOK_COMMA; return t;
            case ';': t.type = TOK_SEMI; return t;
            case '=':
                if (lex_pos < lex_len && lex_src[lex_pos] == '=') { lex_pos++; t.type = TOK_EQ; return t; }
                t.type = TOK_ASSIGN; return t;
            case '!':
                if (lex_pos < lex_len && lex_src[lex_pos] == '=') { lex_pos++; t.type = TOK_NE; return t; }
                t.type = TOK_NOT; return t;
            case '<':
                if (lex_pos < lex_len && lex_src[lex_pos] == '=') { lex_pos++; t.type = TOK_LE; return t; }
                t.type = TOK_LT; return t;
            case '>':
                if (lex_pos < lex_len && lex_src[lex_pos] == '=') { lex_pos++; t.type = TOK_GE; return t; }
                t.type = TOK_GT; return t;
            case '&':
                if (lex_pos < lex_len && lex_src[lex_pos] == '&') { lex_pos++; t.type = TOK_AND; return t; }
                fatal("line %d: expected '&&'", lex_line); return t;
            case '|':
                if (lex_pos < lex_len && lex_src[lex_pos] == '|') { lex_pos++; t.type = TOK_OR; return t; }
                fatal("line %d: expected '||'", lex_line); return t;
            case '$': {
                if (lex_pos < lex_len && (isalpha(lex_src[lex_pos]) || lex_src[lex_pos] == '_')) {
                    int start = lex_pos;
                    while (lex_pos < lex_len && (isalnum(lex_src[lex_pos]) || lex_src[lex_pos] == '_')) lex_pos++;
                    t.type = TOK_DOLLAR_ID;
                    t.val.str = sdupn(lex_src + start, lex_pos - start);
                    return t;
                }
                t.type = TOK_DOLLAR_ID;
                t.val.str = sdup("");
                return t;
            }
            default:
                fatal("line %d: unexpected character '%c' (%d)", lex_line, c, c); return t;
        }
    }
    t.type = TOK_EOF;
    return t;
}

static void lex_init(const char *src) {
    lex_src = src;
    lex_pos = 0;
    lex_len = strlen(src);
    lex_line = 1;
    lex_has_peek = 0;
}

static ASTNode *ast_alloc(NodeType type) {
    ASTNode *n = safe_alloc(sizeof(ASTNode));
    n->type = type;
    n->line = lex_cur.line;
    return n;
}

static ASTNode *ast_num(double v) {
    ASTNode *n = ast_alloc(NODE_NUM);
    n->data.num = v;
    return n;
}

static ASTNode *ast_str(const char *v) {
    ASTNode *n = ast_alloc(NODE_STR);
    n->data.str = sdup(v);
    return n;
}

static ASTNode *ast_id(const char *name) {
    ASTNode *n = ast_alloc(NODE_ID);
    n->data.id = sdup(name);
    return n;
}

static ASTNode *ast_binop(int op, ASTNode *l, ASTNode *r) {
    ASTNode *n = ast_alloc(NODE_BINOP);
    n->data.binop.op = op;
    n->data.binop.left = l;
    n->data.binop.right = r;
    return n;
}

static ASTNode *ast_unary(int op, ASTNode *o) {
    ASTNode *n = ast_alloc(NODE_UNARY);
    n->data.unary.op = op;
    n->data.unary.operand = o;
    return n;
}

static ASTNode *ast_assign(const char *name, ASTNode *val) {
    ASTNode *n = ast_alloc(NODE_ASSIGN);
    n->data.assign.name = sdup(name);
    n->data.assign.value = val;
    return n;
}

static ASTNode *ast_print(ASTNode **exprs, int count) {
    ASTNode *n = ast_alloc(NODE_PRINT);
    n->data.print.exprs = exprs;
    n->data.print.count = count;
    n->data.print.cap = count;
    return n;
}

static ASTNode *ast_input(const char *name, const char *prompt) {
    ASTNode *n = ast_alloc(NODE_INPUT);
    n->data.input.name = sdup(name);
    n->data.input.prompt = prompt ? sdup(prompt) : NULL;
    return n;
}

static ASTNode *ast_if(ASTNode *cond, ASTNode *then, ASTNode *els) {
    ASTNode *n = ast_alloc(NODE_IF);
    n->data.if_stmt.cond = cond;
    n->data.if_stmt.then = then;
    n->data.if_stmt.els = els;
    n->data.if_stmt.flags = 0;
    n->data.if_stmt.has_mode = 0;
    n->data.if_stmt.has_item = NULL;
    n->data.if_stmt.has_items = NULL;
    n->data.if_stmt.has_nitems = 0;
    return n;
}

static ASTNode *ast_while(ASTNode *cond, ASTNode *body) {
    ASTNode *n = ast_alloc(NODE_WHILE);
    n->data.while_stmt.cond = cond;
    n->data.while_stmt.body = body;
    return n;
}

static ASTNode *ast_forto(const char *var, ASTNode *start, ASTNode *end, ASTNode *body) {
    ASTNode *n = ast_alloc(NODE_FORTO);
    n->data.forto.var = sdup(var);
    n->data.forto.start = start;
    n->data.forto.end = end;
    n->data.forto.body = body;
    return n;
}

static ASTNode *ast_loop(ASTNode *body) {
    ASTNode *n = ast_alloc(NODE_LOOP);
    n->data.loop.body = body;
    return n;
}

static ASTNode *ast_stop(void) {
    return ast_alloc(NODE_STOP);
}

static ASTNode *ast_include(const char *name) {
    ASTNode *n = ast_alloc(NODE_INCLUDE);
    n->data.include.name = sdup(name);
    return n;
}

static ASTNode *ast_function(const char *lib, char **args, int argc) {
    ASTNode *n = ast_alloc(NODE_FUNCTION);
    n->data.funcall.lib = sdup(lib);
    n->data.funcall.args = args;
    n->data.funcall.argc = argc;
    return n;
}

static ASTNode *ast_templ(const char *raw) {
    ASTNode *n = ast_alloc(NODE_TEMPLATE);
    n->data.templ.raw = sdup(raw);
    return n;
}

static ASTNode *ast_block(void) {
    ASTNode *n = ast_alloc(NODE_BLOCK);
    n->data.block.stmts = safe_alloc(sizeof(ASTNode*) * 16);
    n->data.block.count = 0;
    n->data.block.cap = 16;
    return n;
}

static void block_add(ASTNode *block, ASTNode *stmt) {
    if (stmt && stmt->type != NODE_EMPTY) {
        if (block->data.block.count >= block->data.block.cap) {
            block->data.block.cap *= 2;
            block->data.block.stmts = realloc(block->data.block.stmts,
                sizeof(ASTNode*) * block->data.block.cap);
            if (!block->data.block.stmts) fatal("out of memory");
        }
        block->data.block.stmts[block->data.block.count++] = stmt;
    }
}

static ASTNode *parse_expr(void);
static ASTNode *parse_stmt(void);
static ASTNode *parse_block(void);
static ASTNode *parse_else_chain(void);

static ASTNode *parse_program(void) {
    ASTNode *prog = ast_block();
    while (lex_cur.type != TOK_EOF) {
        ASTNode *s = parse_stmt();
        if (s->type != NODE_EMPTY) block_add(prog, s);
        if (lex_cur.type == TOK_NEWLINE) lex_next();
    }
    return prog;
}

static ASTNode *parse_block(void) {
    ASTNode *b = ast_block();
    lex_next();
    while (lex_cur.type != TOK_RBRACE && lex_cur.type != TOK_EOF) {
        if (lex_cur.type == TOK_NEWLINE) { lex_next(); continue; }
        ASTNode *s = parse_stmt();
        if (s->type != NODE_EMPTY) block_add(b, s);
        if (lex_cur.type == TOK_NEWLINE) lex_next();
    }
    if (lex_cur.type != TOK_RBRACE) fatal("line %d: expected '}'", lex_cur.line);
    lex_next();
    return b;
}

static ASTNode *parse_else_chain(void) {
    if (lex_cur.type == TOK_ELIF) {
        lex_next();
        ASTNode *cond, *body;
        int flags = 0, has_mode = 0;
        ASTNode *has_item = NULL, **has_items = NULL;
        int has_nitems = 0;

        cond = parse_expr();

        if (lex_cur.type == TOK_HAS) {
            has_mode = 1;
            lex_next();
            if (lex_cur.type == TOK_LBRACKET) {
                lex_next();
                int cap = 0;
                while (lex_cur.type != TOK_RBRACKET && lex_cur.type != TOK_EOF) {
                    if (has_nitems >= cap) {
                        cap = cap ? cap * 2 : 4;
                        has_items = realloc(has_items, sizeof(ASTNode*) * cap);
                        if (!has_items) fatal("out of memory");
                    }
                    has_items[has_nitems++] = parse_expr();
                    if (lex_cur.type == TOK_COMMA) lex_next();
                }
                if (lex_cur.type != TOK_RBRACKET) fatal("line %d: expected ']'", lex_cur.line);
                lex_next();
            } else {
                has_item = parse_expr();
            }
            if (lex_cur.type == TOK_NOCASE) { flags |= 1; lex_next(); }
        } else {
            while (lex_cur.type == TOK_NOCASE || lex_cur.type == TOK_ANYWHERE || lex_cur.type == TOK_WORD) {
                if (lex_cur.type == TOK_NOCASE) flags |= 1;
                else if (lex_cur.type == TOK_ANYWHERE) flags |= 2;
                else if (lex_cur.type == TOK_WORD) flags |= 4;
                lex_next();
            }
        }

        if (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type != TOK_LBRACE) fatal("line %d: expected '{' after elif condition", lex_cur.line);
        body = parse_block();
        ASTNode *rest = NULL;
        while (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type == TOK_ELIF || lex_cur.type == TOK_ELSE) {
            rest = parse_else_chain();
        }

        ASTNode *n = ast_if(cond, body, rest);
        n->data.if_stmt.flags = flags;
        n->data.if_stmt.has_mode = has_mode;
        n->data.if_stmt.has_item = has_item;
        n->data.if_stmt.has_items = has_items;
        n->data.if_stmt.has_nitems = has_nitems;
        return n;
    }
    if (lex_cur.type == TOK_ELSE) {
        lex_next();
        while (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type == TOK_IF) {
            return parse_stmt();
        }
        if (lex_cur.type != TOK_LBRACE) fatal("line %d: expected '{' after else", lex_cur.line);
        return parse_block();
    }
    return NULL;
}

static ASTNode *parse_stmt(void) {
    if (lex_cur.type == TOK_NEWLINE || lex_cur.type == TOK_EOF) return ast_alloc(NODE_EMPTY);

    if (lex_cur.type == TOK_LBRACE) return parse_block();

    if (lex_cur.type == TOK_PRINT) {
        lex_next();
        ASTNode **exprs = NULL;
        int count = 0, cap = 0;
        while (lex_cur.type != TOK_NEWLINE && lex_cur.type != TOK_EOF && lex_cur.type != TOK_RBRACE) {
            if (count > 0) {
                if (lex_cur.type == TOK_COMMA) { lex_next(); }
                else break;
            }
            if (cap <= count) {
                cap = cap ? cap * 2 : 8;
                exprs = realloc(exprs, sizeof(ASTNode*) * cap);
                if (!exprs) fatal("out of memory");
            }
            exprs[count++] = parse_expr();
        }
        if (count == 0) fatal("line %d: print expects an expression", lex_cur.line);
        return ast_print(exprs, count);
    }

    if (lex_cur.type == TOK_INPUT) {
        lex_next();
        char *prompt = NULL;
        if (lex_cur.type == TOK_STR) {
            prompt = sdup(lex_cur.val.str);
            lex_next();
        }
        if (lex_cur.type != TOK_ID) fatal("line %d: input expects a variable name", lex_cur.line);
        char *name = sdup(lex_cur.val.str);
        lex_next();
        ASTNode *n = ast_input(name, prompt);
        if (prompt) free(prompt);
        return n;
    }

    if (lex_cur.type == TOK_IF) {
        lex_next();
        ASTNode *cond, *then, *els = NULL;
        int flags = 0, has_mode = 0;
        ASTNode *has_item = NULL, **has_items = NULL;
        int has_nitems = 0;

        cond = parse_expr();

        if (lex_cur.type == TOK_HAS) {
            has_mode = 1;
            lex_next();
            if (lex_cur.type == TOK_LBRACKET) {
                lex_next();
                int cap = 0;
                while (lex_cur.type != TOK_RBRACKET && lex_cur.type != TOK_EOF) {
                    if (has_nitems >= cap) {
                        cap = cap ? cap * 2 : 4;
                        has_items = realloc(has_items, sizeof(ASTNode*) * cap);
                        if (!has_items) fatal("out of memory");
                    }
                    has_items[has_nitems++] = parse_expr();
                    if (lex_cur.type == TOK_COMMA) lex_next();
                }
                if (lex_cur.type != TOK_RBRACKET) fatal("line %d: expected ']'", lex_cur.line);
                lex_next();
            } else {
                has_item = parse_expr();
            }
            if (lex_cur.type == TOK_NOCASE) { flags |= 1; lex_next(); }
        } else {
            while (lex_cur.type == TOK_NOCASE || lex_cur.type == TOK_ANYWHERE || lex_cur.type == TOK_WORD) {
                if (lex_cur.type == TOK_NOCASE) flags |= 1;
                else if (lex_cur.type == TOK_ANYWHERE) flags |= 2;
                else if (lex_cur.type == TOK_WORD) flags |= 4;
                lex_next();
            }
        }

        if (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type != TOK_LBRACE) fatal("line %d: expected '{' after if condition", lex_cur.line);
        then = parse_block();
        while (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type == TOK_ELIF || lex_cur.type == TOK_ELSE) {
            els = parse_else_chain();
        }

        ASTNode *n = ast_if(cond, then, els);
        n->data.if_stmt.flags = flags;
        n->data.if_stmt.has_mode = has_mode;
        n->data.if_stmt.has_item = has_item;
        n->data.if_stmt.has_items = has_items;
        n->data.if_stmt.has_nitems = has_nitems;
        return n;
    }

    if (lex_cur.type == TOK_WHILE) {
        lex_next();
        ASTNode *cond = parse_expr();
        if (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type != TOK_LBRACE) fatal("line %d: expected '{' after while condition", lex_cur.line);
        ASTNode *body = parse_block();
        return ast_while(cond, body);
    }

    if (lex_cur.type == TOK_FOR) {
        lex_next();
        if (lex_cur.type != TOK_ID) fatal("line %d: for expects a variable name", lex_cur.line);
        char *var = sdup(lex_cur.val.str);
        lex_next();
        if (lex_cur.type != TOK_ASSIGN) fatal("line %d: for expects '=' after variable", lex_cur.line);
        lex_next();
        ASTNode *start = parse_expr();
        if (lex_cur.type != TOK_TO) fatal("line %d: for expects 'to' after start expression", lex_cur.line);
        lex_next();
        ASTNode *end = parse_expr();
        if (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type != TOK_LBRACE) fatal("line %d: expected '{' after for clause", lex_cur.line);
        ASTNode *body = parse_block();
        return ast_forto(var, start, end, body);
    }

    if (lex_cur.type == TOK_EXIT) {
        lex_next();
        return ast_alloc(NODE_EXIT);
    }

    if (lex_cur.type == TOK_LOOP) {
        lex_next();
        if (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type != TOK_LBRACE) fatal("line %d: expected '{' after loop", lex_cur.line);
        ASTNode *body = parse_block();
        return ast_loop(body);
    }

    if (lex_cur.type == TOK_STOP) {
        lex_next();
        return ast_stop();
    }

    if (lex_cur.type == TOK_INCLUDE) {
        lex_next();
        if (lex_cur.type != TOK_ID) fatal("line %d: include expects a library name", lex_cur.line);
        char *name = sdup(lex_cur.val.str);
        lex_next();
        return ast_include(name);
    }

    if (lex_cur.type == TOK_ID && lex_peek_next().type == TOK_ASSIGN) {
        char *name = sdup(lex_cur.val.str);
        lex_next();
        lex_next();
        ASTNode *val = parse_expr();
        return ast_assign(name, val);
    }

    ASTNode *e = parse_expr();
    return e;
}

static ASTNode *parse_primary(void) {
    if (lex_cur.type == TOK_NUM) {
        double v = lex_cur.val.num;
        lex_next();
        return ast_num(v);
    }
    if (lex_cur.type == TOK_STR) {
        char *s = sdup(lex_cur.val.str);
        lex_next();
        return ast_str(s);
    }
    if (lex_cur.type == TOK_TEMPLATE) {
        char *r = sdup(lex_cur.val.str);
        lex_next();
        return ast_templ(r);
    }
    if (lex_cur.type == TOK_FUNCTION) {
        lex_next();
        if (lex_cur.type != TOK_ID) fatal("line %d: function expects a library name", lex_cur.line);
        char *lib = sdup(lex_cur.val.str);
        lex_next();
        char **args = NULL;
        int argc = 0, cap = 0;
        while (lex_cur.type == TOK_ID || lex_cur.type == TOK_STR || lex_cur.type == TOK_NUM || lex_cur.type == TOK_TEMPLATE || lex_cur.type == TOK_DOLLAR_ID) {
            if (argc >= cap) {
                cap = cap ? cap * 2 : 4;
                args = realloc(args, sizeof(char*) * cap);
                if (!args) fatal("out of memory");
            }
            if (lex_cur.type == TOK_NUM) {
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "%.10g", lex_cur.val.num);
                args[argc++] = sdup(tmp);
            } else if (lex_cur.type == TOK_DOLLAR_ID) {
                char *prefixed = malloc(strlen(lex_cur.val.str) + 2);
                prefixed[0] = '$';
                strcpy(prefixed + 1, lex_cur.val.str);
                args[argc++] = prefixed;
            } else {
                args[argc++] = sdup(lex_cur.val.str);
            }
            lex_next();
        }
        return ast_function(lib, args, argc);
    }
    if (lex_cur.type == TOK_ID) {
        char *name = sdup(lex_cur.val.str);
        lex_next();
        return ast_id(name);
    }
    if (lex_cur.type == TOK_LPAREN) {
        lex_next();
        ASTNode *e = parse_expr();
        if (lex_cur.type != TOK_RPAREN) fatal("line %d: expected ')'", lex_cur.line);
        lex_next();
        return e;
    }
    if (lex_cur.type == TOK_MINUS) {
        lex_next();
        ASTNode *o = parse_primary();
        return ast_unary(TOK_MINUS, o);
    }
    if (lex_cur.type == TOK_NOT) {
        lex_next();
        ASTNode *o = parse_primary();
        return ast_unary(TOK_NOT, o);
    }
    fatal("line %d: unexpected token", lex_cur.line);
    return NULL;
}

static ASTNode *parse_unary(void) {
    if (lex_cur.type == TOK_MINUS) {
        lex_next();
        ASTNode *o = parse_unary();
        return ast_unary(TOK_MINUS, o);
    }
    if (lex_cur.type == TOK_NOT) {
        lex_next();
        ASTNode *o = parse_unary();
        return ast_unary(TOK_NOT, o);
    }
    return parse_primary();
}

static ASTNode *parse_mul(void) {
    ASTNode *l = parse_unary();
    while (lex_cur.type == TOK_STAR || lex_cur.type == TOK_SLASH || lex_cur.type == TOK_MOD) {
        int op = lex_cur.type;
        lex_next();
        ASTNode *r = parse_unary();
        l = ast_binop(op, l, r);
    }
    return l;
}

static ASTNode *parse_add(void) {
    ASTNode *l = parse_mul();
    while (lex_cur.type == TOK_PLUS || lex_cur.type == TOK_MINUS) {
        int op = lex_cur.type;
        lex_next();
        ASTNode *r = parse_mul();
        l = ast_binop(op, l, r);
    }
    return l;
}

static ASTNode *parse_cmp(void) {
    ASTNode *l = parse_add();
    while (lex_cur.type == TOK_LT || lex_cur.type == TOK_GT || lex_cur.type == TOK_LE ||
           lex_cur.type == TOK_GE || lex_cur.type == TOK_EQ || lex_cur.type == TOK_NE) {
        int op = lex_cur.type;
        lex_next();
        ASTNode *r = parse_add();
        l = ast_binop(op, l, r);
    }
    return l;
}

static ASTNode *parse_and(void) {
    ASTNode *l = parse_cmp();
    while (lex_cur.type == TOK_AND) {
        lex_next();
        ASTNode *r = parse_cmp();
        l = ast_binop(TOK_AND, l, r);
    }
    return l;
}

static ASTNode *parse_or(void) {
    ASTNode *l = parse_and();
    while (lex_cur.type == TOK_OR) {
        lex_next();
        ASTNode *r = parse_and();
        l = ast_binop(TOK_OR, l, r);
    }
    return l;
}

static ASTNode *parse_expr(void) {
    return parse_or();
}

static char *resolve_arg(char *arg) {
    if (arg[0] == '$') {
        Value v = var_get(arg + 1);
        char *s = val_tostr(v);
        return s;
    }
    return sdup(arg);
}

static Value eval_expr(ASTNode *n) {
    if (!n) return make_num(0);
    switch (n->type) {
        case NODE_NUM:
            return make_num(n->data.num);
        case NODE_STR:
            return make_str(n->data.str);
        case NODE_TEMPLATE: {
            char *raw = n->data.templ.raw;
            char res[2048] = {0};
            size_t ri = 0, pos = 0, len = strlen(raw);
            while (pos < len && ri < sizeof(res) - 1) {
                if (raw[pos] == '{') {
                    pos++;
                    size_t start = pos;
                    while (pos < len && raw[pos] != '}') pos++;
                    if (pos >= len) fatal("line %d: unclosed '{' in template", n->line);
                    size_t vlen = pos - start;
                    char vname[256];
                    memcpy(vname, raw + start, vlen);
                    vname[vlen] = 0;
                    Value v = var_get(vname);
                    char *vs = val_tostr(v);
                    size_t sl = strlen(vs);
                    if (ri + sl >= sizeof(res) - 1) sl = sizeof(res) - ri - 1;
                    memcpy(res + ri, vs, sl); ri += sl;
                    free(vs);
                    pos++;
                } else {
                    res[ri++] = raw[pos++];
                }
            }
            res[ri] = 0;
            return make_str(res);
        }
        case NODE_ID:
            return var_get(n->data.id);
        case NODE_UNARY: {
            Value o = eval_expr(n->data.unary.operand);
            if (n->data.unary.op == TOK_MINUS) {
                if (o.type == VAL_STR) fatal("line %d: cannot negate a string", n->line);
                return make_num(-o.data.num);
            }
            if (n->data.unary.op == TOK_NOT) {
                if (o.type == VAL_STR) return make_num(strlen(o.data.str) == 0 ? 1 : 0);
                return make_num(o.data.num == 0 ? 1 : 0);
            }
            return o;
        }
        case NODE_BINOP: {
            Value l = eval_expr(n->data.binop.left);
            Value r = eval_expr(n->data.binop.right);
            int op = n->data.binop.op;

            if (op == TOK_PLUS) {
                if (l.type == VAL_STR || r.type == VAL_STR) {
                    char *ls = val_tostr(l);
                    char *rs = val_tostr(r);
                    char *res = malloc(strlen(ls) + strlen(rs) + 1);
                    if (!res) fatal("out of memory");
                    strcpy(res, ls);
                    strcat(res, rs);
                    free(ls); free(rs);
                    return make_str(res);
                }
                return make_num(l.data.num + r.data.num);
            }
            if (op == TOK_MINUS) return make_num(val_tonum(l) - val_tonum(r));
            if (op == TOK_STAR)   return make_num(val_tonum(l) * val_tonum(r));
            if (op == TOK_SLASH) {
                double rd = val_tonum(r);
                if (rd == 0) fatal("line %d: division by zero", n->line);
                return make_num(val_tonum(l) / rd);
            }
            if (op == TOK_MOD) {
                long rd = (long)val_tonum(r);
                if (rd == 0) fatal("line %d: modulo by zero", n->line);
                return make_num((long)val_tonum(l) % rd);
            }

            int result = 0;
            if (op == TOK_EQ || op == TOK_NE || op == TOK_LT || op == TOK_GT || op == TOK_LE || op == TOK_GE) {
                if (l.type == VAL_STR || r.type == VAL_STR) {
                    char *ls = val_tostr(l);
                    char *rs = val_tostr(r);
                    int cmp = strcmp(ls, rs);
                    free(ls); free(rs);
                    switch (op) {
                        case TOK_EQ: result = (cmp == 0); break;
                        case TOK_NE: result = (cmp != 0); break;
                        case TOK_LT: result = (cmp < 0); break;
                        case TOK_GT: result = (cmp > 0); break;
                        case TOK_LE: result = (cmp <= 0); break;
                        case TOK_GE: result = (cmp >= 0); break;
                    }
                } else {
                    double lv = l.data.num, rv = r.data.num;
                    switch (op) {
                        case TOK_EQ: result = (lv == rv); break;
                        case TOK_NE: result = (lv != rv); break;
                        case TOK_LT: result = (lv < rv); break;
                        case TOK_GT: result = (lv > rv); break;
                        case TOK_LE: result = (lv <= rv); break;
                        case TOK_GE: result = (lv >= rv); break;
                    }
                }
            } else {
                int lv = (l.type == VAL_STR) ? (strlen(l.data.str) != 0) : (l.data.num != 0);
                int rv = (r.type == VAL_STR) ? (strlen(r.data.str) != 0) : (r.data.num != 0);
                switch (op) {
                    case TOK_AND: result = (lv && rv); break;
                    case TOK_OR: result = (lv || rv); break;
                    default: fatal("line %d: unknown operator", n->line);
                }
            }
            return make_num(result);
        }
        case NODE_FUNCTION: {
            char buf[256];
            if (n->data.funcall.argc < 1) {
                fatal("line %d: function expects a name", n->line);
            }
            char *fn = n->data.funcall.args[0];

            if (!strcmp(n->data.funcall.lib, "date")) {
                time_t t = time(NULL);
                struct tm *tm = localtime(&t);
                static int date_mode = 0;

                if (!strcmp(fn, "set")) {
                    if (n->data.funcall.argc > 1 && !strcmp(n->data.funcall.args[1], "format")) {
                        if (n->data.funcall.argc < 3) fatal("line %d: set format expects a region", n->line);
                        if (!strcmp(n->data.funcall.args[2], "US")) date_mode = 1;
                        else if (!strcmp(n->data.funcall.args[2], "EU")) date_mode = 0;
                        else fatal("line %d: unknown date format '%s'", n->line, n->data.funcall.args[2]);
                    }
                    return make_str("");
                }

                char *fmt_min, *fmt_std, *fmt_stdp, *fmt_full, *fmt_fullns, *fmt_fullnd, *fmt_fp;
                if (date_mode) {
                    fmt_min    = "%-m/%-d/%y"; fmt_std  = "%m/%d/%Y"; fmt_stdp = "%m/%d/%Y %A";
                    fmt_full   = "%m/%d/%Y %A %H:%M:%S"; fmt_fullns = "%m/%d/%Y %A %H:%M";
                    fmt_fullnd = "%m/%d/%Y %H:%M:%S"; fmt_fp = "%m/%d/%Y %A %H:%M:%S";
                } else {
                    fmt_min    = "%-d.%-m.%y"; fmt_std  = "%d.%m.%Y"; fmt_stdp = "%d.%m.%Y %A";
                    fmt_full   = "%d.%m.%Y %A %H:%M:%S"; fmt_fullns = "%d.%m.%Y %A %H:%M";
                    fmt_fullnd = "%d.%m.%Y %H:%M:%S"; fmt_fp = "%d.%m.%Y %A %H:%M:%S";
                }

                if (!strcmp(fn, "minimal")) { strftime(buf, sizeof(buf), fmt_min, tm); return make_str(buf); }
                if (!strcmp(fn, "standart")) { strftime(buf, sizeof(buf), fmt_std, tm); return make_str(buf); }
                if (!strcmp(fn, "standartplus")) { strftime(buf, sizeof(buf), fmt_stdp, tm); return make_str(buf); }
                if (!strcmp(fn, "full")) {
                    if (n->data.funcall.argc > 1 && !strcmp(n->data.funcall.args[1], "noseconds"))
                        strftime(buf, sizeof(buf), fmt_fullns, tm);
                    else if (n->data.funcall.argc > 1 && !strcmp(n->data.funcall.args[1], "noday"))
                        strftime(buf, sizeof(buf), fmt_fullnd, tm);
                    else strftime(buf, sizeof(buf), fmt_full, tm);
                    return make_str(buf);
                }
                if (!strcmp(fn, "fullplus")) {
                    struct timeval tv; gettimeofday(&tv, NULL);
                    strftime(buf, sizeof(buf), fmt_fp, tm);
                    size_t blen = strlen(buf);
                    snprintf(buf + blen, sizeof(buf) - blen, ".%03ld", tv.tv_usec / 1000);
                    return make_str(buf);
                }
                fatal("line %d: unknown date function '%s'", n->line, fn);
            }

            if (!strcmp(n->data.funcall.lib, "math")) {
                if (!strcmp(fn, "hasops")) {
                    if (n->data.funcall.argc < 2) fatal("line %d: math hasops expects a string", n->line);
                    char *arg = resolve_arg(n->data.funcall.args[1]);
                    int found = 0;
                    for (char *p = arg; *p; p++) {
                        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '%') { found = 1; break; }
                    }
                    free(arg);
                    return make_num(found);
                }
                if (!strcmp(fn, "eval")) {
                    if (n->data.funcall.argc < 2) fatal("line %d: math eval expects a string expression", n->line);
                    char *expr_str = resolve_arg(n->data.funcall.args[1]);
                    const char *p = expr_str;
                    double result = math_parse_expr(&p);
                    while (*p == ' ') p++;
                    if (*p) { free(expr_str); fatal("line %d: unexpected '%c' in math expression", n->line, *p); }
                    free(expr_str);
                    return make_num(result);
                }
                if (!strcmp(fn, "random")) {
                    static int seeded = 0;
                    if (!seeded) { srand(time(NULL)); seeded = 1; }
                    return make_num((double)rand() / RAND_MAX);
                }
                if (!strcmp(fn, "randint")) {
                    static int seeded = 0;
                    if (!seeded) { srand(time(NULL)); seeded = 1; }
                    if (n->data.funcall.argc < 3) fatal("line %d: randint expects min and max", n->line);
                    int lo = (int)strtod(n->data.funcall.args[1], NULL);
                    int hi = (int)strtod(n->data.funcall.args[2], NULL);
                    if (lo > hi) { int t = lo; lo = hi; hi = t; }
                    return make_num(lo + rand() % (hi - lo + 1));
                }
                if (!strcmp(fn, "choice")) {
                    static int seeded = 0;
                    if (!seeded) { srand(time(NULL)); seeded = 1; }
                    if (n->data.funcall.argc < 2) fatal("line %d: choice expects at least one option", n->line);
                    int idx = rand() % (n->data.funcall.argc - 1) + 1;
                    return make_str(n->data.funcall.args[idx]);
                }
                if (!strcmp(fn, "sleep")) {
                    if (n->data.funcall.argc < 2) fatal("line %d: sleep expects seconds", n->line);
                    double secs = strtod(n->data.funcall.args[1], NULL);
                    if (secs > 0) {
                        struct timespec ts;
                        ts.tv_sec = (time_t)secs;
                        ts.tv_nsec = (long)((secs - (time_t)secs) * 1e9);
                        nanosleep(&ts, NULL);
                    }
                    return make_num(0);
                }
                if (!strcmp(fn, "factors")) {
                    if (n->data.funcall.argc < 2) fatal("line %d: factors expects a number", n->line);
                    char *arg = resolve_arg(n->data.funcall.args[1]);
                    long num = (long)strtod(arg, NULL); free(arg);
                    if (num < 1) return make_str("");
                    char buf[4096] = {0}; size_t pos = 0;
                    for (long i = 1; i <= num; i++) {
                        if (num % i == 0) {
                            int r = snprintf(buf + pos, sizeof(buf) - pos, "%ld ", i);
                            if (r > 0) pos += r;
                            if (pos >= sizeof(buf) - 10) break;
                        }
                    }
                    return make_str(buf);
                }
                if (!strcmp(fn, "fib")) {
                    if (n->data.funcall.argc < 2) fatal("line %d: fib expects a number", n->line);
                    char *arg = resolve_arg(n->data.funcall.args[1]);
                    long cnt = (long)strtod(arg, NULL); free(arg);
                    if (cnt < 1) return make_str("");
                    if (cnt > 100) cnt = 100;
                    char buf[4096] = {0}; size_t pos = 0;
                    long a = 0, b = 1;
                    for (long i = 0; i < cnt; i++) {
                        int r = snprintf(buf + pos, sizeof(buf) - pos, "%ld ", a);
                        if (r > 0) pos += r;
                        if (pos >= sizeof(buf) - 20) break;
                        long c = a + b; a = b; b = c;
                    }
                    return make_str(buf);
                }
                if (!strcmp(fn, "isprime")) {
                    if (n->data.funcall.argc < 2) fatal("line %d: isprime expects a number", n->line);
                    char *arg = resolve_arg(n->data.funcall.args[1]);
                    long num = (long)strtod(arg, NULL); free(arg);
                    if (num < 2) return make_num(0);
                    int prime = 1;
                    for (long i = 2; i * i <= num; i++) {
                        if (num % i == 0) { prime = 0; break; }
                    }
                    return make_num(prime);
                }
                fatal("line %d: unknown math function '%s'", n->line, fn);
            }

            if (!strcmp(n->data.funcall.lib, "string")) {
                if (!strcmp(fn, "upper")) {
                    if (n->data.funcall.argc < 2) fatal("line %d: string upper expects a string", n->line);
                    char *arg = resolve_arg(n->data.funcall.args[1]);
                    for (char *p = arg; *p; p++) *p = toupper((unsigned char)*p);
                    Value v = make_str(arg);
                    free(arg);
                    return v;
                }
                if (!strcmp(fn, "lower")) {
                    if (n->data.funcall.argc < 2) fatal("line %d: string lower expects a string", n->line);
                    char *arg = resolve_arg(n->data.funcall.args[1]);
                    for (char *p = arg; *p; p++) *p = tolower((unsigned char)*p);
                    Value v = make_str(arg);
                    free(arg);
                    return v;
                }
                if (!strcmp(fn, "reverse")) {
                    if (n->data.funcall.argc < 2) fatal("line %d: string reverse expects a string", n->line);
                    char *arg = resolve_arg(n->data.funcall.args[1]);
                    size_t len = strlen(arg);
                    for (size_t i = 0; i < len / 2; i++) {
                        char t = arg[i]; arg[i] = arg[len-1-i]; arg[len-1-i] = t;
                    }
                    Value v = make_str(arg);
                    free(arg);
                    return v;
                }
                if (!strcmp(fn, "len")) {
                    if (n->data.funcall.argc < 2) fatal("line %d: string len expects a string", n->line);
                    char *arg = resolve_arg(n->data.funcall.args[1]);
                    double l = (double)strlen(arg);
                    free(arg);
                    return make_num(l);
                }
                if (!strcmp(fn, "substr")) {
                    if (n->data.funcall.argc < 4) fatal("line %d: string substr expects string, start, end", n->line);
                    char *arg = resolve_arg(n->data.funcall.args[1]);
                    int start = (int)strtod(n->data.funcall.args[2], NULL);
                    int end = (int)strtod(n->data.funcall.args[3], NULL);
                    size_t len = strlen(arg);
                    if (start < 0) start = 0;
                    if (end > (int)len) end = (int)len;
                    if (start >= end) { free(arg); return make_str(""); }
                    char *res = sdupn(arg + start, end - start);
                    free(arg);
                    return make_str(res);
                }
                if (!strcmp(fn, "replace")) {
                    if (n->data.funcall.argc < 4) fatal("line %d: string replace expects string, old, new", n->line);
                    char *arg = resolve_arg(n->data.funcall.args[1]);
                    char *old = resolve_arg(n->data.funcall.args[2]);
                    char *new = resolve_arg(n->data.funcall.args[3]);
                    size_t olen = strlen(old), nlen = strlen(new);
                    char buf[4096] = {0};
                    size_t ri = 0, pos = 0, len = strlen(arg);
                    while (pos < len && ri < sizeof(buf) - 1) {
                        if (pos + olen <= len && memcmp(arg + pos, old, olen) == 0) {
                            size_t av = sizeof(buf) - ri - 1;
                            size_t cp = nlen < av ? nlen : av;
                            memcpy(buf + ri, new, cp); ri += cp;
                            pos += olen;
                        } else {
                            buf[ri++] = arg[pos++];
                        }
                    }
                    buf[ri] = 0;
                    free(arg); free(old); free(new);
                    return make_str(buf);
                }
                if (!strcmp(fn, "repeat")) {
                    if (n->data.funcall.argc < 3) fatal("line %d: string repeat expects string and count", n->line);
                    char *arg = resolve_arg(n->data.funcall.args[1]);
                    int count = (int)strtod(n->data.funcall.args[2], NULL);
                    if (count < 0) count = 0;
                    if (count > 1000) count = 1000;
                    size_t slen = strlen(arg);
                    char *res = malloc(slen * count + 1);
                    if (!res) fatal("out of memory");
                    res[0] = 0;
                    for (int i = 0; i < count; i++) strcat(res, arg);
                    free(arg);
                    Value v = make_str(res);
                    free(res);
                    return v;
                }
                fatal("line %d: unknown string function '%s'", n->line, fn);
            }

            fatal("line %d: unknown library '%s'", n->line, n->data.funcall.lib);
            return make_num(0);
        }
        default:
            fatal("line %d: expression expected", n->line);
            return make_num(0);
    }
}

static char *str_lower(const char *s) {
    size_t n = strlen(s);
    char *r = malloc(n + 1);
    if (!r) fatal("out of memory");
    for (size_t i = 0; i < n; i++) r[i] = tolower((unsigned char)s[i]);
    r[n] = 0;
    return r;
}

static int is_word_bound(char c) {
    return !isalnum((unsigned char)c) && c != '_';
}

static int check_cond_flags(ASTNode *cond, int flags) {
    if (cond->type != NODE_BINOP) {
        fatal("line %d: nocase/anywhere/word require a comparison", cond->line);
    }
    int op = cond->data.binop.op;
    if (op != TOK_EQ && op != TOK_NE) {
        fatal("line %d: nocase/anywhere/word only work with == or !=", cond->line);
    }

    Value lv = eval_expr(cond->data.binop.left);
    Value rv = eval_expr(cond->data.binop.right);
    char *ls = val_tostr(lv);
    char *rs = val_tostr(rv);
    int result = 0;

    if (flags & 4) {
        char *haystack = (flags & 1) ? str_lower(ls) : sdup(ls);
        char *needle = (flags & 1) ? str_lower(rs) : sdup(rs);
        size_t hl = strlen(haystack), nl = strlen(needle);
        result = 0;
        for (size_t i = 0; i + nl <= hl; i++) {
            if (memcmp(haystack + i, needle, nl) == 0) {
                int before = (i == 0) || is_word_bound(haystack[i-1]);
                int after = (i + nl >= hl) || is_word_bound(haystack[i+nl]);
                if (before && after) { result = 1; break; }
            }
        }
        free(haystack); free(needle);
    } else if (flags & 2) {
        char *haystack = (flags & 1) ? str_lower(ls) : ls;
        char *needle = (flags & 1) ? str_lower(rs) : rs;
        result = (strstr(haystack, needle) != NULL) ? 1 : 0;
        if (flags & 1) { free(haystack); free(needle); }
    } else if (flags & 1) {
        char *ll = str_lower(ls);
        char *lr = str_lower(rs);
        result = (strcmp(ll, lr) == 0) ? 1 : 0;
        free(ll); free(lr);
    }

    free(ls); free(rs);
    return (op == TOK_EQ) ? result : !result;
}

static int check_has(ASTNode *lhs_expr, ASTNode *item, ASTNode **items, int nitems, int nocase) {
    Value lv = eval_expr(lhs_expr);
    char *ls = val_tostr(lv);
    int result = 0;

    if (nitems > 0) {
        result = 1;
        for (int i = 0; i < nitems; i++) {
            Value rv = eval_expr(items[i]);
            char *rs = val_tostr(rv);

            char *haystack = nocase ? str_lower(ls) : ls;
            char *needle = nocase ? str_lower(rs) : rs;
            int found = (strstr(haystack, needle) != NULL) ? 1 : 0;
            if (nocase) { free(haystack); free(needle); }

            if (!found) result = 0;
            free(rs);
        }
    } else if (item) {
        Value rv = eval_expr(item);
        char *rs = val_tostr(rv);
        char *haystack = nocase ? str_lower(ls) : ls;
        char *needle = nocase ? str_lower(rs) : rs;
        result = (strstr(haystack, needle) != NULL) ? 1 : 0;
        if (nocase) { free(haystack); free(needle); }
        free(rs);
    }

    free(ls);
    return result;
}

static double math_parse_factor(const char **p) {
    while (**p == ' ') (*p)++;
    if (**p == '(') {
        (*p)++;
        double v = math_parse_expr(p);
        while (**p == ' ') (*p)++;
        if (**p != ')') fatal("math: expected ')'");
        (*p)++;
        return v;
    }
    if (**p == '-') {
        (*p)++;
        return -math_parse_factor(p);
    }
    char *end;
    double v = strtod(*p, &end);
    if (end == *p) fatal("math: expected number");
    *p = end;
    return v;
}

static double math_parse_term(const char **p) {
    double v = math_parse_factor(p);
    while (1) {
        while (**p == ' ') (*p)++;
        if (**p == '*') { (*p)++; v *= math_parse_factor(p); }
        else if (**p == '/') { (*p)++; double d = math_parse_factor(p); if (d == 0) fatal("math: division by zero"); v /= d; }
        else if (**p == '%') { (*p)++; long d = (long)math_parse_factor(p); if (d == 0) fatal("math: modulo by zero"); v = (long)v % d; }
        else break;
    }
    return v;
}

static double math_parse_expr(const char **p) {
    double v = math_parse_term(p);
    while (1) {
        while (**p == ' ') (*p)++;
        if (**p == '+') { (*p)++; v += math_parse_term(p); }
        else if (**p == '-') { (*p)++; v -= math_parse_term(p); }
        else break;
    }
    return v;
}

static int exec_stmt(ASTNode *n) {
    if (!n || n->type == NODE_EMPTY) return 0;
    switch (n->type) {
        case NODE_ASSIGN: {
            Value v = eval_expr(n->data.assign.value);
            var_set(n->data.assign.name, v);
            return 0;
        }
        case NODE_PRINT: {
            for (int i = 0; i < n->data.print.count; i++) {
                if (i > 0) printf(" ");
                Value v = eval_expr(n->data.print.exprs[i]);
                char *s = val_tostr(v);
                printf("%s", s);
                free(s);
            }
            printf("\n");
            fflush(stdout);
            return 0;
        }
        case NODE_INPUT: {
            if (n->data.input.prompt) {
                printf("%s", n->data.input.prompt);
                fflush(stdout);
            }
            char buf[BUF_SIZE];
            if (!fgets(buf, sizeof(buf), stdin)) {
                var_set(n->data.input.name, make_str(""));
                return 0;
            }
            size_t len = strlen(buf);
            if (len > 0 && buf[len-1] == '\n') buf[len-1] = 0;
            char *end;
            double d = strtod(buf, &end);
            if (*end == 0) {
                var_set(n->data.input.name, make_num(d));
            } else {
                var_set(n->data.input.name, make_str(buf));
            }
            return 0;
        }
        case NODE_IF: {
            int truthy;
            if (n->data.if_stmt.has_mode) {
                truthy = check_has(n->data.if_stmt.cond, n->data.if_stmt.has_item,
                    n->data.if_stmt.has_items, n->data.if_stmt.has_nitems,
                    n->data.if_stmt.flags & 1);
            } else if (n->data.if_stmt.flags) {
                truthy = check_cond_flags(n->data.if_stmt.cond, n->data.if_stmt.flags);
            } else {
                Value cv = eval_expr(n->data.if_stmt.cond);
                truthy = (cv.type == VAL_STR) ? (strlen(cv.data.str) != 0) : (cv.data.num != 0);
            }
            if (truthy) {
                int r = exec_stmt(n->data.if_stmt.then);
                if (r) return r;
            } else if (n->data.if_stmt.els) {
                int r = exec_stmt(n->data.if_stmt.els);
                if (r) return r;
            }
            return 0;
        }
        case NODE_WHILE: {
            while (1) {
                Value cv = eval_expr(n->data.while_stmt.cond);
                double c = (cv.type == VAL_STR) ? (strlen(cv.data.str) != 0) : (cv.data.num != 0);
                if (!c) break;
                int r = exec_stmt(n->data.while_stmt.body);
                if (r == 1) return 1;
                if (r == 2) break;
            }
            return 0;
        }
        case NODE_FORTO: {
            var_set(n->data.forto.var, eval_expr(n->data.forto.start));
            Value ev = eval_expr(n->data.forto.end);
            double end = ev.type == VAL_NUM ? ev.data.num : val_tonum(ev);

            while (1) {
                Value cv = var_get(n->data.forto.var);
                double c = cv.type == VAL_NUM ? cv.data.num : val_tonum(cv);
                if (c > end) break;
                int r = exec_stmt(n->data.forto.body);
                if (r == 1) return 1;
                if (r == 2) break;
                var_set(n->data.forto.var, make_num(c + 1));
            }
            return 0;
        }
        case NODE_BLOCK: {
            for (int i = 0; i < n->data.block.count; i++) {
                int r = exec_stmt(n->data.block.stmts[i]);
                if (r) return r;
            }
            return 0;
        }
        case NODE_LOOP: {
            while (1) {
                int r = exec_stmt(n->data.loop.body);
                if (r == 1) return 1;
                if (r == 2) break;
            }
            return 0;
        }
        case NODE_STOP:
            return 2;
        case NODE_INCLUDE:
            return 0;
        case NODE_EXIT:
            return 1;
        default:
            eval_expr(n);
            return 0;
    }
}

static void run_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "error: cannot open '%s': %s\n", path, strerror(errno)); return; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *src = malloc(sz + 1);
    if (!src) { fclose(f); fprintf(stderr, "out of memory\n"); return; }
    fread(src, 1, sz, f);
    fclose(f);
    src[sz] = 0;

    if (setjmp(error_jmp)) return;

    lex_init(src);
    lex_next();

    ASTNode *prog = parse_program();
    free(src);

    exec_stmt(prog);
}

static void repl(void) {
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

        lex_init(buf);
        lex_next();
        while (lex_cur.type != TOK_EOF) {
            ASTNode *s = parse_stmt();
            if (s->type != NODE_EMPTY) {
                int r = exec_stmt(s);
                if (r) return;
            }
            if (lex_cur.type == TOK_NEWLINE) lex_next();
        }
    }
}

int main(int argc, char **argv) {
    error_occurred = 0;
    if (argc > 1) {
        if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
            printf("usage: lil [file.lil]\n");
            printf("  no args    - start REPL\n");
            printf("  file.lil   - execute file\n");
            return 0;
        }
        run_file(argv[1]);
    } else {
        repl();
    }
    return error_occurred ? 1 : 0;
}
