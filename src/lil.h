#ifndef LIL_H
#define LIL_H

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
#include <math.h>
#include <unistd.h>

#ifdef HAVE_GTK
#include <gtk/gtk.h>
#endif

#define VERSION "0.1.0"
#define MAX_VARS 1024
#define MAX_FUNCS 256
#define MAX_GTK_WIDGETS 256
#define MAX_STACK 256
#define MAX_CODE 65536
#define MAX_STR 256
#define MAX_CONST 256
#define MAX_FALLBACKS 256
#define BUF_SIZE 4096
#define MAX_FIXUPS 256
#define MAX_LOOP_STACK 64
#define VM_STACK 2048
#define MAX_ASSIGN_HISTORY 65536

#define FORCE_NONE 0
#define FORCE_INT 1
#define FORCE_STR 2
#define FORCE_INPUT_INT 3
#define FORCE_INPUT_STR 4

typedef enum { VAL_NUM, VAL_STR, VAL_LIST, VAL_DICT } ValType;

typedef struct Value {
    ValType type;
    union { double num; char *str; struct { struct Value *items; int count; int cap; } list;
        struct { char **keys; struct Value *values; int count; int cap; } dict; } data;
} Value;


typedef enum { TY_NUM, TY_STR, TY_DYN } VarType;

typedef enum { TOK_NUM, TOK_STR, TOK_ID, TOK_PRINT, TOK_INPUT, TOK_IF, TOK_ELSE, TOK_ELIF,
    TOK_WHILE, TOK_FOR, TOK_TO, TOK_GET, TOK_FROM, TOK_EXIT, TOK_PLUS, TOK_MINUS, TOK_STAR,
    TOK_SLASH, TOK_MOD, TOK_EQ, TOK_NE, TOK_LT, TOK_GT, TOK_LE, TOK_GE, TOK_ASSIGN,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE, TOK_COMMA,
    TOK_SEMI, TOK_NEWLINE, TOK_EOF, TOK_AND, TOK_OR, TOK_NOT,
    TOK_LOOP, TOK_STOP, TOK_BREAK, TOK_CONTINUE, TOK_INCLUDE, TOK_STRINGIFY, TOK_INTIFY, TOK_TOGGLE,
    TOK_HAS, TOK_NOCASE, TOK_ANYWHERE, TOK_WORD,
    TOK_LBRACKET, TOK_RBRACKET,
    TOK_TEMPLATE, TOK_DOLLAR_ID, TOK_AT, TOK_CARET,     TOK_AMPERSAND, TOK_PIPE, TOK_DOT,
    TOK_TRY, TOK_CATCH, TOK_FORCE, TOK_UNFORCE, TOK_QMARK, TOK_UNINCLUDE, TOK_COLON, TOK_STRUCT } TokenType;

typedef struct {
    TokenType type;
    int line;
    union { double num; char *str; } val;
} Token;

typedef enum { NODE_NUM, NODE_STR, NODE_ID, NODE_BINOP, NODE_UNARY,
    NODE_ASSIGN, NODE_PRINT, NODE_INPUT, NODE_IF, NODE_WHILE,
    NODE_FORTO, NODE_BLOCK, NODE_EXIT, NODE_EMPTY,
    NODE_LOOP, NODE_STOP, NODE_INCLUDE, NODE_GET, NODE_FUNCTION,
    NODE_TEMPLATE, NODE_FUNC_DEF, NODE_FUNC_CALL, NODE_BREAK, NODE_CONTINUE,
    NODE_STRINGIFY, NODE_INTIFY, NODE_TOGGLE, NODE_TRY, NODE_FORCE, NODE_UNFORCE, NODE_SET_UNDEF,
    NODE_LIST, NODE_INDEX, NODE_INDEX_SET, NODE_DICT, NODE_STRUCT_DEF } NodeType;

typedef struct ASTNode {
    NodeType type;
    int line;
    union {
        double num;
        char *str;
        char *id;
        struct { int op; struct ASTNode *left, *right; } binop;
        struct { int op; struct ASTNode *operand; } unary;
        struct { char *name; struct ASTNode *value; int force_type; } force;
        struct { char *name; struct ASTNode *value; } assign;
        struct { struct ASTNode **exprs; int count; int cap; } print;
        struct { char *name; char *prompt; int force_type; } input;
        struct { struct ASTNode *cond, *then, *els; int flags; int has_mode;
            struct ASTNode *has_item; struct ASTNode **has_items; int has_nitems; } if_stmt;
        struct { struct ASTNode *cond, *body; } while_stmt;
        struct { char *var; struct ASTNode *start, *end, *body; } forto;
        struct { struct ASTNode *body; } loop;
        struct { char *name; char **params; int nparams; struct ASTNode *body; } func_def;
        struct { char *name; struct ASTNode **args; int nargs; } func_call;
        struct { char *path; char **funcs; int nfuncs; } include;
        struct { char **varnames; char **newnames; int nvars; char *path; int *indices; } get_stmt;
        struct { char *lib; char **args; int argc; } funcall;
        struct { char *raw; } templ;
        struct { struct ASTNode *body, *catch_body; } try_stmt;
        struct { char *name; char *fmt; } modify;
        struct { struct ASTNode **elements; int count; } list;
        struct { struct ASTNode **keys; struct ASTNode **values; int count; } dict;
        struct { char *name; char **fields; int nfields; } struct_def;
        struct { struct ASTNode *container, *index; } idx;
        struct { struct ASTNode *container, *index, *value; } idx_set;
        struct { struct ASTNode **stmts; int count, cap; } block;
    } data;
} ASTNode;

typedef struct {
    char *name;
    char **params;
    int nparams;
    struct ASTNode *body;
} FuncDef;

typedef struct {
    char *name;
    char **fields;
    int nfields;
} StructDef;

typedef struct {
    char *name;
    Value val;
    int forced;
    int forced_type;
    int scope_id;
} Var;

#ifdef HAVE_GTK
typedef struct { char name[64]; GtkWidget *widget; } GtkW;
typedef struct { char vname[64]; char sig[64]; } GtkSigData;
#endif

typedef struct { uint16_t op; int arg; } Instr;

typedef struct {
    int var_count;
    Var vars[MAX_VARS];
    int func_count;
    FuncDef funcs[MAX_FUNCS];
    int struct_count;
    StructDef structs[MAX_FUNCS];
    int assign_hist_count;
    int lib_imported[8];
    jmp_buf error_jmp;
    int scope_depth;
} LilState;

extern Var vars[MAX_VARS];
extern int var_count;
extern int error_occurred;
extern FuncDef funcs[MAX_FUNCS];
extern int func_count;
extern StructDef structs[MAX_FUNCS];
extern int struct_count;
extern jmp_buf error_jmp;
extern Value undef_val;
extern int lib_imported[8];
extern Value assign_history[MAX_ASSIGN_HISTORY];
extern int assign_hist_count;
extern int assign_var_idx[MAX_ASSIGN_HISTORY];
extern int compile_mode;
extern int compiled_header;
extern Token lex_cur;
extern int scope_depth;

void fatal(const char *fmt, ...);
char *sdup(const char *s);
char *sdupn(const char *s, size_t n);
void *safe_alloc(size_t sz);

int var_find(const char *name);
Value var_get(const char *name);
int var_ensure(const char *name);
void var_set(const char *name, Value v);
Value var_get_history(int var_idx, int nth);

Value make_num(double n);
Value make_str(const char *s);
Value make_list(void);
void list_append(Value *v, Value item);
Value list_get(Value v, int idx);
void list_set(Value *v, int idx, Value item);
int list_len(Value v);
Value make_dict(void);
void dict_set(Value *v, const char *key, Value val);
Value dict_get(Value v, const char *key);
int dict_has(Value v, const char *key);
int dict_len(Value v);
Value dict_keys(Value v);
int dict_remove(Value *v, const char *key);
void dict_clear(Value *v);

char *val_tostr(Value v);
double val_tonum(Value v);
void val_free(Value v);
Value copy_val(Value v);
int truthy(Value v);
Value val_add(Value a, Value b);
Value val_arith(Value a, Value b, int op);
Value val_neg(Value a);
int val_cmp(Value a, Value b, int op);

void lex_init(const char *src);
Token lex_next(void);
Token lex_peek_next(void);

ASTNode *parse_program(void);
ASTNode *parse_stmt(void);

Value eval_expr(ASTNode *n);
int exec_stmt(ASTNode *n);
void vm_run(void);
void compile_prog(ASTNode **stmts, int nstmts);
extern int code_len;
extern int const_len;
extern int fb_len;
extern int fixup_len;
extern int cur_loop;
extern int for_counter;
extern Value _last_expr_val;
extern char last_error[256];
extern int in_try;

int lib_idx(const char *name);
Value lib_dispatch(const char *lib, const char *fn, int argc, char **args, int line);

char *cescape(const char *s);
extern int cg_loop_id;
extern VarType var_types[MAX_VARS];

int generate_c(const char *path, const char *outpath);

void state_save(LilState *s);
void state_restore(LilState *s);
void push_scope(void);
void pop_scope(void);
int get_scope_depth(void);
void var_set_no_scope(const char *name, Value v);

void run_file(const char *path);
void repl(void);

char *str_lower(const char *s);
char *resolve_arg(char *arg);
void cg_expr(FILE *f, ASTNode *n, VarType want);
void cg_stmt(FILE *f, ASTNode *n, int *loop_ids, int loop_depth);
void cg_collect_vars(ASTNode *n);
void cg_varinit(FILE *f, ASTNode *prog);
VarType infer_expr_type(ASTNode *n);
void infer_type_stmt(ASTNode *n);
void cg_emit_func(FILE *f, ASTNode *n);
int is_cstmt(ASTNode *n);
void typecheck_prog(ASTNode *prog);

#ifdef HAVE_GTK
GtkWidget *gtk_find_w(const char *name);
void gtk_reg_w(const char *name, GtkWidget *w);
Value gtk_dispatch(const char *fn, int argc, char **args, int line);
#endif

int check_has(ASTNode *lhs_expr, ASTNode *item, ASTNode **items, int nitems, int nocase);
int check_cond_flags(ASTNode *cond, int flags);
int is_word_bound(char c);

#endif
