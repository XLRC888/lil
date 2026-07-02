#include "lil.h"

int error_occurred;
Value undef_val = {VAL_NUM, {.num=0}};
int compile_mode;
int compiled_header;
Var vars[MAX_VARS];
int var_count;
jmp_buf error_jmp;

#define MAX_ASSIGN_HISTORY 65536
Value assign_history[MAX_ASSIGN_HISTORY];
int assign_hist_count;
int assign_var_idx[MAX_ASSIGN_HISTORY];
Value _last_expr_val;
char last_error[256];
int in_try;

void fatal(const char *fmt, ...) {
    error_occurred = 1;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(last_error, sizeof(last_error), fmt, ap);
    va_end(ap);
    if (!in_try)
        fprintf(stderr, "\033[1;31merror:\033[0m %s\n", last_error);
    fflush(stderr);
    longjmp(error_jmp, 1);
}

int var_find(const char *name) {
    for (int i = 0; i < var_count; i++)
        if (!strcmp(vars[i].name, name)) return i;
    return -1;
}

Value var_get(const char *name) {
    int i = var_find(name);
    if (i < 0) return undef_val;
    if (vars[i].val.type == VAL_STR) return make_str(vars[i].val.data.str);
    if (vars[i].val.type == VAL_LIST) return copy_val(vars[i].val);
    return make_num(vars[i].val.data.num);
}

int var_ensure(const char *name) {
    int i = var_find(name);
    if (i >= 0) return i;
    if (var_count >= MAX_VARS) fatal("too many variables");
    vars[var_count].name = sdup(name);
    vars[var_count].val = undef_val;
    if (undef_val.type == VAL_STR) vars[var_count].val.data.str = sdup(undef_val.data.str);
    vars[var_count].forced = 0;
    return var_count++;
}

void var_set(const char *name, Value v) {
    int i = var_find(name);
    if (i >= 0) {
        if (vars[i].forced) {
            if (vars[i].forced_type == FORCE_NONE) fatal("cannot assign to a forced variable");
            if (vars[i].forced_type == FORCE_INT && v.type == VAL_STR) fatal("cannot assign a string to an int-forced variable");
            if (vars[i].forced_type == FORCE_STR && v.type == VAL_NUM) fatal("cannot assign a number to a str-forced variable");
        }
        if (assign_hist_count < MAX_ASSIGN_HISTORY) {
            assign_history[assign_hist_count] = copy_val(vars[i].val);
            assign_var_idx[assign_hist_count] = i;
            assign_hist_count++;
        }
        if (vars[i].val.type == VAL_STR) free(vars[i].val.data.str);
        vars[i].val = v;
    } else {
        if (var_count >= MAX_VARS) fatal("too many variables");
        vars[var_count].name = sdup(name);
        vars[var_count].val = v;
        var_count++;
    }
}

void state_save(LilState *s) {
    s->var_count = var_count;
    memcpy(s->vars, vars, sizeof(Var) * var_count);
    s->func_count = func_count;
    memcpy(s->funcs, funcs, sizeof(FuncDef) * func_count);
    s->assign_hist_count = assign_hist_count;
    memcpy(s->lib_imported, lib_imported, sizeof(lib_imported));
    memcpy(s->error_jmp, error_jmp, sizeof(jmp_buf));
}

void state_restore(LilState *s) {
    memcpy(error_jmp, s->error_jmp, sizeof(jmp_buf));
    memcpy(vars, s->vars, sizeof(Var) * s->var_count);
    var_count = s->var_count;
    func_count = s->func_count;
    memcpy(funcs, s->funcs, sizeof(FuncDef) * s->func_count);
    assign_hist_count = s->assign_hist_count;
    memcpy(lib_imported, s->lib_imported, sizeof(lib_imported));
}

void run_file_interpreted(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "error: cannot open '%s': %s\n", path, strerror(errno)); return; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *src = malloc(sz + 1);
    if (!src) { fclose(f); fprintf(stderr, "out of memory\n"); return; }
    long got = fread(src, 1, sz, f);
    fclose(f);
    src[got] = 0;
    if (setjmp(error_jmp)) { free(src); return; }
    lex_init(src);
    lex_next();
    while (lex_cur.type != TOK_EOF) {
        ASTNode *s = parse_stmt();
        if (s->type != NODE_EMPTY) {
            int r = exec_stmt(s);
            if (r == 1) break;
        }
        if (lex_cur.type == TOK_NEWLINE) lex_next();
    }
    free(src);
}

Value var_get_history(int var_idx, int nth) {
    int cnt = 0;
    for (int i = assign_hist_count - 1; i >= 0; i--) {
        if (assign_var_idx[i] == var_idx) {
            if (cnt == nth) return assign_history[i];
            cnt++;
        }
    }
    return undef_val;
}

Value make_num(double n) {
    Value v = {VAL_NUM, {.num=n}};
    return v;
}

Value make_str(const char *s) {
    Value v = {VAL_STR, {.str=sdup(s)}};
    return v;
}

Value make_list(void) {
    Value v;
    v.type = VAL_LIST;
    v.data.list.items = NULL;
    v.data.list.count = 0;
    v.data.list.cap = 0;
    return v;
}

void list_append(Value *v, Value item) {
    if (v->type != VAL_LIST) fatal("not a list");
    if (v->data.list.count >= v->data.list.cap) {
        v->data.list.cap = v->data.list.cap ? v->data.list.cap * 2 : 4;
        v->data.list.items = realloc(v->data.list.items, sizeof(Value) * v->data.list.cap);
        if (!v->data.list.items) fatal("out of memory");
    }
    v->data.list.items[v->data.list.count++] = item;
}

Value list_get(Value v, int idx) {
    if (v.type != VAL_LIST) fatal("not a list");
    if (idx < 0 || idx >= v.data.list.count) fatal("list index out of range");
    return copy_val(v.data.list.items[idx]);
}

void list_set(Value *v, int idx, Value item) {
    if (v->type != VAL_LIST) fatal("not a list");
    if (idx < 0) fatal("list index out of range");
    if (idx >= v->data.list.count) {
        while (v->data.list.cap <= idx) {
            v->data.list.cap = v->data.list.cap ? v->data.list.cap * 2 : 4;
            v->data.list.items = realloc(v->data.list.items, sizeof(Value) * v->data.list.cap);
            if (!v->data.list.items) fatal("out of memory");
        }
        while (v->data.list.count <= idx) {
            v->data.list.items[v->data.list.count++] = make_num(0);
        }
    }
    val_free(v->data.list.items[idx]);
    v->data.list.items[idx] = item;
}

int list_len(Value v) {
    if (v.type != VAL_LIST) return 0;
    return v.data.list.count;
}

char *val_tostr(Value v) {
    if (v.type == VAL_STR) return sdup(v.data.str);
    if (v.type == VAL_LIST) {
        size_t cap = 64, pos = 0;
        char *buf = malloc(cap);
        if (!buf) fatal("out of memory");
        buf[pos++] = '[';
        for (int i = 0; i < v.data.list.count; i++) {
            if (i > 0) {
                if (pos + 2 >= cap) { cap *= 2; buf = realloc(buf, cap); if (!buf) fatal("out of memory"); }
                buf[pos++] = ','; buf[pos++] = ' ';
            }
            char *s = val_tostr(v.data.list.items[i]);
            size_t sl = strlen(s);
            while (pos + sl + 2 >= cap) { cap *= 2; buf = realloc(buf, cap); if (!buf) fatal("out of memory"); }
            memcpy(buf + pos, s, sl); pos += sl;
            free(s);
        }
        if (pos + 2 >= cap) { buf = realloc(buf, cap + 2); if (!buf) fatal("out of memory"); }
        buf[pos++] = ']'; buf[pos] = 0;
        return buf;
    }
    double d = v.data.num;
    char buf[128];
    if (d == (long)d) snprintf(buf, sizeof(buf), "%ld", (long)d);
    else snprintf(buf, sizeof(buf), "%.10g", d);
    return sdup(buf);
}

double val_tonum(Value v) {
    if (v.type == VAL_NUM) return v.data.num;
    if (v.type == VAL_LIST) return v.data.list.count;
    char *end;
    double d = strtod(v.data.str, &end);
    if (*end) fatal("cannot convert '%s' to number", v.data.str);
    return d;
}

void val_free(Value v) {
    if (v.type == VAL_STR) free(v.data.str);
    else if (v.type == VAL_LIST) {
        for (int i = 0; i < v.data.list.count; i++)
            val_free(v.data.list.items[i]);
        free(v.data.list.items);
    }
}

Value copy_val(Value v) {
    if (v.type == VAL_STR) v.data.str = sdup(v.data.str);
    else if (v.type == VAL_LIST) {
        Value *items = malloc(sizeof(Value) * (v.data.list.cap > 0 ? v.data.list.cap : 4));
        if (!items) fatal("out of memory");
        for (int i = 0; i < v.data.list.count; i++)
            items[i] = copy_val(v.data.list.items[i]);
        v.data.list.items = items;
    }
    return v;
}

int truthy(Value v) {
    if (v.type == VAL_STR) return strlen(v.data.str) != 0;
    if (v.type == VAL_LIST) return v.data.list.count != 0;
    return v.data.num != 0;
}

Value val_add(Value a, Value b) {
    if (a.type == VAL_LIST || b.type == VAL_LIST) {
        char *as = val_tostr(a), *bs = val_tostr(b);
        char *r = malloc(strlen(as) + strlen(bs) + 1);
        if (!r) fatal("out of memory");
        strcpy(r, as); strcat(r, bs);
        val_free(a); val_free(b); free(as); free(bs);
        Value vr = make_str(r); free(r); return vr;
    }
    if (a.type == VAL_STR || b.type == VAL_STR) {
        char *as = val_tostr(a), *bs = val_tostr(b);
        char *r = malloc(strlen(as) + strlen(bs) + 1);
        if (!r) fatal("out of memory");
        strcpy(r, as); strcat(r, bs);
        val_free(a); val_free(b); free(as); free(bs);
        Value vr = make_str(r); free(r); return vr;
    }
    double rv = a.data.num + b.data.num;
    val_free(a); val_free(b); return make_num(rv);
}

Value val_arith(Value a, Value b, int op) {
    if (a.type == VAL_LIST || b.type == VAL_LIST) { val_free(a); val_free(b); return make_num(0); }
    if (a.type == VAL_STR || b.type == VAL_STR) { val_free(a); val_free(b); return make_num(0); }
    double av = a.data.num, bv = b.data.num;
    val_free(a); val_free(b);
    double r = 0;
    switch (op) {
        case 1: r = av + bv; break;
        case 2: r = av - bv; break;
        case 3: r = av * bv; break;
        case 4: r = bv == 0 ? 0 : av / bv; break;
        case 5: r = bv == 0 ? 0 : (double)((int)av % (int)bv); break;
    }
    return make_num(r);
}

Value val_neg(Value a) {
    if (a.type == VAL_LIST) { val_free(a); return make_num(0); }
    if (a.type == VAL_STR) { val_free(a); return make_num(0); }
    double r = -a.data.num; val_free(a); return make_num(r);
}

int val_cmp(Value a, Value b, int op) {
    int r; double av, bv;
    if (a.type == VAL_STR || b.type == VAL_STR) {
        char *as = val_tostr(a), *bs = val_tostr(b);
        int c = strcmp(as, bs); free(as); free(bs);
        if (op == 0) r = c == 0; else if (op == 1) r = c != 0; else if (op == 2) r = c < 0;
        else if (op == 3) r = c > 0; else if (op == 4) r = c <= 0; else r = c >= 0;
    } else {
        av = a.data.num; bv = b.data.num;
        if (op == 0) r = av == bv; else if (op == 1) r = av != bv; else if (op == 2) r = av < bv;
        else if (op == 3) r = av > bv; else if (op == 4) r = av <= bv; else r = av >= bv;
    }
    val_free(a); val_free(b); return r;
}

Value stmt_val(ASTNode *n);
int exec_stmt(ASTNode *n);

Value eval_expr(ASTNode *n) {
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
                    int saved_count = var_count;
                    Var saved_vars[MAX_VARS];
                    memcpy(saved_vars, vars, sizeof(Var) * var_count);
                    for (int j = 0; j < funcs[i].nparams && j < n->data.func_call.nargs; j++) {
                        Value v = eval_expr(n->data.func_call.args[j]);
                        var_set(funcs[i].params[j], v);
                    }
                    Value ret = make_num(0);
                    int r = exec_stmt(funcs[i].body);
                    if (r == 1) { memcpy(vars, saved_vars, sizeof(Var) * saved_count); var_count = saved_count; return make_num(0); }
                    if (funcs[i].body->type == NODE_BLOCK && funcs[i].body->data.block.count > 0) {
                        ASTNode *last = funcs[i].body->data.block.stmts[funcs[i].body->data.block.count - 1];
                        if (last) {
                            if (last->type == NODE_ASSIGN) {
                                int vi = var_find(last->data.assign.name);
                                if (vi >= 0) ret = var_get(last->data.assign.name);
                            } else {
                                ret = copy_val(_last_expr_val);
                            }
                        }
                    }
                    memcpy(vars, saved_vars, sizeof(Var) * saved_count);
                    var_count = saved_count;
                    return ret;
                }
            }
            fatal("line %d: function '%s' not defined", n->line, n->data.func_call.name);
            return make_num(0);
        }
        case NODE_LIST: {
            Value list = make_list();
            for (int i = 0; i < n->data.list.count; i++) {
                Value elem = eval_expr(n->data.list.elements[i]);
                list_append(&list, elem);
            }
            return list;
        }
        case NODE_INDEX: {
            Value container = eval_expr(n->data.idx.container);
            Value idx = eval_expr(n->data.idx.index);
            double di = val_tonum(idx);
            val_free(idx);
            if (container.type != VAL_LIST) { val_free(container); fatal("line %d: cannot index non-list", n->line); }
            int iidx = (int)di;
            if (iidx < 0 || iidx >= container.data.list.count) { val_free(container); fatal("line %d: list index out of range", n->line); }
            Value result = copy_val(container.data.list.items[iidx]);
            val_free(container);
            return result;
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
                    val_free(l); val_free(r);
                    return concatv;
                }
                val_free(l); val_free(r);
                return make_num(l.data.num + r.data.num);
            }
            if (op == TOK_MINUS) { double _r = val_tonum(l) - val_tonum(r); val_free(l); val_free(r); return make_num(_r); }
            if (op == TOK_STAR)  { double _r = val_tonum(l) * val_tonum(r); val_free(l); val_free(r); return make_num(_r); }
            if (op == TOK_SLASH) {
                double rd = val_tonum(r);
                if (rd == 0) fatal("line %d: division by zero", n->line);
                double _r = val_tonum(l) / rd;
                val_free(l); val_free(r);
                return make_num(_r);
            }
            if (op == TOK_MOD) {
                long rd = (long)val_tonum(r);
                if (rd == 0) fatal("line %d: modulo by zero", n->line);
                double _r = (long)val_tonum(l) % rd;
                val_free(l); val_free(r);
                return make_num(_r);
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
            val_free(l); val_free(r);
            return make_num(result);
        }
        case NODE_FUNCTION: {
            if (n->data.funcall.argc < 1)
                fatal("line %d: function expects a name", n->line);
            int li = lib_idx(n->data.funcall.lib);
            if (li >= 0 && !lib_imported[li])
                fatal("line %d: library '%s' not imported (use 'include %s')", n->line, n->data.funcall.lib, n->data.funcall.lib);
#ifdef HAVE_GTK
            if (!strcmp(n->data.funcall.lib, "gtk"))
                return gtk_dispatch(n->data.funcall.args[0], n->data.funcall.argc, n->data.funcall.args, n->line);
#endif
            if (!strcmp(n->data.funcall.lib, "gtk"))
                fatal("line %d: gtk not supported (recompile with 'make gtk')", n->line);
            return lib_dispatch(n->data.funcall.lib, n->data.funcall.args[0], n->data.funcall.argc, n->data.funcall.args, n->line);
        }
        default:
            fatal("line %d: expression expected", n->line);
            return make_num(0);
    }
}

Value stmt_val(ASTNode *n) {
    if (!n) return make_num(0);
    switch (n->type) {
        case NODE_NUM: case NODE_STR: case NODE_ID:
        case NODE_BINOP: case NODE_UNARY: case NODE_ASSIGN:
        case NODE_FUNC_CALL: case NODE_TEMPLATE:
            return eval_expr(n);
        case NODE_IF: {
            Value cv = eval_expr(n->data.if_stmt.cond);
            int t = (cv.type == VAL_STR) ? (strlen(cv.data.str) != 0) : (cv.data.num != 0);
            if (t) return stmt_val(n->data.if_stmt.then);
            if (n->data.if_stmt.els) {
                if (n->data.if_stmt.els->type == NODE_IF) return stmt_val(n->data.if_stmt.els);
                return stmt_val(n->data.if_stmt.els);
            }
            return make_num(0);
        }
        case NODE_BLOCK: {
            if (n->data.block.count > 0)
                return stmt_val(n->data.block.stmts[n->data.block.count - 1]);
            return make_num(0);
        }
        default: return make_num(0);
    }
}

int exec_stmt(ASTNode *n) {
    if (!n || n->type == NODE_EMPTY) return 0;
    switch (n->type) {
        case NODE_ASSIGN: {
            Value v = eval_expr(n->data.assign.value);
            _last_expr_val = v;
            int idx = var_find(n->data.assign.name);
            if (idx >= 0 && vars[idx].forced) {
                if (vars[idx].forced_type == FORCE_NONE) fatal("line %d: cannot assign to a forced variable", n->line);
                if (vars[idx].forced_type == FORCE_INT && v.type == VAL_STR) fatal("line %d: cannot assign string to int-forced variable", n->line);
                if (vars[idx].forced_type == FORCE_STR && v.type == VAL_NUM) fatal("line %d: cannot assign number to str-forced variable", n->line);
            }
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
            while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = 0;
            if (n->data.input.force_type == FORCE_INT) {
                char *end;
                double d = strtod(buf, &end);
                while (*end == ' ' || *end == '\t') end++;
                if (*end != 0) fatal("line %d: input '%s' is not a valid number", n->line, buf);
                var_set(n->data.input.name, make_num(d));
                int _fi = var_find(n->data.input.name);
                if (_fi >= 0) vars[_fi].forced = 1;
            } else if (n->data.input.force_type == FORCE_STR) {
                var_set(n->data.input.name, make_str(buf));
                int _fi = var_find(n->data.input.name);
                if (_fi >= 0) vars[_fi].forced = 1;
            } else if (n->data.input.force_type == FORCE_INPUT_INT) {
                char *end;
                double d = strtod(buf, &end);
                while (*end == ' ' || *end == '\t') end++;
                if (*end != 0) fatal("line %d: input '%s' is not a valid number", n->line, buf);
                var_set(n->data.input.name, make_num(d));
            } else if (n->data.input.force_type == FORCE_INPUT_STR) {
                var_set(n->data.input.name, make_str(buf));
            } else {
                char *end;
                double d = strtod(buf, &end);
                while (*end == ' ' || *end == '\t') end++;
                if (*end == 0) {
                    var_set(n->data.input.name, make_num(d));
                } else {
                    var_set(n->data.input.name, make_str(buf));
                }
            }
            return 0;
        }
        case NODE_STRINGIFY: {
            int idx = var_find(n->data.modify.name);
            if (idx < 0) { var_set(n->data.modify.name, make_str("")); idx = var_count - 1; }
            if (vars[idx].forced) fatal("line %d: cannot stringify a forced variable", n->line);
            char *s = val_tostr(vars[idx].val);
            var_set(n->data.modify.name, make_str(s));
            free(s);
            return 0;
        }
        case NODE_INTIFY: {
            int _fidx = var_find(n->data.modify.name);
            if (_fidx >= 0 && vars[_fidx].forced) fatal("line %d: cannot intify a forced variable", n->line);
            if (n->data.modify.fmt) {
                int idx = var_find(n->data.modify.name);
                if (idx < 0) { var_set(n->data.modify.name, make_str("")); idx = var_count - 1; }
                char *s = val_tostr(vars[idx].val);
                size_t slen = strlen(s);
                char *out = malloc(slen * 16 + 1);
                if (!out) fatal("out of memory");
                out[0] = 0;
                for (size_t i = 0; i < slen; i++) {
                    if (i > 0) strcat(out, " ");
                    if (!strcmp(n->data.modify.fmt, "binary")) {
                        for (int b = 7; b >= 0; b--) strcat(out, ((unsigned char)s[i] >> b) & 1 ? "1" : "0");
                    } else if (!strcmp(n->data.modify.fmt, "hex")) {
                        char buf[16]; snprintf(buf, sizeof(buf), "%02x", (unsigned char)s[i]);
                        strcat(out, buf);
                    } else {
                        char buf[16]; snprintf(buf, sizeof(buf), "%03o", (unsigned char)s[i]);
                        strcat(out, buf);
                    }
                }
                free(s);
                var_set(n->data.modify.name, make_str(out));
                free(out);
            } else {
                Value v = var_get(n->data.modify.name);
                double d = val_tonum(v);
                var_set(n->data.modify.name, make_num(d));
            }
            return 0;
        }
        case NODE_TOGGLE: {
            int idx = var_find(n->data.modify.name);
            if (idx < 0) { var_set(n->data.modify.name, make_str("")); idx = var_count - 1; }
            if (vars[idx].forced) fatal("line %d: cannot toggle a forced variable", n->line);
            if (vars[idx].val.type == VAL_STR) {
                double d = val_tonum(vars[idx].val);
                var_set(n->data.modify.name, make_num(d));
            } else {
                char *s = val_tostr(vars[idx].val);
                var_set(n->data.modify.name, make_str(s));
                free(s);
            }
            return 0;
        }
        case NODE_TRY: {
            jmp_buf old;
            memcpy(old, error_jmp, sizeof(jmp_buf));
            in_try = 1;
            if (setjmp(error_jmp) == 0) {
                exec_stmt(n->data.try_stmt.body);
                memcpy(error_jmp, old, sizeof(jmp_buf));
                in_try = 0;
            } else {
                error_occurred = 0;
                memcpy(error_jmp, old, sizeof(jmp_buf));
                in_try = 0;
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
                if (r == 3) continue;
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
                if (r == 3) continue;
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
                if (r == 3) continue;
            }
            return 0;
        }
        case NODE_STOP:
        case NODE_BREAK:
            return 2;
        case NODE_CONTINUE:
            return 3;
        case NODE_GET: {
            LilState saved;
            state_save(&saved);

            var_count = 0;
            func_count = 0;
            assign_hist_count = 0;
            run_file_interpreted(n->data.get_stmt.path);

            memcpy(error_jmp, saved.error_jmp, sizeof(jmp_buf));
            Value *ex = malloc(sizeof(Value) * n->data.get_stmt.nvars);
            for (int i = 0; i < n->data.get_stmt.nvars; i++) {
                char *vn = n->data.get_stmt.varnames[i];
                int ix = n->data.get_stmt.indices[i];
                if (ix > 0) {
                    int vi = var_find(vn);
                    if (vi < 0) ex[i] = copy_val(undef_val);
                    else ex[i] = copy_val(var_get_history(vi, ix));
                } else {
                    ex[i] = copy_val(var_get(vn));
                }
            }

            memcpy(vars, saved.vars, sizeof(Var) * saved.var_count);
            var_count = saved.var_count;
            func_count = saved.func_count;
            memcpy(funcs, saved.funcs, sizeof(FuncDef) * saved.func_count);
            assign_hist_count = saved.assign_hist_count;
            memcpy(lib_imported, saved.lib_imported, sizeof(lib_imported));

            for (int i = 0; i < n->data.get_stmt.nvars; i++) {
                char *nn = n->data.get_stmt.newnames[i] ? n->data.get_stmt.newnames[i] : n->data.get_stmt.varnames[i];
                var_set(nn, ex[i]);
            }
            free(ex);
            return 0;
        }
        case NODE_INCLUDE: {
            if (n->data.include.funcs && n->data.include.nfuncs > 0) {
                LilState saved;
                state_save(&saved);
                int outer_fc = func_count;

                var_count = 0;
                assign_hist_count = 0;
                run_file_interpreted(n->data.include.path);

                int nfc = func_count;
                int wi = outer_fc;
                for (int i = outer_fc; i < nfc; i++) {
                    int match = 0;
                    for (int j = 0; j < n->data.include.nfuncs; j++) {
                        if (!strcmp(funcs[i].name, n->data.include.funcs[j])) {
                            match = 1;
                            break;
                        }
                    }
                    if (match) {
                        if (wi != i) funcs[wi] = funcs[i];
                        wi++;
                    } else {
                        free(funcs[i].name);
                    }
                }
                func_count = wi;

                memcpy(error_jmp, saved.error_jmp, sizeof(jmp_buf));
                memcpy(vars, saved.vars, sizeof(Var) * saved.var_count);
                var_count = saved.var_count;
                assign_hist_count = saved.assign_hist_count;
                memcpy(lib_imported, saved.lib_imported, sizeof(lib_imported));
            }
            return 0;
        }
        case NODE_FUNC_DEF: {
            for (int i = 0; i < func_count; i++) {
                if (!strcmp(funcs[i].name, n->data.func_def.name)) {
                    fatal("line %d: function '%s' already defined", n->line, n->data.func_def.name);
                }
            }
            if (func_count >= MAX_FUNCS) fatal("line %d: too many function definitions", n->line);
            funcs[func_count].name = sdup(n->data.func_def.name);
            funcs[func_count].params = n->data.func_def.params;
            funcs[func_count].nparams = n->data.func_def.nparams;
            funcs[func_count].body = n->data.func_def.body;
            func_count++;
            return 0;
        }
        case NODE_FORCE: {
            int idx = var_ensure(n->data.force.name);
            if (n->data.force.value) {
                Value v = eval_expr(n->data.force.value);
                if (n->data.force.force_type == FORCE_NONE)
                    n->data.force.force_type = (v.type == VAL_STR) ? FORCE_STR : FORCE_INT;
                var_set(n->data.force.name, v);
                idx = var_find(n->data.force.name);
            }
            vars[idx].forced = 1;
            vars[idx].forced_type = n->data.force.force_type;
            return 0;
        }
        case NODE_UNFORCE: {
            int idx = var_ensure(n->data.force.name);
            vars[idx].forced = 0;
            vars[idx].forced_type = 0;
            if (n->data.force.value) {
                Value v = eval_expr(n->data.force.value);
                var_set(n->data.force.name, v);
            }
            return 0;
        }
        case NODE_INDEX_SET: {
            Value cval = eval_expr(n->data.idx_set.container);
            Value ival = eval_expr(n->data.idx_set.index);
            Value vval = eval_expr(n->data.idx_set.value);
            double di = val_tonum(ival);
            val_free(ival);
            if (cval.type != VAL_LIST) { val_free(cval); val_free(vval); fatal("line %d: cannot index non-list", n->line); }
            int iidx = (int)di;
            if (iidx < 0) { val_free(cval); val_free(vval); fatal("line %d: negative list index", n->line); }
            int found = var_find(n->data.idx_set.container->data.id);
            if (found >= 0 && vars[found].forced) fatal("line %d: cannot modify forced list", n->line);
            list_set(&cval, iidx, vval);
            if (found >= 0) var_set(n->data.idx_set.container->data.id, cval);
            else val_free(cval);
            return 0;
        }
        case NODE_EXIT:
            return 1;
        case NODE_SET_UNDEF: {
            Value v = eval_expr(n->data.assign.value);
            if (undef_val.type == VAL_STR) free(undef_val.data.str);
            undef_val = v;
            return 0;
        }
        default:
            _last_expr_val = eval_expr(n);
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
#define FIXUP_INIT 1024

Instr *code;
int code_len, code_cap;
Value *consts;
int const_len, const_cap;
char **strtab;
ASTNode **fallbacks;
int fb_len, fb_cap;
int cur_loop;
int *fixups;
int *fixup_lv;
int fixup_cap, fixup_len;
int *cont_fixups;
int *cont_lv;
int cont_cap, cont_len;
Value vm_stack[VM_STACK];
int sp;
int vm_exit;
int for_counter;

int add_const(Value v) {
    if (const_len >= const_cap) {
        const_cap = const_cap ? const_cap * 2 : CONST_INIT;
        consts = realloc(consts, const_cap * sizeof(Value));
    }
    consts[const_len] = v;
    return const_len++;
}

int add_fallback(ASTNode *n) {
    if (fb_len >= fb_cap) {
        fb_cap = fb_cap ? fb_cap * 2 : 128;
        fallbacks = realloc(fallbacks, fb_cap * sizeof(ASTNode*));
    }
    fallbacks[fb_len] = n;
    return fb_len++;
}

void emit(uint16_t op, int arg) {
    if (code_len >= code_cap) {
        code_cap = code_cap ? code_cap * 2 : CODE_INIT;
        code = realloc(code, code_cap * sizeof(Instr));
    }
    code[code_len].op = op;
    code[code_len].arg = arg;
    code_len++;
}

void patch(int idx, int target) {
    code[idx].arg = target;
}

void add_fixup(int addr) {
    if (fixup_len >= fixup_cap) {
        fixup_cap = fixup_cap ? fixup_cap * 2 : FIXUP_INIT;
        fixups = realloc(fixups, fixup_cap * sizeof(int));
        fixup_lv = realloc(fixup_lv, fixup_cap * sizeof(int));
    }
    fixups[fixup_len] = addr;
    fixup_lv[fixup_len] = cur_loop;
    fixup_len++;
}

void patch_fixups(int target) {
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

void add_cont_fixup(int addr) {
    if (cont_len >= cont_cap) {
        cont_cap = cont_cap ? cont_cap * 2 : 16;
        cont_fixups = realloc(cont_fixups, cont_cap * sizeof(int));
        cont_lv = realloc(cont_lv, cont_cap * sizeof(int));
    }
    cont_fixups[cont_len] = addr;
    cont_lv[cont_len] = cur_loop;
    cont_len++;
}

void patch_cont_fixups(int target) {
    for (int i = 0; i < cont_len; i++) {
        if (cont_lv[i] == cur_loop)
            patch(cont_fixups[i], target);
    }
    int w = 0;
    for (int i = 0; i < cont_len; i++) {
        if (cont_lv[i] != cur_loop) {
            cont_fixups[w] = cont_fixups[i];
            cont_lv[w] = cont_lv[i];
            w++;
        }
    }
    cont_len = w;
}

int ce_expr(ASTNode *n) {
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

int is_cstmt(ASTNode *n) {
    if (!n) return 1;
    switch (n->type) {
        case NODE_EMPTY: case NODE_EXIT: case NODE_STOP: case NODE_BREAK: case NODE_CONTINUE: return 1;
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

void ce_stmt(ASTNode *n) {
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
            if (!ce_expr(n->data.assign.value)) { emit(OP_FALLBACK, add_fallback(n)); break; }
            emit(OP_VAR_SET_IDX, vidx);
            break;
        }
        case NODE_PRINT: {
            int ok = 1;
            for (int i = 0; i < n->data.print.count; i++)
                if (!ce_expr(n->data.print.exprs[i])) ok = 0;
            if (!ok) { emit(OP_FALLBACK, add_fallback(n)); break; }
            emit(OP_PRINT, n->data.print.count);
            break;
        }
        case NODE_IF: {
            if (n->data.if_stmt.has_mode || n->data.if_stmt.flags || !ce_expr(n->data.if_stmt.cond)) {
                emit(OP_FALLBACK, add_fallback(n)); break;
            }
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
            if (!is_cstmt(n) || !ce_expr(n->data.while_stmt.cond)) { emit(OP_FALLBACK, add_fallback(n)); break; }
            int ls = code_len;
            ce_expr(n->data.while_stmt.cond);
            emit(OP_JZ, 0);
            int pe = code_len - 1;
            cur_loop++; ce_stmt(n->data.while_stmt.body);
            patch_cont_fixups(ls);
            emit(OP_JMP, ls);
            patch(pe, code_len);
            patch_fixups(code_len);
            cur_loop--;
            break;
        }
        case NODE_FORTO: {
            if (!is_cstmt(n) || !ce_expr(n->data.forto.start) || !ce_expr(n->data.forto.end)) {
                emit(OP_FALLBACK, add_fallback(n)); break;
            }
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
            int cont = code_len;
            emit(OP_VAR_GET_IDX, vidx);
            emit(OP_CONST, add_const(make_num(1)));
            emit(OP_ADD, 0);
            emit(OP_VAR_SET_IDX, vidx);
            patch_cont_fixups(cont);
            emit(OP_JMP, ls);
            patch(pe, code_len);
            patch_fixups(code_len);
            cur_loop--;
            break;
        }
        case NODE_LOOP: {
            if (!is_cstmt(n)) { emit(OP_FALLBACK, add_fallback(n)); break; }
            int ls = code_len;
            cur_loop++; ce_stmt(n->data.loop.body); patch_cont_fixups(ls); emit(OP_JMP, ls); patch_fixups(code_len); cur_loop--;
            break;
        }
        case NODE_STOP:
        case NODE_BREAK:
            if (cur_loop == 0) fatal("line %d: stop/break outside a loop", n->line);
            add_fixup(code_len);
            emit(OP_JMP, 0);
            break;
        case NODE_CONTINUE:
            if (cur_loop == 0) fatal("line %d: continue outside a loop", n->line);
            add_cont_fixup(code_len);
            emit(OP_JMP, 0);
            break;
        case NODE_EXIT:
            emit(OP_EXIT, 0);
            break;
        case NODE_SET_UNDEF: {
            Value v = eval_expr(n->data.assign.value);
            if (undef_val.type == VAL_STR) free(undef_val.data.str);
            undef_val = v;
            break;
        }
        case NODE_BLOCK:
            for (int i = 0; i < n->data.block.count; i++)
                ce_stmt(n->data.block.stmts[i]);
            break;
        default:
            emit(OP_FALLBACK, add_fallback(n));
            break;
    }
}

void compile_prog(ASTNode **stmts, int nstmts) {
    for (int i = 0; i < nstmts; i++)
        ce_stmt(stmts[i]);
    emit(OP_HALT, 0);
}

void vm_run(void) {
    int ip = 0;
    sp = 0;
    vm_exit = 0;
    void *dtab[] = { &&OP_NOP, &&OP_CONST, &&OP_VAR_GET, &&OP_VAR_SET,
        &&OP_VAR_GET_IDX, &&OP_VAR_SET_IDX, &&OP_INC_IDX, &&OP_DEC_IDX,
        &&OP_ADD, &&OP_SUB, &&OP_MUL, &&OP_DIV, &&OP_MOD, &&OP_NEG,
        &&OP_EQ, &&OP_NE, &&OP_LT, &&OP_GT, &&OP_LE, &&OP_GE,
        &&OP_AND, &&OP_OR, &&OP_NOT,
        &&OP_PRINT, &&OP_JMP, &&OP_JZ, &&OP_HALT, &&OP_EXIT, &&OP_FALLBACK, &&OP_POP };
    goto *dtab[code[ip].op];

OP_NOP: ip++; goto *dtab[code[ip].op];
OP_CONST: {
    Value v = consts[code[ip].arg];
    if (v.type == VAL_STR) v.data.str = sdup(v.data.str);
    else if (v.type == VAL_LIST) v = copy_val(v);
    vm_stack[sp++] = v; ip++; goto *dtab[code[ip].op];
}
OP_VAR_GET: {
    Value v = var_get(strtab[code[ip].arg]);
    vm_stack[sp++] = v; ip++; goto *dtab[code[ip].op];
}
OP_VAR_SET: {
    char *vname = strtab[code[ip].arg];
    Value sv = vm_stack[--sp];
    int si = var_find(vname);
    if (si >= 0 && vars[si].forced) fatal("cannot assign to a forced variable");
    var_set(vname, sv); ip++; goto *dtab[code[ip].op];
}
OP_VAR_GET_IDX: {
    Value v = vars[code[ip].arg].val;
    if (v.type == VAL_STR) v.data.str = sdup(v.data.str);
    else if (v.type == VAL_LIST) v = copy_val(v);
    vm_stack[sp++] = v; ip++; goto *dtab[code[ip].op];
}
OP_VAR_SET_IDX: {
    int idx = code[ip].arg;
    Value v = vm_stack[--sp];
    if (vars[idx].forced) {
        if (vars[idx].forced_type == FORCE_NONE) fatal("cannot assign to a forced variable");
        if (vars[idx].forced_type == FORCE_INT && v.type == VAL_STR) fatal("cannot assign string to int-forced variable");
        if (vars[idx].forced_type == FORCE_STR && v.type == VAL_NUM) fatal("cannot assign number to str-forced variable");
    }
    if (vars[idx].val.type == VAL_STR) free(vars[idx].val.data.str);
    vars[idx].val = v;
    ip++; goto *dtab[code[ip].op];
}
OP_INC_IDX: {
    int idx = code[ip].arg;
    if (vars[idx].forced && vars[idx].forced_type != FORCE_INT) fatal("cannot assign to a forced variable");
    if (vars[idx].val.type == VAL_STR) free(vars[idx].val.data.str);
    vars[idx].val.data.num += 1;
    vars[idx].val.type = VAL_NUM;
    ip++; goto *dtab[code[ip].op];
}
OP_DEC_IDX: {
    int idx = code[ip].arg;
    if (vars[idx].forced && vars[idx].forced_type != FORCE_INT) fatal("cannot assign to a forced variable");
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
OP_EQ: {
    Value b = vm_stack[--sp], a = vm_stack[--sp];
    vm_stack[sp++] = make_num(val_cmp(a, b, 0));
    ip++; goto *dtab[code[ip].op];
}
OP_NE: {
    Value b = vm_stack[--sp], a = vm_stack[--sp];
    vm_stack[sp++] = make_num(val_cmp(a, b, 1));
    ip++; goto *dtab[code[ip].op];
}
OP_LT: {
    Value b = vm_stack[--sp], a = vm_stack[--sp];
    vm_stack[sp++] = make_num(val_cmp(a, b, 2));
    ip++; goto *dtab[code[ip].op];
}
OP_GT: {
    Value b = vm_stack[--sp], a = vm_stack[--sp];
    vm_stack[sp++] = make_num(val_cmp(a, b, 3));
    ip++; goto *dtab[code[ip].op];
}
OP_LE: {
    Value b = vm_stack[--sp], a = vm_stack[--sp];
    vm_stack[sp++] = make_num(val_cmp(a, b, 4));
    ip++; goto *dtab[code[ip].op];
}
OP_GE: {
    Value b = vm_stack[--sp], a = vm_stack[--sp];
    vm_stack[sp++] = make_num(val_cmp(a, b, 5));
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
        else if (vals[i].type == VAL_LIST) { char *s = val_tostr(vals[i]); printf("%s", s); free(s); val_free(vals[i]); }
        else printf("%g", vals[i].data.num);
    }
    free(vals); printf("\n"); fflush(stdout);
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
