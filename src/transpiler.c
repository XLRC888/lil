#include "lil.h"

int cg_loop_id;
VarType var_types[MAX_VARS];

char *cescape(const char *s) {
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

VarType infer_expr_type(ASTNode *n) {
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

void infer_type_stmt(ASTNode *n) {
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
        case NODE_INTIFY:
        case NODE_SWIFY: {
            int i = var_ensure(n->data.input.name);
            var_types[i] = TY_DYN;
            break;
        }
        case NODE_TRY:
            infer_type_stmt(n->data.try_stmt.body);
            infer_type_stmt(n->data.try_stmt.catch_body);
            break;
        case NODE_FORCE:
        case NODE_UNFORCE:
            var_ensure(n->data.assign.name);
            if (n->data.assign.value) infer_type_stmt(n->data.assign.value);
            break;
        default: break;
    }
}

void cg_collect_vars(ASTNode *n);
void cg_varinit(FILE *f, ASTNode *prog) {
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
            if (undef_val.type == VAL_STR)
                fprintf(f, "  Value %s = make_str(\"%s\");\n", vars[i].name, undef_val.data.str);
            else
                fprintf(f, "  Value %s = {VAL_NUM,{.num=%.17g}};\n", vars[i].name, undef_val.data.num);
            fprintf(f, "  var_ensure(\"%s\");\n", vars[i].name);
        }
    }
}

void cg_collect_vars(ASTNode *n) {
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
        case NODE_INTIFY:
        case NODE_SWIFY: var_ensure(n->data.input.name); break;
        case NODE_TRY: cg_collect_vars(n->data.try_stmt.body); cg_collect_vars(n->data.try_stmt.catch_body); break;
        case NODE_FORCE:
        case NODE_UNFORCE: var_ensure(n->data.assign.name); if (n->data.assign.value) cg_collect_vars(n->data.assign.value); break;
        case NODE_SET_UNDEF: cg_collect_vars(n->data.assign.value); break;
        case NODE_ID: var_ensure(n->data.id); break;
        case NODE_BINOP: cg_collect_vars(n->data.binop.left); cg_collect_vars(n->data.binop.right); break;
        case NODE_UNARY: cg_collect_vars(n->data.unary.operand); break;
        case NODE_FUNC_CALL: for (int i = 0; i < n->data.func_call.nargs; i++) cg_collect_vars(n->data.func_call.args[i]); break;
        case NODE_TEMPLATE: break;
        default: break;
    }
}

void cg_expr(FILE *f, ASTNode *n, VarType want) {
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
                    if (want == TY_NUM) fprintf(f, "val_cmp(");
                    else fprintf(f, "make_num(val_cmp(");
                    cg_expr(f, n->data.binop.left, TY_DYN);
                    fprintf(f, ",");
                    cg_expr(f, n->data.binop.right, TY_DYN);
                    if (op == TOK_EQ) fprintf(f, ",0)%s", want == TY_NUM ? "" : ")");
                    else if (op == TOK_NE) fprintf(f, ",1)%s", want == TY_NUM ? "" : ")");
                    else if (op == TOK_LT) fprintf(f, ",2)%s", want == TY_NUM ? "" : ")");
                    else if (op == TOK_GT) fprintf(f, ",3)%s", want == TY_NUM ? "" : ")");
                    else if (op == TOK_LE) fprintf(f, ",4)%s", want == TY_NUM ? "" : ")");
                    else fprintf(f, ",5)%s", want == TY_NUM ? "" : ")");
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
            fprintf(f, "_lil_fn_%s(", n->data.func_call.name);
            for (int i = 0; i < n->data.func_call.nargs; i++) {
                if (i > 0) fprintf(f, ", ");
                cg_expr(f, n->data.func_call.args[i], TY_DYN);
            }
            fprintf(f, ")");
            break;
        }
        case NODE_FUNCTION: {
            char *lib = n->data.funcall.lib;
            char *fn = n->data.funcall.args[0];
            int li = lib_idx(lib);
            if (li >= 0 && !lib_imported[li])
                fatal("line %d: library '%s' not imported (use 'include %s')", n->line, lib, lib);
            if (!strcmp(lib, "date")) {
                if (!strcmp(fn, "minimal"))
                    fprintf(f, "({time_t t=time(NULL);struct tm*tm=localtime(&t);char b[64];strftime(b,64,\"%%-d.%%-m.%%y\",tm);make_str(strdup(b));})");
                else if (!strcmp(fn, "standart"))
                    fprintf(f, "({time_t t=time(NULL);struct tm*tm=localtime(&t);char b[64];strftime(b,64,\"%%d.%%m.%%Y\",tm);make_str(strdup(b));})");
                else if (!strcmp(fn, "standartplus"))
                    fprintf(f, "({time_t t=time(NULL);struct tm*tm=localtime(&t);char b[64];strftime(b,64,\"%%d.%%m.%%Y %%A\",tm);make_str(strdup(b));})");
                else if (!strcmp(fn, "full"))
                    fprintf(f, "({time_t t=time(NULL);struct tm*tm=localtime(&t);char b[64];strftime(b,64,\"%%d.%%m.%%Y %%A %%H:%%M:%%S\",tm);make_str(strdup(b));})");
                else if (!strcmp(fn, "fullplus"))
                    fprintf(f, "({time_t t=time(NULL);struct tm*tm=localtime(&t);struct timeval tv;gettimeofday(&tv,0);char b[64];strftime(b,64,\"%%d.%%m.%%Y %%A %%H:%%M:%%S\",tm);char r[80];snprintf(r,80,\"%%s.%%03ld\",b,(long)tv.tv_usec/1000);make_str(r);})");
                else if (!strcmp(fn, "format"))
                    fprintf(f, "make_str(\"\")");
                else
                    fprintf(f, want == TY_NUM ? "0" : "make_num(0)");
            } else if (!strcmp(lib, "math")) {
                if (!strcmp(fn, "random")) { fprintf(f, "make_num((double)rand()/RAND_MAX)"); }
                else if (!strcmp(fn, "randint")) { fprintf(f, "({int lo=(int)(%s),hi=(int)(%s);if(lo>hi){int t=lo;lo=hi;hi=t;}make_num((double)(lo+rand()%%(hi-lo+1)));})", n->data.funcall.args[1], n->data.funcall.args[2]); }
                else if (!strcmp(fn, "choice")) {
                    fprintf(f, "({int _n=");
                    fprintf(f, "%d", n->data.funcall.argc - 1);
                    fprintf(f, ";int _i=rand()%%_n+1;");
                    fprintf(f, "make_str(strdup((char*[]){");
                    for (int i = 1; i < n->data.funcall.argc; i++) {
                        if (i > 1) fprintf(f, ",");
                        fprintf(f, "\"%s\"", n->data.funcall.args[i]);
                    }
                    fprintf(f, "}[_i-1]));})");
                }
                else if (!strcmp(fn, "sleep")) { fprintf(f, "({double _s=%s;struct timespec ts;ts.tv_sec=(time_t)_s;ts.tv_nsec=(long)((_s-ts.tv_sec)*1e9);nanosleep(&ts,0);make_num(0);})", n->data.funcall.args[1]); }
                else if (!strcmp(fn, "hasops")) { fprintf(f, "make_num(0)"); }
                else if (!strcmp(fn, "eval")) { fprintf(f, "make_num(0)"); }
                else if (!strcmp(fn, "factors")) { fprintf(f, "make_str(strdup(\"\"))"); }
                else if (!strcmp(fn, "fib")) { fprintf(f, "make_str(strdup(\"\"))"); }
                else fprintf(f, want == TY_NUM ? "0" : "make_num(0)");
            } else
                fprintf(f, want == TY_NUM ? "0" : "make_num(0)");
            break;
        }
        default: fprintf(f, want == TY_NUM ? "0" : "make_num(0)"); break;
    }
}

void cg_stmt(FILE *f, ASTNode *n, int *loop_ids, int loop_depth) {
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
        case NODE_CONTINUE: {
            if (loop_depth > 0) {
                fprintf(f, "continue;\n");
            } else {
                fatal("line %d: continue outside a loop", n->line);
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
        case NODE_SWIFY: {
            fprintf(f, "{\n  if (%s.type == VAL_STR) {\n", n->data.input.name);
            fprintf(f, "    { char *_e = %s.data.str; char *_end; double _d = strtod(_e,&_end); if (*_end) { fprintf(stderr,\"convert error\\n\"); longjmp(_try_jmp,1); } val_free(%s); %s = make_num(_d); }\n", n->data.input.name, n->data.input.name, n->data.input.name);
            fprintf(f, "  } else {\n");
            fprintf(f, "    char _b[128];\n    snprintf(_b,sizeof(_b),\"%%g\",%s.data.num);\n", n->data.input.name);
            fprintf(f, "    val_free(%s); %s = make_str(_b);\n  }\n}\n", n->data.input.name, n->data.input.name);
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
        case NODE_FUNC_DEF: break;
        case NODE_FORCE:
        case NODE_UNFORCE: {
            if (n->data.assign.value) {
                int idx = var_find(n->data.assign.name);
                VarType vt = idx >= 0 ? var_types[idx] : TY_DYN;
                if (vt == TY_NUM) {
                    fprintf(f, "%s = ", n->data.assign.name);
                    cg_expr(f, n->data.assign.value, TY_NUM);
                    fprintf(f, ";\n");
                } else {
                    fprintf(f, "val_free(%s); %s = ", n->data.assign.name, n->data.assign.name);
                    cg_expr(f, n->data.assign.value, TY_DYN);
                    fprintf(f, ";\n");
                }
            }
            break;
        }
    }
}

void cg_emit_func(FILE *f, ASTNode *n) {
    int np = n->data.func_def.nparams;
    int sc = var_count;
    VarType st[MAX_VARS];
    memcpy(st, var_types, sizeof(VarType) * (var_count < MAX_VARS ? var_count : MAX_VARS));
    var_count = 0;
    for (int i = 0; i < np; i++) var_ensure(n->data.func_def.params[i]);
    cg_collect_vars(n->data.func_def.body);
    infer_type_stmt(n->data.func_def.body);
    fprintf(f, "static Value _lil_fn_%s(", n->data.func_def.name);
    for (int i = 0; i < np; i++) { if (i > 0) fprintf(f, ", "); fprintf(f, "Value p%d", i); }
    fprintf(f, ") {\n");
    for (int i = 0; i < var_count; i++) {
        VarType t = var_types[i];
        if (t == TY_NUM) fprintf(f, "  double %s = 0;\n", vars[i].name);
        else if (t == TY_STR) fprintf(f, "  Value %s = {VAL_STR,{.str=NULL}};\n", vars[i].name);
        else fprintf(f, "  Value %s = {VAL_NUM,{.num=0}};\n", vars[i].name);
    }
    for (int i = 0; i < np; i++) fprintf(f, "  %s = p%d;\n", n->data.func_def.params[i], i);
    fprintf(f, "  Value _ret = make_num(0);\n");
    cg_stmt(f, n->data.func_def.body, NULL, 0);
    if (n->data.func_def.body->type == NODE_BLOCK && n->data.func_def.body->data.block.count > 0) {
        ASTNode *last = n->data.func_def.body->data.block.stmts[n->data.func_def.body->data.block.count - 1];
        if (last) {
            if (last->type == NODE_ASSIGN) {
                int idx = var_find(last->data.assign.name);
                VarType vt = idx >= 0 ? var_types[idx] : TY_DYN;
                if (vt == TY_NUM) fprintf(f, "  _ret = make_num(%s);\n", last->data.assign.name);
                else fprintf(f, "  _ret = copy_val(%s);\n", last->data.assign.name);
            } else {
                fprintf(f, "  _ret = ");
                cg_expr(f, last, TY_DYN);
                fprintf(f, ";\n");
            }
        }
    }
    fprintf(f, "  return _ret;\n}\n\n");
    var_count = sc;
    memcpy(var_types, st, sizeof(VarType) * (sc < MAX_VARS ? sc : MAX_VARS));
}

int generate_c(const char *path, const char *outpath) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "error: cannot open '%s': %s\n", path, strerror(errno)); return 1; }
    memset(lib_imported, 0, sizeof(lib_imported));
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

    if (setjmp(error_jmp)) { free(src); src = NULL; return 1; }

    lex_init(psrc);
    lex_next();
    ASTNode *prog = parse_program();

    char cpath[1024];
    snprintf(cpath, sizeof(cpath), "%s.c", outpath);
    f = fopen(cpath, "w");
    if (!f) { fprintf(stderr, "error: cannot write '%s'\n", cpath); free(src); return 1; }

    fprintf(f, "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <stdint.h>\n#include <math.h>\n#include <setjmp.h>\n#include <time.h>\n#include <sys/time.h>\n#include <unistd.h>\n\n");
    fprintf(f, "typedef enum { VAL_NUM, VAL_STR } ValType;\n");
    fprintf(f, "typedef struct { ValType type; union { double num; char *str; } data; } Value;\n");
    fprintf(f, "static jmp_buf _try_jmp;\n");
    fprintf(f, "Value make_num(double n) { Value v = {VAL_NUM,{.num=n}}; return v; }\n");
    fprintf(f, "Value make_str(const char *s) { Value v = {VAL_STR,{.str=strdup(s)}}; return v; }\n");
    fprintf(f, "void val_free(Value v) { if (v.type == VAL_STR) free(v.data.str); }\n");
    fprintf(f, "Value copy_val(Value v) { if (v.type==VAL_STR) v.data.str=strdup(v.data.str); return v; }\n");
    fprintf(f, "int truthy(Value v) { if (v.type == VAL_STR) return strlen(v.data.str)!=0; return v.data.num!=0; }\n");
    fprintf(f, "char *val_tostr(Value v) {\n");
    fprintf(f, "  if (v.type == VAL_STR) return strdup(v.data.str);\n");
    fprintf(f, "  char buf[128]; double d = v.data.num;\n");
    fprintf(f, "  if (d==(long)d) snprintf(buf,sizeof(buf),\"%%ld\",(long)d);\n");
    fprintf(f, "  else snprintf(buf,sizeof(buf),\"%%.10g\",d);\n");
    fprintf(f, "  return strdup(buf);\n");
    fprintf(f, "}\n");
    fprintf(f, "double val_tonum(Value v) {\n");
    fprintf(f, "  if (v.type == VAL_NUM) return v.data.num;\n");
    fprintf(f, "  char *end; double d = strtod(v.data.str,&end);\n");
    fprintf(f, "  if (*end) { fprintf(stderr,\"convert error\\n\"); longjmp(_try_jmp,1); }\n");
    fprintf(f, "  return d;\n");
    fprintf(f, "}\n");
    fprintf(f, "Value val_add(Value a, Value b) {\n");
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
    fprintf(f, "Value val_arith(Value a, Value b, int op) {\n");
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
    fprintf(f, "Value val_neg(Value a) {\n");
    fprintf(f, "  if (a.type==VAL_STR) { val_free(a); return make_num(0); }\n");
    fprintf(f, "  double r=-a.data.num; val_free(a); return make_num(r);\n");
    fprintf(f, "}\n");
    fprintf(f, "int val_cmp(Value a, Value b, int op) {\n");
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
    fprintf(f, "typedef struct { char *name; Value val; int forced; } Var;\n");
    fprintf(f, "static Var vars[MAX_VARS];\n");
    fprintf(f, "static int var_count;\n");
    fprintf(f, "static Value undef_val = {VAL_NUM,{.num=0}};\n");
    fprintf(f, "int var_ensure(const char *name) {\n");
    fprintf(f, "  for (int i=0;i<var_count;i++) if(!strcmp(vars[i].name,name)) return i;\n");
    fprintf(f, "  if (var_count>=MAX_VARS) { fprintf(stderr,\"too many vars\\n\"); longjmp(_try_jmp,1); }\n");
    fprintf(f, "  vars[var_count].name=strdup(name);\n");
    fprintf(f, "  vars[var_count].val=undef_val;\n");
    fprintf(f, "  if (undef_val.type==VAL_STR) vars[var_count].val.data.str=strdup(undef_val.data.str);\n");
    fprintf(f, "  vars[var_count].forced=0;\n");
    fprintf(f, "  return var_count++;\n");
    fprintf(f, "}\n\n");

    var_count = 0;
    cg_loop_id = 0;
    for (int i = 0; i < MAX_VARS; i++) var_types[i] = TY_DYN;
    for (int i = 0; i < prog->data.block.count; i++)
        if (prog->data.block.stmts[i]->type == NODE_SET_UNDEF) {
            Value _uv = eval_expr(prog->data.block.stmts[i]->data.assign.value);
            if (undef_val.type == VAL_STR) free(undef_val.data.str);
            undef_val = _uv;
        }
    for (int i = 0; i < prog->data.block.count; i++)
        infer_type_stmt(prog->data.block.stmts[i]);

    for (int i = 0; i < prog->data.block.count; i++)
        if (prog->data.block.stmts[i]->type == NODE_FUNC_DEF)
            cg_emit_func(f, prog->data.block.stmts[i]);

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
