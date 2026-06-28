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
#include <stdint.h>

#define VERSION "0.1.0"
#define MAX_VARS 1024
#define BUF_SIZE 4096

typedef enum { TOK_NUM, TOK_STR, TOK_ID, TOK_PRINT, TOK_INPUT, TOK_IF, TOK_ELSE, TOK_ELIF,
    TOK_WHILE, TOK_FOR, TOK_TO, TOK_EXIT, TOK_PLUS, TOK_MINUS, TOK_STAR,
    TOK_SLASH, TOK_MOD, TOK_EQ, TOK_NE, TOK_LT, TOK_GT, TOK_LE, TOK_GE, TOK_ASSIGN,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE, TOK_COMMA,
    TOK_SEMI, TOK_NEWLINE, TOK_EOF, TOK_AND, TOK_OR, TOK_NOT,
    TOK_LOOP, TOK_STOP, TOK_BREAK, TOK_INCLUDE, TOK_FUNCTION, TOK_STRIFY, TOK_INTIFY,
    TOK_HAS, TOK_NOCASE, TOK_ANYWHERE, TOK_WORD,
    TOK_LBRACKET, TOK_RBRACKET,
    TOK_TEMPLATE, TOK_DOLLAR_ID, TOK_AT, TOK_CARET, TOK_TRY, TOK_CATCH } TokenType;

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
    NODE_TEMPLATE, NODE_FUNC_DEF, NODE_FUNC_CALL, NODE_BREAK, NODE_STRIFY, NODE_INTIFY, NODE_TRY } NodeType;

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
        struct { char *name; struct ASTNode *body; } func_def;
        struct { char *name; } func_call;
        struct { char *name; } include;
        struct { char *lib; char **args; int argc; } funcall;
        struct { char *raw; } templ;
        struct { struct ASTNode *body, *catch_body; } try_stmt;
        struct { struct ASTNode **stmts; int count, cap; } block;
    } data;
} ASTNode;

typedef struct {
    char *name;
    Value val;
} Var;

#define MAX_FUNCS 256
typedef struct {
    char *name;
    struct ASTNode *body;
} FuncDef;

static FuncDef funcs[MAX_FUNCS];
static int func_count;

static jmp_buf error_jmp;
static int error_occurred;
static int compiled_header;
static int compile_mode;

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

static int var_ensure(const char *name) {
    int i = var_find(name);
    if (i >= 0) return i;
    if (var_count >= MAX_VARS) fatal("too many variables");
    vars[var_count].name = sdup(name);
    vars[var_count].val = (Value){VAL_NUM, {.num=0}};
    return var_count++;
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

static void val_free(Value v) {
    if (v.type == VAL_STR) free(v.data.str);
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
            while (lex_pos < lex_len && lex_src[lex_pos] != '\n' && lex_src[lex_pos] != '\r') lex_pos++;
            continue;
        }

        if (c == '"') {
            lex_pos++;
            int start = lex_pos;
            int sline = lex_line;
            while (lex_pos < lex_len && lex_src[lex_pos] != '"') {
                if (lex_src[lex_pos] == '\\' && lex_pos + 1 < lex_len) { lex_pos += 2; continue; }
                if (lex_src[lex_pos] == '\n') lex_line++;
                lex_pos++;
            }
            if (lex_pos >= lex_len) fatal("line %d: unterminated string", sline);
            t.type = TOK_STR;
            t.val.str = sdupn(lex_src + start, lex_pos - start);
            lex_pos++;
            return t;
        }

        if (c == '`') {
            lex_pos++;
            int start = lex_pos;
            int sline = lex_line;
            while (lex_pos < lex_len && lex_src[lex_pos] != '`') {
                if (lex_src[lex_pos] == '\\' && lex_pos + 1 < lex_len) { lex_pos += 2; continue; }
                if (lex_src[lex_pos] == '\n') lex_line++;
                lex_pos++;
            }
            if (lex_pos >= lex_len) fatal("line %d: unterminated template", sline);
            t.type = TOK_TEMPLATE;
            t.val.str = sdupn(lex_src + start, lex_pos - start);
            lex_pos++;
            return t;
        }

        if (isdigit(c)) {
            char *end;
            if (c == '0' && lex_pos + 1 < lex_len && (lex_src[lex_pos+1] == 'x' || lex_src[lex_pos+1] == 'X'))
                fatal("line %d: hex literals not supported, use decimal", lex_line);
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
            if (!strcmp(word, "strify"))  { free(word); t.type = TOK_STRIFY; return t; }
            if (!strcmp(word, "intify"))  { free(word); t.type = TOK_INTIFY; return t; }
            if (!strcmp(word, "try"))     { free(word); t.type = TOK_TRY; return t; }
            if (!strcmp(word, "catch"))   { free(word); t.type = TOK_CATCH; return t; }
            if (!strcmp(word, "loop"))    { free(word); t.type = TOK_LOOP; return t; }
            if (!strcmp(word, "stop"))    { free(word); t.type = TOK_STOP; return t; }
            if (!strcmp(word, "break"))   { free(word); t.type = TOK_BREAK; return t; }
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
            case '@': t.type = TOK_AT; return t;
            case '^': t.type = TOK_CARET; return t;
            case '$': {
                if (lex_pos < lex_len && (isalpha(lex_src[lex_pos]) || lex_src[lex_pos] == '_')) {
                    int start = lex_pos;
                    while (lex_pos < lex_len && (isalnum(lex_src[lex_pos]) || lex_src[lex_pos] == '_')) lex_pos++;
                    t.type = TOK_DOLLAR_ID;
                    t.val.str = sdupn(lex_src + start, lex_pos - start);
                    return t;
                }
                fatal("line %d: '$' must be followed by a variable name", lex_line);
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

static ASTNode *ast_func_def(const char *name, ASTNode *body) {
    ASTNode *n = ast_alloc(NODE_FUNC_DEF);
    n->data.func_def.name = sdup(name);
    n->data.func_def.body = body;
    return n;
}

static ASTNode *ast_func_call(const char *name) {
    ASTNode *n = ast_alloc(NODE_FUNC_CALL);
    n->data.func_call.name = sdup(name);
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
    if (lex_cur.type == TOK_BREAK) {
        lex_next();
        return ast_alloc(NODE_BREAK);
    }
    if (lex_cur.type == TOK_TRY) {
        lex_next();
        if (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type != TOK_LBRACE) fatal("line %d: expected '{' after try", lex_cur.line);
        ASTNode *body = parse_block();
        while (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type != TOK_CATCH) fatal("line %d: expected catch after try", lex_cur.line);
        lex_next();
        if (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type != TOK_LBRACE) fatal("line %d: expected '{' after catch", lex_cur.line);
        ASTNode *catch_body = parse_block();
        ASTNode *n = ast_alloc(NODE_TRY);
        n->data.try_stmt.body = body;
        n->data.try_stmt.catch_body = catch_body;
        return n;
    }
    if (lex_cur.type == TOK_INCLUDE) {
        lex_next();
        if (lex_cur.type != TOK_ID) fatal("line %d: include expects a library name", lex_cur.line);
        char *name = sdup(lex_cur.val.str);
        lex_next();
        return ast_include(name);
    }
    if (lex_cur.type == TOK_STRIFY) {
        lex_next();
        if (lex_cur.type != TOK_ID) fatal("line %d: strify expects a variable name", lex_cur.line);
        char *name = sdup(lex_cur.val.str);
        lex_next();
        ASTNode *n = ast_alloc(NODE_STRIFY);
        n->data.input.name = name;
        return n;
    }
    if (lex_cur.type == TOK_INTIFY) {
        lex_next();
        if (lex_cur.type != TOK_ID) fatal("line %d: intify expects a variable name", lex_cur.line);
        char *name = sdup(lex_cur.val.str);
        lex_next();
        char *fmt = NULL;
        if (lex_cur.type == TOK_ID && (!strcmp(lex_cur.val.str, "binary") || !strcmp(lex_cur.val.str, "hex") || !strcmp(lex_cur.val.str, "octal"))) {
            fmt = sdup(lex_cur.val.str);
            lex_next();
        }
        ASTNode *n = ast_alloc(NODE_INTIFY);
        n->data.input.name = name;
        n->data.input.prompt = fmt;
        return n;
    }

    if (lex_cur.type == TOK_MOD && lex_peek_next().type == TOK_ID) {
        lex_next();
        char *vname = sdup(lex_cur.val.str);
        lex_next();
        return ast_assign(vname, ast_str(""));
    }

    if (lex_cur.type == TOK_DOLLAR_ID) {
        char *fname = sdup(lex_cur.val.str);
        lex_next();
        if (lex_cur.type != TOK_LPAREN) fatal("line %d: expected '(' after function name", lex_cur.line);
        lex_next();
        if (lex_cur.type != TOK_RPAREN) fatal("line %d: expected ')'", lex_cur.line);
        lex_next();
        if (lex_cur.type == TOK_NEWLINE) lex_next();
        if (lex_cur.type != TOK_LBRACE) fatal("line %d: expected '{' after function definition", lex_cur.line);
        ASTNode *body = parse_block();
        return ast_func_def(fname, body);
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
                if (!prefixed) fatal("out of memory");
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
        if (lex_peek_next().type == TOK_LPAREN) {
            char *fname = sdup(lex_cur.val.str);
            lex_next();
            lex_next();
            if (lex_cur.type != TOK_RPAREN) fatal("line %d: expected ')' after function call", lex_cur.line);
            lex_next();
            return ast_func_call(fname);
        }
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
    if (lex_cur.type == TOK_AT) {
        lex_next();
        ASTNode *o = parse_unary();
        return ast_unary(TOK_AT, o);
    }
    if (lex_cur.type == TOK_CARET) {
        lex_next();
        ASTNode *o = parse_unary();
        return ast_unary(TOK_CARET, o);
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
            char *res = malloc(2048);
            size_t res_cap = 2048, ri = 0, pos = 0, len = strlen(raw);
            if (!res) fatal("out of memory");
            while (pos < len) {
                if (ri + 256 >= res_cap) {
                    res_cap *= 2; res = realloc(res, res_cap);
                    if (!res) fatal("out of memory");
                }
                if (raw[pos] == '{') {
                    pos++;
                    size_t start = pos;
                    while (pos < len && raw[pos] != '}') {
                        if (raw[pos] == '{') fatal("line %d: nested '{' in template", n->line);
                        pos++;
                    }
                    if (pos >= len) fatal("line %d: unclosed '{' in template", n->line);
                    size_t vlen = pos - start;
                    if (vlen == 0) fatal("line %d: empty '{}' in template", n->line);
                    char vname[256];
                    if (vlen >= 256) vlen = 255;
                    memcpy(vname, raw + start, vlen);
                    vname[vlen] = 0;
                    Value v = var_get(vname);
                    char *vs = val_tostr(v);
                    size_t sl = strlen(vs);
                    while (ri + sl + 1 >= res_cap) {
                        res_cap *= 2; res = realloc(res, res_cap);
                        if (!res) fatal("out of memory");
                    }
                    memcpy(res + ri, vs, sl); ri += sl;
                    free(vs);
                    pos++;
                } else {
                    res[ri++] = raw[pos++];
                }
            }
            res[ri] = 0;
            Value tv = make_str(res);
            free(res);
            return tv;
        }
        case NODE_FUNC_CALL: {
            for (int i = 0; i < func_count; i++) {
                if (!strcmp(funcs[i].name, n->data.func_call.name)) {
                    exec_stmt(funcs[i].body);
                    return make_num(0);
                }
            }
            fatal("line %d: function '%s' not defined", n->line, n->data.func_call.name);
            return make_num(0);
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
            if (n->data.unary.op == TOK_AT || n->data.unary.op == TOK_CARET)
                fatal("line %d: '@' and '^' are only available in compiled mode", n->line);
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
                    Value concatv = make_str(res);
                    free(res);
                    return concatv;
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
                { static int rng_seeded; if (!rng_seeded) { srand(time(NULL)); rng_seeded = 1; } }
                if (!strcmp(fn, "random")) {
                    return make_num((double)rand() / RAND_MAX);
                }
                if (!strcmp(fn, "randint")) {
                    if (n->data.funcall.argc < 3) fatal("line %d: randint expects min and max", n->line);
                    char *r1 = resolve_arg(n->data.funcall.args[1]);
                    char *r2 = resolve_arg(n->data.funcall.args[2]);
                    int lo = (int)strtod(r1, NULL); free(r1);
                    int hi = (int)strtod(r2, NULL); free(r2);
                    if (lo > hi) { int t = lo; lo = hi; hi = t; }
                    return make_num(lo + rand() % (hi - lo + 1));
                }
                if (!strcmp(fn, "choice")) {
                    if (n->data.funcall.argc < 2) fatal("line %d: choice expects at least one option", n->line);
                    int idx = rand() % (n->data.funcall.argc - 1) + 1;
                    return make_str(n->data.funcall.args[idx]);
                }
                if (!strcmp(fn, "sleep")) {
                    if (n->data.funcall.argc < 2) fatal("line %d: sleep expects seconds", n->line);
                    char *rs = resolve_arg(n->data.funcall.args[1]);
                    double secs = strtod(rs, NULL); free(rs);
                    if (secs > 0) {
                        struct timespec ts;
                        ts.tv_sec = (time_t)secs;
                        ts.tv_nsec = (long)((secs - (time_t)secs) * 1e9);
                        while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {}
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
                    if (cnt > 92) cnt = 92;
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
                    for (long i = 2; i <= num / i; i++) {
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
                    char *rs = resolve_arg(n->data.funcall.args[2]);
                    int start = (int)strtod(rs, NULL); free(rs);
                    rs = resolve_arg(n->data.funcall.args[3]);
                    int end = (int)strtod(rs, NULL); free(rs);
                    size_t len = strlen(arg);
                    if (start < 0) start = 0;
                    if (end > (int)len) end = (int)len;
                    if (start >= end) { free(arg); return make_str(""); }
                    char *ress = sdupn(arg + start, end - start);
                    free(arg);
                    Value sv = make_str(ress);
                    free(ress);
                    return sv;
                }
                if (!strcmp(fn, "replace")) {
                    if (n->data.funcall.argc < 4) fatal("line %d: string replace expects string, old, new", n->line);
                    char *arg = resolve_arg(n->data.funcall.args[1]);
                    char *old = resolve_arg(n->data.funcall.args[2]);
                    char *new = resolve_arg(n->data.funcall.args[3]);
                    size_t olen = strlen(old), nlen = strlen(new);
                    if (olen == 0) { Value rv = make_str(arg); free(arg); free(old); free(new); return rv; }
                    size_t blen = strlen(arg) + 1;
                    if (nlen > olen) blen += (strlen(arg) / olen) * (nlen - olen) + 1;
                    char *buf = malloc(blen > 4096 ? blen : 4096);
                    if (!buf) fatal("out of memory");
                    size_t ri = 0, pos = 0, len = strlen(arg);
                    while (pos < len) {
                        if (ri + nlen + 1 >= blen) {
                            blen *= 2; buf = realloc(buf, blen);
                            if (!buf) fatal("out of memory");
                        }
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
                    Value repv = make_str(buf);
                    free(buf);
                    return repv;
                }
                if (!strcmp(fn, "repeat")) {
                    if (n->data.funcall.argc < 3) fatal("line %d: string repeat expects string and count", n->line);
                    char *arg = resolve_arg(n->data.funcall.args[1]);
                    char *rc = resolve_arg(n->data.funcall.args[2]);
                    int count = (int)strtod(rc, NULL); free(rc);
                    if (count < 0) count = 0;
                    if (count > 1000) count = 1000;
                    size_t slen = strlen(arg);
                    if (slen > 0 && (size_t)count > 1000000 / slen) count = (int)(1000000 / slen);
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
            while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r' || buf[len-1] == ' ' || buf[len-1] == '\t')) buf[--len] = 0;
            char *end;
            double d = strtod(buf, &end);
            while (*end == ' ' || *end == '\t') end++;
            if (*end == 0) {
                var_set(n->data.input.name, make_num(d));
            } else {
                var_set(n->data.input.name, make_str(buf));
            }
            return 0;
        }
        case NODE_STRIFY: {
            int idx = var_find(n->data.input.name);
            if (idx < 0) { var_set(n->data.input.name, make_str("")); idx = var_count - 1; }
            char *s = val_tostr(vars[idx].val);
            var_set(n->data.input.name, make_str(s));
            free(s);
            return 0;
        }
        case NODE_INTIFY: {
            if (n->data.input.prompt) {
                int idx = var_find(n->data.input.name);
                if (idx < 0) { var_set(n->data.input.name, make_str("")); idx = var_count - 1; }
                char *s = val_tostr(vars[idx].val);
                size_t slen = strlen(s);
                char *out = malloc(slen * 16 + 1);
                if (!out) fatal("out of memory");
                out[0] = 0;
                for (size_t i = 0; i < slen; i++) {
                    if (i > 0) strcat(out, " ");
                    if (!strcmp(n->data.input.prompt, "binary")) {
                        for (int b = 7; b >= 0; b--) strcat(out, ((unsigned char)s[i] >> b) & 1 ? "1" : "0");
                    } else if (!strcmp(n->data.input.prompt, "hex")) {
                        char buf[16]; snprintf(buf, sizeof(buf), "%02x", (unsigned char)s[i]);
                        strcat(out, buf);
                    } else {
                        char buf[16]; snprintf(buf, sizeof(buf), "%03o", (unsigned char)s[i]);
                        strcat(out, buf);
                    }
                }
                free(s);
                var_set(n->data.input.name, make_str(out));
                free(out);
            } else {
                Value v = var_get(n->data.input.name);
                double d = val_tonum(v);
                var_set(n->data.input.name, make_num(d));
            }
            return 0;
        }
        case NODE_TRY: {
            jmp_buf old;
            memcpy(old, error_jmp, sizeof(jmp_buf));
            if (setjmp(error_jmp) == 0) {
                exec_stmt(n->data.try_stmt.body);
                memcpy(error_jmp, old, sizeof(jmp_buf));
            } else {
                memcpy(error_jmp, old, sizeof(jmp_buf));
                exec_stmt(n->data.try_stmt.catch_body);
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
        case NODE_BREAK:
            return 2;
        case NODE_INCLUDE:
            return 0;
        case NODE_FUNC_DEF: {
            for (int i = 0; i < func_count; i++) {
                if (!strcmp(funcs[i].name, n->data.func_def.name)) {
                    fatal("line %d: function '%s' already defined", n->line, n->data.func_def.name);
                }
            }
            if (func_count >= MAX_FUNCS) fatal("line %d: too many function definitions", n->line);
            funcs[func_count].name = sdup(n->data.func_def.name);
            funcs[func_count].body = n->data.func_def.body;
            func_count++;
            return 0;
        }
        case NODE_EXIT:
            return 1;
        default:
            eval_expr(n);
            return 0;
    }
}

enum {
    OP_NOP, OP_CONST, OP_VAR_GET, OP_VAR_SET,
    OP_VAR_GET_IDX, OP_VAR_SET_IDX, OP_INC_IDX, OP_DEC_IDX,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_NEG,
    OP_EQ, OP_NE, OP_LT, OP_GT, OP_LE, OP_GE,
    OP_AND, OP_OR, OP_NOT,
    OP_PRINT, OP_JMP, OP_JZ, OP_HALT, OP_EXIT, OP_FALLBACK, OP_POP
};

#define CODE_INIT 4096
#define CONST_INIT 256
#define STR_INIT 256
#define VM_STACK 4096
#define FIXUP_INIT 1024

typedef struct { uint16_t op; int arg; } Instr;

static Instr *code;
static int code_len, code_cap;
static Value *consts;
static int const_len, const_cap;
static char **strtab;
static ASTNode **fallbacks;
static int fb_len, fb_cap;
static int cur_loop;
static int *fixups;
static int *fixup_lv;
static int fixup_cap, fixup_len;
static Value vm_stack[VM_STACK];
static int sp;
static int vm_exit;
static int for_counter;

static int add_const(Value v) {
    if (const_len >= const_cap) {
        const_cap = const_cap ? const_cap * 2 : CONST_INIT;
        consts = realloc(consts, const_cap * sizeof(Value));
    }
    consts[const_len] = v;
    return const_len++;
}

static int add_fallback(ASTNode *n) {
    if (fb_len >= fb_cap) {
        fb_cap = fb_cap ? fb_cap * 2 : 128;
        fallbacks = realloc(fallbacks, fb_cap * sizeof(ASTNode*));
    }
    fallbacks[fb_len] = n;
    return fb_len++;
}

static void emit(uint16_t op, int arg) {
    if (code_len >= code_cap) {
        code_cap = code_cap ? code_cap * 2 : CODE_INIT;
        code = realloc(code, code_cap * sizeof(Instr));
    }
    code[code_len].op = op;
    code[code_len].arg = arg;
    code_len++;
}

static void patch(int idx, int target) {
    code[idx].arg = target;
}

static void add_fixup(int addr) {
    if (fixup_len >= fixup_cap) {
        fixup_cap = fixup_cap ? fixup_cap * 2 : FIXUP_INIT;
        fixups = realloc(fixups, fixup_cap * sizeof(int));
        fixup_lv = realloc(fixup_lv, fixup_cap * sizeof(int));
    }
    fixups[fixup_len] = addr;
    fixup_lv[fixup_len] = cur_loop;
    fixup_len++;
}

static void patch_fixups(int target) {
    for (int i = 0; i < fixup_len; i++) {
        if (fixup_lv[i] == cur_loop)
            patch(fixups[i], target);
    }
    int w = 0;
    for (int i = 0; i < fixup_len; i++) {
        if (fixup_lv[i] != cur_loop) {
            fixups[w] = fixups[i];
            fixup_lv[w] = fixup_lv[i];
            w++;
        }
    }
    fixup_len = w;
}

static int ce_expr(ASTNode *n) {
    if (!n) { emit(OP_CONST, add_const(make_num(0))); return 1; }
    switch (n->type) {
        case NODE_NUM: emit(OP_CONST, add_const(make_num(n->data.num))); return 1;
        case NODE_STR: emit(OP_CONST, add_const(make_str(n->data.str))); return 1;
        case NODE_ID: emit(OP_VAR_GET_IDX, var_ensure(n->data.id)); return 1;
        case NODE_BINOP:
            if (!ce_expr(n->data.binop.left)) return 0;
            if (!ce_expr(n->data.binop.right)) return 0;
            switch (n->data.binop.op) {
                case TOK_PLUS: emit(OP_ADD, 0); break;
                case TOK_MINUS: emit(OP_SUB, 0); break;
                case TOK_STAR: emit(OP_MUL, 0); break;
                case TOK_SLASH: emit(OP_DIV, 0); break;
                case TOK_MOD: emit(OP_MOD, 0); break;
                case TOK_EQ: emit(OP_EQ, 0); break;
                case TOK_NE: emit(OP_NE, 0); break;
                case TOK_LT: emit(OP_LT, 0); break;
                case TOK_GT: emit(OP_GT, 0); break;
                case TOK_LE: emit(OP_LE, 0); break;
                case TOK_GE: emit(OP_GE, 0); break;
                case TOK_AND: emit(OP_AND, 0); break;
                case TOK_OR: emit(OP_OR, 0); break;
            }
            return 1;
        case NODE_UNARY:
            if (!ce_expr(n->data.unary.operand)) return 0;
            if (n->data.unary.op == TOK_MINUS) emit(OP_NEG, 0);
            else if (n->data.unary.op == TOK_NOT) emit(OP_NOT, 0);
            else if (n->data.unary.op == TOK_AT || n->data.unary.op == TOK_CARET)
                fatal("line %d: '@' and '^' are only available in compiled mode", n->line);
            return 1;
        default: return 0;
    }
}

static int is_cstmt(ASTNode *n) {
    if (!n) return 1;
    switch (n->type) {
        case NODE_EMPTY: case NODE_EXIT: case NODE_STOP: case NODE_BREAK: return 1;
        case NODE_PRINT:
            for (int i = 0; i < n->data.print.count; i++)
                if (n->data.print.exprs[i]->type == NODE_FUNC_CALL
                    || n->data.print.exprs[i]->type == NODE_TEMPLATE) return 0;
            return 1;
        case NODE_ASSIGN:
            return n->data.assign.value->type != NODE_FUNC_CALL
                && n->data.assign.value->type != NODE_TEMPLATE;
        case NODE_IF:
            if (n->data.if_stmt.has_mode || n->data.if_stmt.flags) return 0;
            return is_cstmt(n->data.if_stmt.then)
                && (!n->data.if_stmt.els || is_cstmt(n->data.if_stmt.els));
        case NODE_WHILE: return is_cstmt(n->data.while_stmt.body);
        case NODE_FORTO: return is_cstmt(n->data.forto.body);
        case NODE_LOOP: return is_cstmt(n->data.loop.body);
        case NODE_TRY: return is_cstmt(n->data.try_stmt.body) && is_cstmt(n->data.try_stmt.catch_body);
        case NODE_BLOCK: {
            for (int i = 0; i < n->data.block.count; i++)
                if (!is_cstmt(n->data.block.stmts[i])) return 0;
            return 1;
        }
        default: return 0;
    }
}

static void ce_stmt(ASTNode *n) {
    if (!n) return;
    switch (n->type) {
        case NODE_EMPTY: break;
        case NODE_ASSIGN: {
            int vidx = var_ensure(n->data.assign.name);
            int is_inc = 0, is_dec = 0;
            if (n->data.assign.value->type == NODE_BINOP) {
                ASTNode *l = n->data.assign.value->data.binop.left;
                ASTNode *r = n->data.assign.value->data.binop.right;
                int opc = n->data.assign.value->data.binop.op;
                if (l->type == NODE_ID && r->type == NODE_NUM && r->data.num == 1
                    && strcmp(l->data.id, n->data.assign.name) == 0) {
                    if (opc == TOK_PLUS) is_inc = 1;
                    else if (opc == TOK_MINUS) is_dec = 1;
                }
                if (r->type == NODE_ID && l->type == NODE_NUM && l->data.num == 1
                    && strcmp(r->data.id, n->data.assign.name) == 0 && opc == TOK_PLUS) is_inc = 1;
            }
            if (is_inc) { emit(OP_INC_IDX, vidx); break; }
            if (is_dec) { emit(OP_DEC_IDX, vidx); break; }
            ce_expr(n->data.assign.value);
            emit(OP_VAR_SET_IDX, vidx);
            break;
        }
        case NODE_PRINT: {
            for (int i = 0; i < n->data.print.count; i++)
                ce_expr(n->data.print.exprs[i]);
            emit(OP_PRINT, n->data.print.count);
            break;
        }
        case NODE_IF: {
            if (n->data.if_stmt.has_mode || n->data.if_stmt.flags) {
                emit(OP_FALLBACK, add_fallback(n)); break;
            }
            ce_expr(n->data.if_stmt.cond);
            emit(OP_JZ, 0);
            int pf = code_len - 1;
            ce_stmt(n->data.if_stmt.then);
            if (n->data.if_stmt.els) {
                emit(OP_JMP, 0);
                int pe = code_len - 1;
                patch(pf, code_len);
                ce_stmt(n->data.if_stmt.els);
                patch(pe, code_len);
            } else {
                patch(pf, code_len);
            }
            break;
        }
        case NODE_WHILE: {
            if (!is_cstmt(n)) { emit(OP_FALLBACK, add_fallback(n)); break; }
            int ls = code_len;
            ce_expr(n->data.while_stmt.cond);
            emit(OP_JZ, 0);
            int pe = code_len - 1;
            cur_loop++; ce_stmt(n->data.while_stmt.body);
            emit(OP_JMP, ls);
            patch(pe, code_len);
            patch_fixups(code_len);
            cur_loop--;
            break;
        }
        case NODE_FORTO: {
            if (!is_cstmt(n)) { emit(OP_FALLBACK, add_fallback(n)); break; }
            int vidx = var_ensure(n->data.forto.var);
            ce_expr(n->data.forto.start);
            emit(OP_VAR_SET_IDX, vidx);
            ce_expr(n->data.forto.end);
            int fid = for_counter++;
            char h[64];
            snprintf(h, sizeof(h), "__for_%d", fid);
            int hidx = var_ensure(h);
            emit(OP_VAR_SET_IDX, hidx);
            int ls = code_len;
            emit(OP_VAR_GET_IDX, vidx);
            emit(OP_VAR_GET_IDX, hidx);
            emit(OP_LE, 0);
            emit(OP_JZ, 0);
            int pe = code_len - 1;
            cur_loop++; ce_stmt(n->data.forto.body);
            emit(OP_VAR_GET_IDX, vidx);
            emit(OP_CONST, add_const(make_num(1)));
            emit(OP_ADD, 0);
            emit(OP_VAR_SET_IDX, vidx);
            emit(OP_JMP, ls);
            patch(pe, code_len);
            patch_fixups(code_len);
            cur_loop--;
            break;
        }
        case NODE_LOOP: {
            if (!is_cstmt(n)) { emit(OP_FALLBACK, add_fallback(n)); break; }
            int ls = code_len;
            cur_loop++; ce_stmt(n->data.loop.body); emit(OP_JMP, ls); patch_fixups(code_len); cur_loop--;
            break;
        }
        case NODE_STOP:
        case NODE_BREAK:
            if (cur_loop == 0) fatal("line %d: stop/break outside a loop", n->line);
            add_fixup(code_len);
            emit(OP_JMP, 0);
            break;
        case NODE_EXIT:
            emit(OP_EXIT, 0);
            break;
        case NODE_BLOCK:
            for (int i = 0; i < n->data.block.count; i++)
                ce_stmt(n->data.block.stmts[i]);
            break;
        default:
            emit(OP_FALLBACK, add_fallback(n));
            break;
    }
}

static void compile_prog(ASTNode **stmts, int nstmts) {
    for (int i = 0; i < nstmts; i++)
        ce_stmt(stmts[i]);
    emit(OP_HALT, 0);
}

static int truthy(Value v) {
    if (v.type == VAL_STR) return strlen(v.data.str) != 0;
    return v.data.num != 0;
}

static void vm_run(void) {
    int ip = 0;
    sp = 0;
    vm_exit = 0;
    void *dtab[] = { &&OP_NOP, &&OP_CONST, &&OP_VAR_GET, &&OP_VAR_SET,
        &&OP_VAR_GET_IDX, &&OP_VAR_SET_IDX, &&OP_INC_IDX, &&OP_DEC_IDX,
        &&OP_ADD, &&OP_SUB, &&OP_MUL, &&OP_DIV, &&OP_MOD, &&OP_NEG,
        &&OP_CMP, &&OP_CMP, &&OP_CMP, &&OP_CMP, &&OP_CMP, &&OP_CMP,
        &&OP_AND, &&OP_OR, &&OP_NOT,
        &&OP_PRINT, &&OP_JMP, &&OP_JZ, &&OP_HALT, &&OP_EXIT, &&OP_FALLBACK, &&OP_POP };
    goto *dtab[code[ip].op];

OP_NOP: ip++; goto *dtab[code[ip].op];
OP_CONST: {
    Value v = consts[code[ip].arg];
    if (v.type == VAL_STR) v.data.str = sdup(v.data.str);
    vm_stack[sp++] = v; ip++; goto *dtab[code[ip].op];
}
OP_VAR_GET: {
    Value v = var_get(strtab[code[ip].arg]);
    if (v.type == VAL_STR) v.data.str = sdup(v.data.str);
    vm_stack[sp++] = v; ip++; goto *dtab[code[ip].op];
}
OP_VAR_SET: {
    var_set(strtab[code[ip].arg], vm_stack[--sp]); ip++; goto *dtab[code[ip].op];
}
OP_VAR_GET_IDX: {
    Value v = vars[code[ip].arg].val;
    if (v.type == VAL_STR) v.data.str = sdup(v.data.str);
    vm_stack[sp++] = v; ip++; goto *dtab[code[ip].op];
}
OP_VAR_SET_IDX: {
    int idx = code[ip].arg;
    Value v = vm_stack[--sp];
    if (vars[idx].val.type == VAL_STR) free(vars[idx].val.data.str);
    vars[idx].val = v;
    ip++; goto *dtab[code[ip].op];
}
OP_INC_IDX: {
    int idx = code[ip].arg;
    if (vars[idx].val.type == VAL_STR) free(vars[idx].val.data.str);
    vars[idx].val.data.num += 1;
    vars[idx].val.type = VAL_NUM;
    ip++; goto *dtab[code[ip].op];
}
OP_DEC_IDX: {
    int idx = code[ip].arg;
    if (vars[idx].val.type == VAL_STR) free(vars[idx].val.data.str);
    vars[idx].val.data.num -= 1;
    vars[idx].val.type = VAL_NUM;
    ip++; goto *dtab[code[ip].op];
}
OP_ADD: {
    Value b = vm_stack[--sp], a = vm_stack[--sp];
    if (a.type == VAL_STR || b.type == VAL_STR) {
        char *as = val_tostr(a), *bs = val_tostr(b);
        char *r = malloc(strlen(as) + strlen(bs) + 1);
        strcpy(r, as); strcat(r, bs);
        val_free(a); val_free(b); free(as); free(bs);
        vm_stack[sp++] = make_str(r); free(r);
    } else {
        val_free(a); val_free(b); vm_stack[sp++] = make_num(a.data.num + b.data.num);
    }
    ip++; goto *dtab[code[ip].op];
}
OP_SUB: {
    Value b = vm_stack[--sp], a = vm_stack[--sp];
    if (a.type == VAL_STR || b.type == VAL_STR) { val_free(a); val_free(b); vm_stack[sp++] = make_num(0); }
    else { val_free(a); val_free(b); vm_stack[sp++] = make_num(a.data.num - b.data.num); }
    ip++; goto *dtab[code[ip].op];
}
OP_MUL: {
    Value b = vm_stack[--sp], a = vm_stack[--sp];
    if (a.type == VAL_STR || b.type == VAL_STR) { val_free(a); val_free(b); vm_stack[sp++] = make_num(0); }
    else { val_free(a); val_free(b); vm_stack[sp++] = make_num(a.data.num * b.data.num); }
    ip++; goto *dtab[code[ip].op];
}
OP_DIV: {
    Value b = vm_stack[--sp], a = vm_stack[--sp];
    if (a.type == VAL_STR || b.type == VAL_STR) { val_free(a); val_free(b); vm_stack[sp++] = make_num(0); }
    else {
        double rd = b.data.num;
        if (rd == 0) { val_free(a); val_free(b); vm_stack[sp++] = make_num(0); }
        else { val_free(a); val_free(b); vm_stack[sp++] = make_num(a.data.num / rd); }
    }
    ip++; goto *dtab[code[ip].op];
}
OP_MOD: {
    Value b = vm_stack[--sp], a = vm_stack[--sp];
    if (a.type == VAL_STR || b.type == VAL_STR) { val_free(a); val_free(b); vm_stack[sp++] = make_num(0); }
    else {
        int rd = (int)b.data.num;
        if (rd == 0) { val_free(a); val_free(b); vm_stack[sp++] = make_num(0); }
        else { val_free(a); val_free(b); vm_stack[sp++] = make_num((double)((int)a.data.num % rd)); }
    }
    ip++; goto *dtab[code[ip].op];
}
OP_NEG: {
    Value a = vm_stack[--sp];
    if (a.type == VAL_STR) { val_free(a); vm_stack[sp++] = make_num(0); }
    else { double r = -a.data.num; val_free(a); vm_stack[sp++] = make_num(r); }
    ip++; goto *dtab[code[ip].op];
}
OP_CMP: {
    Value b = vm_stack[--sp], a = vm_stack[--sp];
    int r;
    uint16_t oc = code[ip].op;
    if (a.type == VAL_STR || b.type == VAL_STR) {
        char *as = val_tostr(a), *bs = val_tostr(b);
        int c = strcmp(as, bs);
        free(as); free(bs);
        if (oc == OP_EQ) r = c == 0; else if (oc == OP_NE) r = c != 0;
        else if (oc == OP_LT) r = c < 0; else if (oc == OP_GT) r = c > 0;
        else if (oc == OP_LE) r = c <= 0; else r = c >= 0;
    } else {
        double av = a.data.num, bv = b.data.num;
        if (oc == OP_EQ) r = av == bv; else if (oc == OP_NE) r = av != bv;
        else if (oc == OP_LT) r = av < bv; else if (oc == OP_GT) r = av > bv;
        else if (oc == OP_LE) r = av <= bv; else r = av >= bv;
    }
    val_free(a); val_free(b); vm_stack[sp++] = make_num(r);
    ip++; goto *dtab[code[ip].op];
}
OP_AND: {
    Value b = vm_stack[--sp], a = vm_stack[--sp];
    int r = truthy(a) && truthy(b);
    val_free(a); val_free(b); vm_stack[sp++] = make_num(r);
    ip++; goto *dtab[code[ip].op];
}
OP_OR: {
    Value b = vm_stack[--sp], a = vm_stack[--sp];
    int r = truthy(a) || truthy(b);
    val_free(a); val_free(b); vm_stack[sp++] = make_num(r);
    ip++; goto *dtab[code[ip].op];
}
OP_NOT: {
    Value a = vm_stack[--sp];
    int r = !truthy(a);
    val_free(a); vm_stack[sp++] = make_num(r);
    ip++; goto *dtab[code[ip].op];
}
OP_PRINT: {
    int n = code[ip].arg;
    Value *vals = malloc(n * sizeof(Value));
    for (int i = n - 1; i >= 0; i--)
        vals[i] = vm_stack[--sp];
    for (int i = 0; i < n; i++) {
        if (vals[i].type == VAL_STR) { printf("%s", vals[i].data.str); free(vals[i].data.str); }
        else printf("%g", vals[i].data.num);
    }
    free(vals); printf("\n");
    ip++; goto *dtab[code[ip].op];
}
OP_JMP: {
    ip = code[ip].arg; goto *dtab[code[ip].op];
}
OP_JZ: {
    Value a = vm_stack[--sp];
    int t = truthy(a); val_free(a);
    ip = t ? ip + 1 : code[ip].arg;
    goto *dtab[code[ip].op];
}
OP_HALT: return;
OP_EXIT: return;
OP_POP: {
    val_free(vm_stack[--sp]); ip++; goto *dtab[code[ip].op];
}
OP_FALLBACK: {
    int r = exec_stmt(fallbacks[code[ip].arg]);
    if (r == 1) return;
    ip++; goto *dtab[code[ip].op];
}
}

static char *cescape(const char *s) {
    size_t n = strlen(s);
    char *r = malloc(n * 4 + 1);
    if (!r) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '\\') { r[j++] = '\\'; r[j++] = '\\'; }
        else if (s[i] == '"') { r[j++] = '\\'; r[j++] = '"'; }
        else if (s[i] == '\n') { r[j++] = '\\'; r[j++] = 'n'; }
        else if (s[i] == '\r') { r[j++] = '\\'; r[j++] = 'r'; }
        else if (s[i] == '\t') { r[j++] = '\\'; r[j++] = 't'; }
        else r[j++] = s[i];
    }
    r[j] = 0;
    return r;
}

static int cg_loop_id;

typedef enum { TY_NUM, TY_STR, TY_DYN } VarType;
static VarType var_types[MAX_VARS];

static VarType infer_expr_type(ASTNode *n) {
    if (!n) return TY_NUM;
    switch (n->type) {
        case NODE_NUM: return TY_NUM;
        case NODE_STR: return TY_STR;
        case NODE_ID: {
            int i = var_find(n->data.id);
            if (i < 0) return TY_NUM;
            return var_types[i];
        }
        case NODE_BINOP: {
            int op = n->data.binop.op;
            VarType lt = infer_expr_type(n->data.binop.left);
            VarType rt = infer_expr_type(n->data.binop.right);
            if (op == TOK_AND || op == TOK_OR || op == TOK_EQ || op == TOK_NE || op == TOK_LT || op == TOK_GT || op == TOK_LE || op == TOK_GE)
                return TY_NUM;
            if (op == TOK_PLUS && (lt == TY_STR || rt == TY_STR)) return TY_STR;
            if (lt == TY_NUM && rt == TY_NUM) return TY_NUM;
            return TY_DYN;
        }
        case NODE_UNARY: {
            VarType ot = infer_expr_type(n->data.unary.operand);
            if (n->data.unary.op == TOK_NOT) return TY_NUM;
            if (n->data.unary.op == TOK_MINUS && ot == TY_NUM) return TY_NUM;
            if (n->data.unary.op == TOK_MINUS) return TY_DYN;
            if (n->data.unary.op == TOK_AT || n->data.unary.op == TOK_CARET) return TY_NUM;
            return ot;
        }
        case NODE_TEMPLATE: return TY_STR;
        case NODE_FUNC_CALL: return TY_DYN;
        default: return TY_DYN;
    }
}

static void infer_type_stmt(ASTNode *n) {
    if (!n) return;
    switch (n->type) {
        case NODE_ASSIGN: {
            int i = var_find(n->data.assign.name);
            if (i < 0) { var_ensure(n->data.assign.name); i = var_count - 1; }
            var_types[i] = infer_expr_type(n->data.assign.value);
            break;
        }
        case NODE_WHILE: infer_type_stmt(n->data.while_stmt.cond); infer_type_stmt(n->data.while_stmt.body); break;
        case NODE_IF: infer_type_stmt(n->data.if_stmt.cond); infer_type_stmt(n->data.if_stmt.then); infer_type_stmt((ASTNode*)n->data.if_stmt.els); break;
        case NODE_LOOP: infer_type_stmt(n->data.loop.body); break;
        case NODE_FORTO: {
            int i = var_ensure(n->data.forto.var);
            var_types[i] = TY_NUM;
            break;
        }
        case NODE_BLOCK: for (int i = 0; i < n->data.block.count; i++) infer_type_stmt(n->data.block.stmts[i]); break;
        case NODE_INPUT: {
            int i = var_ensure(n->data.input.name);
            var_types[i] = TY_STR;
            break;
        }
        case NODE_STRIFY:
        case NODE_INTIFY: {
            int i = var_ensure(n->data.input.name);
            var_types[i] = TY_DYN;
            break;
        }
        case NODE_TRY:
            infer_type_stmt(n->data.try_stmt.body);
            infer_type_stmt(n->data.try_stmt.catch_body);
            break;
        default: break;
    }
}

static void cg_collect_vars(ASTNode *n);
static void cg_varinit(FILE *f, ASTNode *prog) {
    var_count = 0;
    for (int i = 0; i < prog->data.block.count; i++)
        cg_collect_vars(prog->data.block.stmts[i]);
    for (int i = 0; i < var_count; i++) {
        VarType t = var_types[i];
        if (t == TY_NUM)
            fprintf(f, "  double %s = 0;\n", vars[i].name);
        else if (t == TY_STR) {
            fprintf(f, "  Value %s = {VAL_STR,{.str=NULL}};\n", vars[i].name);
            fprintf(f, "  var_ensure(\"%s\");\n", vars[i].name);
        } else {
            fprintf(f, "  Value %s = {VAL_NUM,{.num=0}};\n", vars[i].name);
            fprintf(f, "  var_ensure(\"%s\");\n", vars[i].name);
        }
    }
}

static void cg_collect_vars(ASTNode *n) {
    if (!n) return;
    switch (n->type) {
        case NODE_ASSIGN: var_ensure(n->data.assign.name); cg_collect_vars(n->data.assign.value); break;
        case NODE_WHILE: cg_collect_vars(n->data.while_stmt.cond); cg_collect_vars(n->data.while_stmt.body); break;
        case NODE_IF: cg_collect_vars(n->data.if_stmt.cond); cg_collect_vars(n->data.if_stmt.then); cg_collect_vars((ASTNode*)n->data.if_stmt.els); break;
        case NODE_LOOP: cg_collect_vars(n->data.loop.body); break;
        case NODE_FORTO: var_ensure(n->data.forto.var); cg_collect_vars(n->data.forto.start); cg_collect_vars(n->data.forto.end); cg_collect_vars(n->data.forto.body); break;
        case NODE_BLOCK: for (int i = 0; i < n->data.block.count; i++) cg_collect_vars(n->data.block.stmts[i]); break;
        case NODE_PRINT: for (int i = 0; i < n->data.print.count; i++) cg_collect_vars(n->data.print.exprs[i]); break;
        case NODE_INPUT:
        case NODE_STRIFY:
        case NODE_INTIFY: var_ensure(n->data.input.name); break;
        case NODE_TRY: cg_collect_vars(n->data.try_stmt.body); cg_collect_vars(n->data.try_stmt.catch_body); break;
        default: break;
    }
}

static void cg_expr(FILE *f, ASTNode *n, VarType want) {
    if (!n) { fprintf(f, want == TY_NUM ? "0" : "make_num(0)"); return; }
    switch (n->type) {
        case NODE_NUM: {
            if (want == TY_NUM) fprintf(f, "%.17g", n->data.num);
            else fprintf(f, "make_num(%.17g)", n->data.num);
            break;
        }
        case NODE_STR: {
            char *esc = cescape(n->data.str);
            fprintf(f, "make_str(\"%s\")", esc);
            free(esc);
            break;
        }
        case NODE_ID: {
            int idx = var_find(n->data.id);
            VarType vt = idx >= 0 ? var_types[idx] : TY_NUM;
            if (vt == TY_NUM) {
                if (want == TY_NUM) fprintf(f, "%s", n->data.id);
                else fprintf(f, "make_num(%s)", n->data.id);
            } else {
                if (want == TY_NUM) { fprintf(f, "val_tonum(%s)", n->data.id); }
                else fprintf(f, "copy_val(%s)", n->data.id);
            }
            break;
        }
        case NODE_BINOP: {
            int op = n->data.binop.op;
            VarType lt = infer_expr_type(n->data.binop.left);
            VarType rt = infer_expr_type(n->data.binop.right);
            if (op == TOK_AND || op == TOK_OR) {
                if (want == TY_NUM) {
                    fprintf(f, "(truthy(");
                    cg_expr(f, n->data.binop.left, TY_DYN);
                    fprintf(f, ") %s truthy(", op == TOK_AND ? "&&" : "||");
                    cg_expr(f, n->data.binop.right, TY_DYN);
                    fprintf(f, "))");
                } else {
                    fprintf(f, "make_num(truthy(");
                    cg_expr(f, n->data.binop.left, TY_DYN);
                    fprintf(f, ") %s truthy(", op == TOK_AND ? "&&" : "||");
                    cg_expr(f, n->data.binop.right, TY_DYN);
                    fprintf(f, "))");
                }
                break;
            }
            if (op == TOK_EQ || op == TOK_NE || op == TOK_LT || op == TOK_GT || op == TOK_LE || op == TOK_GE) {
                if (lt == TY_NUM && rt == TY_NUM) {
                    char *opstr = "=="; int neg = 0;
                    if (op == TOK_EQ) opstr = "==";
                    else if (op == TOK_NE) { opstr = "=="; neg = 1; }
                    else if (op == TOK_LT) opstr = "<";
                    else if (op == TOK_GT) opstr = ">";
                    else if (op == TOK_LE) opstr = "<=";
                    else if (op == TOK_GE) opstr = ">=";
                    if (neg) {
                        fprintf(f, "(!(");
                        cg_expr(f, n->data.binop.left, TY_NUM);
                        fprintf(f, " %s ", opstr);
                        cg_expr(f, n->data.binop.right, TY_NUM);
                        fprintf(f, "))");
                    } else {
                        fprintf(f, "(");
                        cg_expr(f, n->data.binop.left, TY_NUM);
                        fprintf(f, " %s ", opstr);
                        cg_expr(f, n->data.binop.right, TY_NUM);
                        fprintf(f, ")");
                    }
                    if (want != TY_NUM) fprintf(f, "?1:0");
                } else {
                    fprintf(f, "make_num(val_cmp(");
                    cg_expr(f, n->data.binop.left, TY_DYN);
                    fprintf(f, ",");
                    cg_expr(f, n->data.binop.right, TY_DYN);
                    if (op == TOK_EQ) fprintf(f, ",0))");
                    else if (op == TOK_NE) fprintf(f, ",1))");
                    else if (op == TOK_LT) fprintf(f, ",2))");
                    else if (op == TOK_GT) fprintf(f, ",3))");
                    else if (op == TOK_LE) fprintf(f, ",4))");
                    else fprintf(f, ",5))");
                }
                break;
            }
            if (op == TOK_PLUS && (lt == TY_STR || rt == TY_STR)) {
                fprintf(f, "val_add(");
                cg_expr(f, n->data.binop.left, TY_DYN);
                fprintf(f, ",");
                cg_expr(f, n->data.binop.right, TY_DYN);
                fprintf(f, ")");
                break;
            }
            if (lt == TY_NUM && rt == TY_NUM) {
                if (op == TOK_MOD) {
                    if (want == TY_NUM) fprintf(f, "(");
                    else fprintf(f, "make_num(");
                    fprintf(f, "(double)((int)");
                    cg_expr(f, n->data.binop.left, TY_NUM);
                    fprintf(f, "%%(int)");
                    cg_expr(f, n->data.binop.right, TY_NUM);
                    fprintf(f, ")");
                    fprintf(f, ")");
                } else {
                    if (want == TY_NUM) fprintf(f, "(");
                    else fprintf(f, "make_num(");
                    cg_expr(f, n->data.binop.left, TY_NUM);
                    if (op == TOK_PLUS) fprintf(f, "+");
                    else if (op == TOK_MINUS) fprintf(f, "-");
                    else if (op == TOK_STAR) fprintf(f, "*");
                    else if (op == TOK_SLASH) fprintf(f, "/");
                    cg_expr(f, n->data.binop.right, TY_NUM);
                    fprintf(f, ")");
                }
            } else {
                fprintf(f, "val_arith(");
                cg_expr(f, n->data.binop.left, TY_DYN);
                fprintf(f, ",");
                cg_expr(f, n->data.binop.right, TY_DYN);
                if (op == TOK_PLUS) fprintf(f, ",1)");
                else if (op == TOK_MINUS) fprintf(f, ",2)");
                else if (op == TOK_STAR) fprintf(f, ",3)");
                else if (op == TOK_SLASH) fprintf(f, ",4)");
                else if (op == TOK_MOD) fprintf(f, ",5)");
                else fprintf(f, ",1)");
            }
            break;
        }
        case NODE_UNARY: {
            if (n->data.unary.op == TOK_MINUS) {
                VarType ot = infer_expr_type(n->data.unary.operand);
                if (ot == TY_NUM) {
                    if (want == TY_NUM) { fprintf(f, "-("); cg_expr(f, n->data.unary.operand, TY_NUM); fprintf(f, ")"); }
                    else { fprintf(f, "make_num(-("); cg_expr(f, n->data.unary.operand, TY_NUM); fprintf(f, "))"); }
                } else {
                    fprintf(f, "val_neg(");
                    cg_expr(f, n->data.unary.operand, TY_DYN);
                    fprintf(f, ")");
                }
            } else if (n->data.unary.op == TOK_NOT) {
                fprintf(f, "make_num(!truthy(");
                cg_expr(f, n->data.unary.operand, TY_DYN);
                fprintf(f, "))");
            } else if (n->data.unary.op == TOK_AT) {
                VarType ot = infer_expr_type(n->data.unary.operand);
                if (ot == TY_NUM) {
                    if (want == TY_NUM) {
                        fprintf(f, "(double)(uintptr_t)&");
                        cg_expr(f, n->data.unary.operand, TY_NUM);
                    } else {
                        fprintf(f, "make_num((double)(uintptr_t)&");
                        cg_expr(f, n->data.unary.operand, TY_NUM);
                        fprintf(f, ")");
                    }
                } else {
                    fprintf(f, want == TY_NUM ? "0" : "make_num(0)");
                }
            } else if (n->data.unary.op == TOK_CARET) {
                if (want == TY_NUM) {
                    fprintf(f, "*(double*)(unsigned long)(");
                    cg_expr(f, n->data.unary.operand, TY_NUM);
                    fprintf(f, ")");
                } else {
                    fprintf(f, "make_num(*(double*)(unsigned long)(");
                    cg_expr(f, n->data.unary.operand, TY_NUM);
                    fprintf(f, "))");
                }
            }
            break;
        }
        case NODE_TEMPLATE: {
            fprintf(f, "make_str(eval_template(");
            char *esc = cescape(n->data.templ.raw);
            fprintf(f, "\"%s\"", esc);
            free(esc);
            fprintf(f, "))");
            break;
        }
        case NODE_FUNC_CALL: {
            fprintf(f, "call_func(\"%s\")", n->data.func_call.name);
            break;
        }
        default: fprintf(f, want == TY_NUM ? "0" : "make_num(0)"); break;
    }
}

static void cg_stmt(FILE *f, ASTNode *n, int *loop_ids, int loop_depth) {
    if (!n) return;
    switch (n->type) {
        case NODE_EMPTY: break;
        case NODE_ASSIGN: {
            int idx = var_find(n->data.assign.name);
            VarType vt = idx >= 0 ? var_types[idx] : TY_DYN;
            if (vt == TY_NUM) {
                fprintf(f, "%s = ", n->data.assign.name);
                cg_expr(f, n->data.assign.value, TY_NUM);
                fprintf(f, ";\n");
            } else if (vt == TY_STR) {
                fprintf(f, "val_free(%s); %s = ", n->data.assign.name, n->data.assign.name);
                cg_expr(f, n->data.assign.value, TY_DYN);
                fprintf(f, ";\n");
            } else {
                fprintf(f, "val_free(%s); %s = ", n->data.assign.name, n->data.assign.name);
                cg_expr(f, n->data.assign.value, TY_DYN);
                fprintf(f, ";\n");
            }
            break;
        }
        case NODE_PRINT: {
            for (int i = 0; i < n->data.print.count; i++) {
                VarType et = infer_expr_type(n->data.print.exprs[i]);
                if (i > 0) fprintf(f, "printf(\" \");\n");
                if (et == TY_NUM) {
                    fprintf(f, "printf(\"%%g\",");
                    cg_expr(f, n->data.print.exprs[i], TY_NUM);
                    fprintf(f, ");\n");
                } else {
                    fprintf(f, "{\n  Value _v = ");
                    cg_expr(f, n->data.print.exprs[i], TY_DYN);
                    fprintf(f, ";\n  if (_v.type == VAL_STR) { printf(\"%%s\",_v.data.str); val_free(_v); }\n");
                    fprintf(f, "  else printf(\"%%g\",_v.data.num);\n}\n");
                }
            }
            fprintf(f, "printf(\"\\n\");\n");
            break;
        }
        case NODE_WHILE: {
            int lid = cg_loop_id++;
            VarType ct = infer_expr_type(n->data.while_stmt.cond);
            if (ct == TY_NUM) {
                fprintf(f, "while (");
                cg_expr(f, n->data.while_stmt.cond, TY_NUM);
                fprintf(f, ") {\n");
            } else {
                fprintf(f, "while (truthy(");
                cg_expr(f, n->data.while_stmt.cond, TY_DYN);
                fprintf(f, ")) {\n");
            }
            int *new_ids = malloc((loop_depth + 1) * sizeof(int));
            if (loop_depth > 0) memcpy(new_ids, loop_ids, loop_depth * sizeof(int));
            new_ids[loop_depth] = lid;
            cg_stmt(f, n->data.while_stmt.body, new_ids, loop_depth + 1);
            free(new_ids);
            fprintf(f, "}\n");
            fprintf(f, "_lil_break_%d: ;\n", lid);
            break;
        }
        case NODE_LOOP: {
            int lid = cg_loop_id++;
            fprintf(f, "while (1) {\n");
            int *new_ids = malloc((loop_depth + 1) * sizeof(int));
            if (loop_depth > 0) memcpy(new_ids, loop_ids, loop_depth * sizeof(int));
            new_ids[loop_depth] = lid;
            cg_stmt(f, n->data.loop.body, new_ids, loop_depth + 1);
            free(new_ids);
            fprintf(f, "}\n");
            fprintf(f, "_lil_break_%d: ;\n", lid);
            break;
        }
        case NODE_STOP:
        case NODE_BREAK: {
            if (loop_depth > 0) {
                fprintf(f, "goto _lil_break_%d;\n", loop_ids[loop_depth - 1]);
            } else {
                fatal("line %d: stop/break outside a loop", n->line);
            }
            break;
        }
        case NODE_EXIT: fprintf(f, "return 0;\n"); break;
        case NODE_IF: {
            VarType ct = infer_expr_type(n->data.if_stmt.cond);
            fprintf(f, "if (");
            if (ct == TY_NUM) {
                cg_expr(f, n->data.if_stmt.cond, TY_NUM);
            } else {
                fprintf(f, "truthy(");
                cg_expr(f, n->data.if_stmt.cond, TY_DYN);
                fprintf(f, ")");
            }
            fprintf(f, ") {\n");
            cg_stmt(f, n->data.if_stmt.then, loop_ids, loop_depth);
            if (n->data.if_stmt.els) {
                if (((ASTNode*)n->data.if_stmt.els)->type == NODE_IF)
                    fprintf(f, "} else ");
                else
                    fprintf(f, "} else {\n");
                cg_stmt(f, (ASTNode*)n->data.if_stmt.els, loop_ids, loop_depth);
                if (((ASTNode*)n->data.if_stmt.els)->type != NODE_IF)
                    fprintf(f, "}\n");
                else
                    fprintf(f, "\n");
            } else {
                fprintf(f, "}\n");
            }
            break;
        }
        case NODE_FORTO: {
            int lid = cg_loop_id++;
            fprintf(f, "%s = val_tonum(", n->data.forto.var);
            cg_expr(f, n->data.forto.start, TY_DYN);
            fprintf(f, ");\n");
            fprintf(f, "{\n  double _end = val_tonum(");
            cg_expr(f, n->data.forto.end, TY_DYN);
            fprintf(f, ");\n  while (%s <= _end) {\n", n->data.forto.var);
            int *new_ids = malloc((loop_depth + 1) * sizeof(int));
            if (loop_depth > 0) memcpy(new_ids, loop_ids, loop_depth * sizeof(int));
            new_ids[loop_depth] = lid;
            cg_stmt(f, n->data.forto.body, new_ids, loop_depth + 1);
            free(new_ids);
            fprintf(f, "    %s += 1;\n", n->data.forto.var);
            fprintf(f, "  }\n  _lil_break_%d: ;\n}\n", lid);
            break;
        }
        case NODE_BLOCK: {
            for (int i = 0; i < n->data.block.count; i++) {
                cg_stmt(f, n->data.block.stmts[i], loop_ids, loop_depth);
            }
            break;
        }
        case NODE_INPUT: {
            int idx = var_find(n->data.input.name);
            VarType vt = idx >= 0 ? var_types[idx] : TY_STR;
            fprintf(f, "{\n  char _buf[4096];\n");
            if (n->data.input.prompt) {
                char *esc = cescape(n->data.input.prompt);
                fprintf(f, "  printf(\"%s\"); fflush(stdout);\n", esc);
                free(esc);
            }
            fprintf(f, "  if (fgets(_buf,4096,stdin)) {\n");
            fprintf(f, "    size_t _l = strlen(_buf);\n");
            fprintf(f, "    if (_l>0 && _buf[_l-1]=='\\n') _buf[_l-1]=0;\n");
            if (vt == TY_NUM) {
                fprintf(f, "    %s = val_tonum(make_str(_buf));\n", n->data.input.name);
            } else {
                fprintf(f, "    val_free(%s); %s = make_str(_buf);\n", n->data.input.name, n->data.input.name);
            }
            fprintf(f, "  }\n}\n");
            break;
        }
        case NODE_STRIFY: {
            fprintf(f, "{\n  char _b[128];\n  snprintf(_b,sizeof(_b),\"%%g\",%s.data.num);\n", n->data.input.name);
            fprintf(f, "  val_free(%s); %s = make_str(_b);\n}\n", n->data.input.name, n->data.input.name);
            break;
        }
        case NODE_INTIFY: {
            if (n->data.input.prompt) {
                char *fmt = n->data.input.prompt;
                fprintf(f, "{\n  char *_s = val_tostr(%s);\n", n->data.input.name);
                fprintf(f, "  size_t _n = strlen(_s);\n");
                fprintf(f, "  char *_o = malloc(_n * 16 + 1); _o[0] = 0;\n");
                fprintf(f, "  for (size_t _i = 0; _i < _n; _i++) {\n");
                fprintf(f, "    if (_i > 0) strcat(_o, \" \");\n");
                if (!strcmp(fmt, "binary")) {
                    fprintf(f, "    for (int _b = 7; _b >= 0; _b--) strcat(_o, ((unsigned char)_s[_i] >> _b) & 1 ? \"1\" : \"0\");\n");
                } else if (!strcmp(fmt, "hex")) {
                    fprintf(f, "    { char _b[16]; snprintf(_b,16,\"%%02x\",(unsigned char)_s[_i]); strcat(_o,_b); }\n");
                } else {
                    fprintf(f, "    { char _b[16]; snprintf(_b,16,\"%%03o\",(unsigned char)_s[_i]); strcat(_o,_b); }\n");
                }
                fprintf(f, "  }\n");
                fprintf(f, "  free(_s); val_free(%s); %s = make_str(_o); free(_o);\n", n->data.input.name, n->data.input.name);
                fprintf(f, "}\n");
            } else {
                fprintf(f, "%s.data.num = val_tonum(%s);\n", n->data.input.name, n->data.input.name);
                fprintf(f, "%s.type = VAL_NUM;\n", n->data.input.name);
            }
            break;
        }
        case NODE_TRY: {
            fprintf(f, "{\n  jmp_buf _save;\n  memcpy(_save,_try_jmp,sizeof(jmp_buf));\n");
            fprintf(f, "  if (setjmp(_try_jmp) == 0) {\n");
            cg_stmt(f, n->data.try_stmt.body, loop_ids, loop_depth);
            fprintf(f, "    memcpy(_try_jmp,_save,sizeof(jmp_buf));\n");
            fprintf(f, "  } else {\n");
            fprintf(f, "    memcpy(_try_jmp,_save,sizeof(jmp_buf));\n");
            cg_stmt(f, n->data.try_stmt.catch_body, loop_ids, loop_depth);
            fprintf(f, "  }\n}\n");
            break;
        }
    }
}

static int generate_c(const char *path, const char *outpath) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "error: cannot open '%s': %s\n", path, strerror(errno)); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *src = malloc(sz + 1);
    if (!src) { fclose(f); fprintf(stderr, "out of memory\n"); return 1; }
    long got = fread(src, 1, sz, f);
    fclose(f);
    if (got < sz) fprintf(stderr, "warning: short read from '%s'\n", path);
    src[got] = 0;

    char *psrc = src;
    while (*psrc == ' ' || *psrc == '\t' || *psrc == '\n' || *psrc == '\r') psrc++;
    if (psrc[0] == '[' && strncmp(psrc, "[compiled]", 10) == 0
        && (psrc[10] == '\n' || psrc[10] == '\r' || psrc[10] == 0)) {
        psrc = psrc + 10;
        while (*psrc == '\n' || *psrc == '\r') psrc++;
    }

    if (setjmp(error_jmp)) { free(src); return 1; }

    lex_init(psrc);
    lex_next();
    ASTNode *prog = parse_program();

    char cpath[1024];
    snprintf(cpath, sizeof(cpath), "%s.c", outpath);
    f = fopen(cpath, "w");
    if (!f) { fprintf(stderr, "error: cannot write '%s'\n", cpath); free(src); return 1; }

    fprintf(f, "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <stdint.h>\n#include <math.h>\n#include <setjmp.h>\n\n");
    fprintf(f, "typedef enum { VAL_NUM, VAL_STR } ValType;\n");
    fprintf(f, "typedef struct { ValType type; union { double num; char *str; } data; } Value;\n");
    fprintf(f, "static jmp_buf _try_jmp;\n");
    fprintf(f, "static Value make_num(double n) { Value v = {VAL_NUM,{.num=n}}; return v; }\n");
    fprintf(f, "static Value make_str(const char *s) { Value v = {VAL_STR,{.str=strdup(s)}}; return v; }\n");
    fprintf(f, "static void val_free(Value v) { if (v.type == VAL_STR) free(v.data.str); }\n");
    fprintf(f, "static Value copy_val(Value v) { if (v.type==VAL_STR) v.data.str=strdup(v.data.str); return v; }\n");
    fprintf(f, "static int truthy(Value v) { if (v.type == VAL_STR) return strlen(v.data.str)!=0; return v.data.num!=0; }\n");
    fprintf(f, "static char *val_tostr(Value v) {\n");
    fprintf(f, "  if (v.type == VAL_STR) return strdup(v.data.str);\n");
    fprintf(f, "  char buf[128]; double d = v.data.num;\n");
    fprintf(f, "  if (d==(long)d) snprintf(buf,sizeof(buf),\"%%ld\",(long)d);\n");
    fprintf(f, "  else snprintf(buf,sizeof(buf),\"%%.10g\",d);\n");
    fprintf(f, "  return strdup(buf);\n");
    fprintf(f, "}\n");
    fprintf(f, "static double val_tonum(Value v) {\n");
    fprintf(f, "  if (v.type == VAL_NUM) return v.data.num;\n");
    fprintf(f, "  char *end; double d = strtod(v.data.str,&end);\n");
    fprintf(f, "  if (*end) { fprintf(stderr,\"convert error\\n\"); longjmp(_try_jmp,1); }\n");
    fprintf(f, "  return d;\n");
    fprintf(f, "}\n");
    fprintf(f, "static Value val_add(Value a, Value b) {\n");
    fprintf(f, "  if (a.type==VAL_STR||b.type==VAL_STR) {\n");
    fprintf(f, "    char *as=val_tostr(a),*bs=val_tostr(b);\n");
    fprintf(f, "    char *r=malloc(strlen(as)+strlen(bs)+1);\n");
    fprintf(f, "    strcpy(r,as); strcat(r,bs);\n");
    fprintf(f, "    val_free(a);val_free(b);free(as);free(bs);\n");
    fprintf(f, "    Value vr=make_str(r);free(r);return vr;\n");
    fprintf(f, "  }\n");
    fprintf(f, "  double r=a.data.num+b.data.num;\n");
    fprintf(f, "  val_free(a);val_free(b);return make_num(r);\n");
    fprintf(f, "}\n");
    fprintf(f, "static Value val_arith(Value a, Value b, int op) {\n");
    fprintf(f, "  if (a.type==VAL_STR||b.type==VAL_STR) { val_free(a);val_free(b);return make_num(0); }\n");
    fprintf(f, "  double av=a.data.num,bv=b.data.num;\n");
    fprintf(f, "  val_free(a);val_free(b);\n");
    fprintf(f, "  double r=0;\n");
    fprintf(f, "  switch(op) {\n");
    fprintf(f, "    case 1: r=av+bv;break; case 2: r=av-bv;break;\n");
    fprintf(f, "    case 3: r=av*bv;break; case 4: r=bv==0?0:av/bv;break;\n");
    fprintf(f, "    case 5: r=bv==0?0:(double)((int)av%%(int)bv);break;\n");
    fprintf(f, "  }\n");
    fprintf(f, "  return make_num(r);\n");
    fprintf(f, "}\n");
    fprintf(f, "static Value val_neg(Value a) {\n");
    fprintf(f, "  if (a.type==VAL_STR) { val_free(a); return make_num(0); }\n");
    fprintf(f, "  double r=-a.data.num; val_free(a); return make_num(r);\n");
    fprintf(f, "}\n");
    fprintf(f, "static int val_cmp(Value a, Value b, int op) {\n");
    fprintf(f, "  int r; double av, bv;\n");
    fprintf(f, "  if (a.type==VAL_STR||b.type==VAL_STR) {\n");
    fprintf(f, "    char *as=val_tostr(a),*bs=val_tostr(b);\n");
    fprintf(f, "    int c=strcmp(as,bs);free(as);free(bs);\n");
    fprintf(f, "    if(op==0)r=c==0;else if(op==1)r=c!=0;else if(op==2)r=c<0;\n");
    fprintf(f, "    else if(op==3)r=c>0;else if(op==4)r=c<=0;else r=c>=0;\n");
    fprintf(f, "  } else {\n");
    fprintf(f, "    av=a.data.num; bv=b.data.num;\n");
    fprintf(f, "    if(op==0)r=av==bv;else if(op==1)r=av!=bv;else if(op==2)r=av<bv;\n");
    fprintf(f, "    else if(op==3)r=av>bv;else if(op==4)r=av<=bv;else r=av>=bv;\n");
    fprintf(f, "  }\n");
    fprintf(f, "  val_free(a);val_free(b);return r;\n");
    fprintf(f, "}\n\n");

    fprintf(f, "#define MAX_VARS 1024\n");
    fprintf(f, "typedef struct { char *name; Value val; } Var;\n");
    fprintf(f, "static Var vars[MAX_VARS];\n");
    fprintf(f, "static int var_count;\n");
    fprintf(f, "static int var_ensure(const char *name) {\n");
    fprintf(f, "  for (int i=0;i<var_count;i++) if(!strcmp(vars[i].name,name)) return i;\n");
    fprintf(f, "  if (var_count>=MAX_VARS) { fprintf(stderr,\"too many vars\\n\"); longjmp(_try_jmp,1); }\n");
    fprintf(f, "  vars[var_count].name=strdup(name);\n");
    fprintf(f, "  vars[var_count].val=(Value){VAL_NUM,{.num=0}};\n");
    fprintf(f, "  return var_count++;\n");
    fprintf(f, "}\n\n");

    var_count = 0;
    cg_loop_id = 0;
    for (int i = 0; i < MAX_VARS; i++) var_types[i] = TY_DYN;
    for (int i = 0; i < prog->data.block.count; i++)
        infer_type_stmt(prog->data.block.stmts[i]);

    fprintf(f, "int main() {\n");
    cg_varinit(f, prog);
    int *empty = NULL;
    cg_stmt(f, prog, empty, 0);
    fprintf(f, "  for (int i=0;i<var_count;i++) if(vars[i].val.type==VAL_STR) free(vars[i].val.data.str);\n");
    fprintf(f, "  return 0;\n}\n");
    fclose(f);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "gcc -O2 -o %s %s -lm 2>&1", outpath, cpath);
    int ret = system(cmd);
    if (ret) {
        fprintf(stderr, "error: compilation failed (gcc returned %d)\n", ret);
        free(src);
        return 1;
    }

    printf("compiled: %s\n", outpath);
    free(src);
    return 0;
}

static void run_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "error: cannot open '%s': %s\n", path, strerror(errno)); return; }
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
    char *psrc = src;
    while (*psrc == ' ' || *psrc == '\t' || *psrc == '\n' || *psrc == '\r') psrc++;
    if (psrc[0] == '[' && strncmp(psrc, "[compiled]", 10) == 0
        && (psrc[10] == '\n' || psrc[10] == '\r' || psrc[10] == 0)) {
        compiled_header = 1;
        psrc = psrc + 10;
        while (*psrc == '\n' || *psrc == '\r') psrc++;
    }
    if (compiled_header && !compile_mode) {
        fprintf(stderr, "error: code is in compiled mode (use -c to compile)\n");
        free(src);
        return;
    }

    if (setjmp(error_jmp)) { free(src); return; }

    lex_init(psrc);
    lex_next();

    ASTNode *prog = parse_program();
    free(src);

    code_len = 0; const_len = 0; fb_len = 0;
    fixup_len = 0; cur_loop = 0; for_counter = 0;
    compile_prog(prog->data.block.stmts, prog->data.block.count);
    vm_run();
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
                if (r == 1) return;
            }
            if (lex_cur.type == TOK_NEWLINE) lex_next();
        }
    }
}

int main(int argc, char **argv) {
    error_occurred = 0;
    compile_mode = 0;
    const char *inpath = NULL;
    const char *outpath = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            printf("usage: lil [options] [file.lil]\n");
            printf("  no args      start REPL\n");
            printf("  file.lil     execute file\n");
            printf("  -c file.lil  compile to standalone binary\n");
            printf("  -o output    output filename (default: a.out)\n");
            return 0;
        } else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--compile")) {
            compile_mode = 1;
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
        if (!outpath) {
            const char *dot = inpath ? strrchr(inpath, '.') : NULL;
            if (dot) {
                size_t n = dot - inpath;
                char *buf = malloc(n + 1);
                memcpy(buf, inpath, n); buf[n] = 0;
                outpath = buf;
            } else {
                outpath = "a.out";
            }
        }
        if (!inpath) { fprintf(stderr, "error: no input file\n"); return 1; }
        return generate_c(inpath, outpath);
    }
    if (inpath) {
        run_file(inpath);
    } else {
        repl();
    }
    return error_occurred ? 1 : 0;
}
