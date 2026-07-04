#define _POSIX_C_SOURCE 199309L
#include "lil.h"
#include <sys/wait.h>

int lib_imported[8];

int lib_idx(const char *name) {
    if (!strcmp(name, "math")) return 0;
    if (!strcmp(name, "date")) return 1;
    if (!strcmp(name, "string")) return 2;
    if (!strcmp(name, "file")) return 3;
    if (!strcmp(name, "sys")) return 4;
    if (!strcmp(name, "gtk")) return 5;
    if (!strcmp(name, "list")) return 6;
    if (!strcmp(name, "dict")) return 7;
    return -1;
}

char *resolve_arg(char *arg) {
    if (arg[0] == '\1') {
        Value v = var_get(arg + 1);
        char *s = val_tostr(v);
        return s;
    }
    return sdup(arg);
}

static int resolve_fn_idx(char *arg, int line) {
    char *name;
    if (arg[0] == '\1') {
        Value v = var_get(arg + 1);
        if (v.type == VAL_STR) {
            name = sdup(v.data.str);
            val_free(v);
        } else {
            name = sdup(arg + 1);
        }
    } else {
        name = sdup(arg);
    }
    for (int i = 0; i < func_count; i++) {
        if (!strcmp(funcs[i].name, name)) { free(name); return i; }
    }
    free(name);
    fatal("line %d: function '%s' not found", line, arg);
    return -1;
}

static double math_parse_expr(const char **p);
static double math_parse_term(const char **p);
static double math_parse_factor(const char **p);

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

Value lib_dispatch(const char *lib, const char *fn, int argc, char **args, int line) {
    int li = lib_idx(lib);
    if (li >= 0 && !lib_imported[li])
        fatal("line %d: library '%s' not imported (use 'include %s')", line, lib, lib);

    if (!strcmp(lib, "date")) {
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        static int date_mode = 0;

        if (!strcmp(fn, "format")) {
            if (argc < 2) {
                return make_str(date_mode ? "US" : "EU");
            }
            char *f = resolve_arg(args[1]);
            char *fl = str_lower(f);
            if (!strcmp(fl, "us")) date_mode = 1;
            else if (!strcmp(fl, "eu")) date_mode = 0;
            else { free(fl); free(f); fatal("line %d: unknown date format '%s'", line, args[1]); }
            free(fl); free(f);
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

        char buf[256];
        if (!strcmp(fn, "minimal")) { strftime(buf, sizeof(buf), fmt_min, tm); return make_str(buf); }
        if (!strcmp(fn, "standart")) { strftime(buf, sizeof(buf), fmt_std, tm); return make_str(buf); }
        if (!strcmp(fn, "standartplus")) { strftime(buf, sizeof(buf), fmt_stdp, tm); return make_str(buf); }
        if (!strcmp(fn, "full")) {
            if (argc > 1 && !strcmp(args[1], "noseconds"))
                strftime(buf, sizeof(buf), fmt_fullns, tm);
            else if (argc > 1 && !strcmp(args[1], "noday"))
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
        fatal("line %d: unknown date function '%s'", line, fn);
    }

    if (!strcmp(lib, "math")) {
        if (!strcmp(fn, "hasops")) {
            if (argc < 2) fatal("line %d: math hasops expects a string", line);
            char *arg = resolve_arg(args[1]);
            int found = 0;
            for (char *p = arg; *p; p++) {
                if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '%') { found = 1; break; }
            }
            free(arg);
            return make_num(found);
        }
        if (!strcmp(fn, "eval")) {
            if (argc < 2) fatal("line %d: math eval expects a string expression", line);
            char *expr_str = resolve_arg(args[1]);
            const char *p = expr_str;
            double result = math_parse_expr(&p);
            while (*p == ' ') p++;
            if (*p) { free(expr_str); fatal("line %d: unexpected '%c' in math expression", line, *p); }
            free(expr_str);
            return make_num(result);
        }
        { static int rng_seeded; if (!rng_seeded) { srand(time(NULL)); rng_seeded = 1; } }
        if (!strcmp(fn, "random")) {
            return make_num((double)rand() / RAND_MAX);
        }
        if (!strcmp(fn, "randint")) {
            if (argc < 3) fatal("line %d: randint expects min and max", line);
            char *r1 = resolve_arg(args[1]);
            char *r2 = resolve_arg(args[2]);
            int lo = (int)strtod(r1, NULL); free(r1);
            int hi = (int)strtod(r2, NULL); free(r2);
            if (lo > hi) { int t = lo; lo = hi; hi = t; }
            return make_num(lo + rand() % (hi - lo + 1));
        }
        if (!strcmp(fn, "choice")) {
            if (argc < 2) fatal("line %d: choice expects at least one option", line);
            int idx = rand() % (argc - 1) + 1;
            return make_str(args[idx]);
        }
        if (!strcmp(fn, "sleep")) {
            if (argc < 2) fatal("line %d: sleep expects seconds", line);
            char *rs = resolve_arg(args[1]);
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
            if (argc < 2) fatal("line %d: factors expects a number", line);
            char *arg = resolve_arg(args[1]);
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
            if (argc < 2) fatal("line %d: fib expects a number", line);
            char *arg = resolve_arg(args[1]);
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
            if (argc < 2) fatal("line %d: isprime expects a number", line);
            char *arg = resolve_arg(args[1]);
            long num = (long)strtod(arg, NULL); free(arg);
            if (num < 2) return make_num(0);
            int prime = 1;
            for (long i = 2; i <= num / i; i++) {
                if (num % i == 0) { prime = 0; break; }
            }
            return make_num(prime);
        }
        fatal("line %d: unknown math function '%s'", line, fn);
    }

    if (!strcmp(lib, "string")) {
        if (!strcmp(fn, "upper")) {
            if (argc < 2) fatal("line %d: string upper expects a string", line);
            char *arg = resolve_arg(args[1]);
            for (char *p = arg; *p; p++) *p = toupper((unsigned char)*p);
            Value v = make_str(arg);
            free(arg);
            return v;
        }
        if (!strcmp(fn, "lower")) {
            if (argc < 2) fatal("line %d: string lower expects a string", line);
            char *arg = resolve_arg(args[1]);
            for (char *p = arg; *p; p++) *p = tolower((unsigned char)*p);
            Value v = make_str(arg);
            free(arg);
            return v;
        }
        if (!strcmp(fn, "reverse")) {
            if (argc < 2) fatal("line %d: string reverse expects a string", line);
            char *arg = resolve_arg(args[1]);
            size_t len = strlen(arg);
            for (size_t i = 0; i < len / 2; i++) {
                char t = arg[i]; arg[i] = arg[len-1-i]; arg[len-1-i] = t;
            }
            Value v = make_str(arg);
            free(arg);
            return v;
        }
        if (!strcmp(fn, "len")) {
            if (argc < 2) fatal("line %d: string len expects a string", line);
            char *arg = resolve_arg(args[1]);
            double l = (double)strlen(arg);
            free(arg);
            return make_num(l);
        }
        if (!strcmp(fn, "substr")) {
            if (argc < 4) fatal("line %d: string substr expects string, start, end", line);
            char *arg = resolve_arg(args[1]);
            char *rs = resolve_arg(args[2]);
            int start = (int)strtod(rs, NULL); free(rs);
            rs = resolve_arg(args[3]);
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
            if (argc < 4) fatal("line %d: string replace expects string, old, new", line);
            char *arg = resolve_arg(args[1]);
            char *old = resolve_arg(args[2]);
            char *new = resolve_arg(args[3]);
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
                    size_t av = blen - ri - 1;
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
            if (argc < 3) fatal("line %d: string repeat expects string and count", line);
            char *arg = resolve_arg(args[1]);
            char *rc = resolve_arg(args[2]);
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
        if (!strcmp(fn, "split")) {
            if (argc < 3) fatal("line %d: string split expects string and delimiter", line);
            char *arg = resolve_arg(args[1]);
            char *delim = resolve_arg(args[2]);
            size_t dlen = strlen(delim);
            Value list = make_list();
            if (dlen == 0) {
                for (size_t i = 0; arg[i]; i++) {
                    char buf[2] = { arg[i], 0 };
                    list_append(&list, make_str(buf));
                }
            } else {
                char *p = arg;
                while (1) {
                    char *found = strstr(p, delim);
                    if (found) {
                        list_append(&list, make_str(sdupn(p, found - p)));
                        p = found + dlen;
                    } else {
                        list_append(&list, make_str(sdup(p)));
                        break;
                    }
                }
            }
            free(arg); free(delim);
            return list;
        }
        if (!strcmp(fn, "join")) {
            if (argc < 3) fatal("line %d: string join expects list and delimiter", line);
            char *vname = args[1];
            if (vname[0] == '\1') vname++;
            int vi = var_find(vname);
            if (vi < 0 || vars[vi].val.type != VAL_LIST) fatal("line %d: '%s' is not a list", line, vname);
            char *delim = resolve_arg(args[2]);
            Value lst = vars[vi].val;
            size_t dlen = strlen(delim);
            size_t total = 1;
            for (int i = 0; i < lst.data.list.count; i++) {
                char *s = val_tostr(lst.data.list.items[i]);
                total += strlen(s);
                free(s);
                if (i > 0) total += dlen;
            }
            char *buf = malloc(total);
            if (!buf) fatal("out of memory");
            buf[0] = 0;
            for (int i = 0; i < lst.data.list.count; i++) {
                char *s = val_tostr(lst.data.list.items[i]);
                if (i > 0) strcat(buf, delim);
                strcat(buf, s);
                free(s);
            }
            free(delim);
            Value rv = make_str(buf);
            free(buf);
            return rv;
        }
        if (!strcmp(fn, "contains")) {
            if (argc < 3) fatal("line %d: string contains expects string and substring", line);
            char *arg = resolve_arg(args[1]);
            char *sub = resolve_arg(args[2]);
            int r = strstr(arg, sub) ? 1 : 0;
            free(arg); free(sub);
            return make_num(r);
        }
        if (!strcmp(fn, "find")) {
            if (argc < 3) fatal("line %d: string find expects string and substring", line);
            char *arg = resolve_arg(args[1]);
            char *sub = resolve_arg(args[2]);
            char *pos = strstr(arg, sub);
            double idx = pos ? (double)(pos - arg) : -1;
            free(arg); free(sub);
            return make_num(idx);
        }
        if (!strcmp(fn, "ord")) {
            if (argc < 2) fatal("line %d: string ord expects a string", line);
            char *arg = resolve_arg(args[1]);
            double code = arg[0] ? (double)(unsigned char)arg[0] : 0;
            free(arg);
            return make_num(code);
        }
        if (!strcmp(fn, "chr")) {
            if (argc < 2) fatal("line %d: string chr expects a number", line);
            char *arg = resolve_arg(args[1]);
            int code = (int)strtod(arg, NULL) & 0xFF;
            free(arg);
            char buf[2] = { (char)code, 0 };
            return make_str(buf);
        }
        if (!strcmp(fn, "isdigit")) {
            if (argc < 2) return make_num(0);
            char *arg = resolve_arg(args[1]);
            if (!arg[0]) { free(arg); return make_num(0); }
            int r = 1;
            for (char *p = arg; *p; p++) { if (!isdigit((unsigned char)*p)) { r = 0; break; } }
            free(arg);
            return make_num(r);
        }
        if (!strcmp(fn, "isalpha")) {
            if (argc < 2) return make_num(0);
            char *arg = resolve_arg(args[1]);
            if (!arg[0]) { free(arg); return make_num(0); }
            int r = 1;
            for (char *p = arg; *p; p++) { if (!isalpha((unsigned char)*p)) { r = 0; break; } }
            free(arg);
            return make_num(r);
        }
        if (!strcmp(fn, "isalnum")) {
            if (argc < 2) return make_num(0);
            char *arg = resolve_arg(args[1]);
            if (!arg[0]) { free(arg); return make_num(0); }
            int r = 1;
            for (char *p = arg; *p; p++) { if (!isalnum((unsigned char)*p)) { r = 0; break; } }
            free(arg);
            return make_num(r);
        }
        if (!strcmp(fn, "isspace")) {
            if (argc < 2) return make_num(0);
            char *arg = resolve_arg(args[1]);
            if (!arg[0]) { free(arg); return make_num(0); }
            int r = 1;
            for (char *p = arg; *p; p++) { if (!isspace((unsigned char)*p)) { r = 0; break; } }
            free(arg);
            return make_num(r);
        }
        fatal("line %d: unknown string function '%s'", line, fn);
    }

    if (!strcmp(lib, "file")) {
        if (!strcmp(fn, "read")) {
            if (argc < 2) fatal("line %d: file read expects a path", line);
            char *path = resolve_arg(args[1]);
            FILE *fp = fopen(path, "rb");
            if (!fp) { snprintf(last_error, sizeof(last_error), "cannot open '%s'", path); free(path); return make_str(""); }
            last_error[0] = 0;
            fseek(fp, 0, SEEK_END); long sz = ftell(fp); rewind(fp);
            char *buf = malloc(sz + 1); if (!buf) { fclose(fp); free(path); fatal("out of memory"); }
            long got = fread(buf, 1, sz, fp); fclose(fp);
            buf[got] = 0; free(path);
            Value v = make_str(buf); free(buf); return v;
        }
        if (!strcmp(fn, "write")) {
            if (argc < 3) fatal("line %d: file write expects path and content", line);
            char *path = resolve_arg(args[1]);
            char *content = resolve_arg(args[2]);
            FILE *fp = fopen(path, "wb");
            if (fp) { fwrite(content, 1, strlen(content), fp); fclose(fp); }
            free(path); free(content);
            return make_num(0);
        }
        if (!strcmp(fn, "append")) {
            if (argc < 3) fatal("line %d: file append expects path and content", line);
            char *path = resolve_arg(args[1]);
            char *content = resolve_arg(args[2]);
            FILE *fp = fopen(path, "ab");
            if (fp) { fwrite(content, 1, strlen(content), fp); fclose(fp); }
            free(path); free(content);
            return make_num(0);
        }
        if (!strcmp(fn, "delete")) {
            if (argc < 2) fatal("line %d: file delete expects a path", line);
            char *path = resolve_arg(args[1]);
            int r = remove(path);
            free(path);
            return make_num(r == 0 ? 1 : 0);
        }
        if (!strcmp(fn, "exists")) {
            if (argc < 2) fatal("line %d: file exists expects a path", line);
            char *path = resolve_arg(args[1]);
            FILE *fp = fopen(path, "rb");
            int r = fp ? 1 : 0;
            if (fp) fclose(fp);
            free(path);
            return make_num(r);
        }
        if (!strcmp(fn, "list")) {
            if (argc < 2) fatal("line %d: file list expects a path", line);
            char *path = resolve_arg(args[1]);
            char cmd[4096]; snprintf(cmd, sizeof(cmd), "ls -1 \"%s\"", path);
            FILE *fp = popen(cmd, "r");
            if (!fp) { free(path); return make_str(""); }
            char buf[65536] = {0}; size_t total = 0;
            while (fgets(buf + total, sizeof(buf) - total, fp)) total = strlen(buf);
            pclose(fp); free(path);
            return make_str(buf);
        }
        fatal("line %d: unknown file function '%s'", line, fn);
    }

    if (!strcmp(lib, "list")) {
        if (!strcmp(fn, "new")) {
            return make_list();
        }
        if (!strcmp(fn, "push")) {
            if (argc < 3) fatal("line %d: list push expects list name and value", line);
            char *vname = args[1];
            if (vname[0] == '\1') vname++;
            int vi = var_find(vname);
            if (vi < 0) { var_set(vname, make_list()); vi = var_find(vname); }
            if (vars[vi].val.type != VAL_LIST) fatal("line %d: '%s' is not a list", line, vname);
            char *vs = resolve_arg(args[2]);
            char *end;
            double d = strtod(vs, &end);
            Value item;
            if (*end) {
                if (args[2][0] == '\1') {
                    int oi = var_find(args[2] + 1);
                    if (oi >= 0) item = copy_val(vars[oi].val);
                    else item = make_str(vs);
                } else {
                    item = make_str(vs);
                }
            } else item = make_num(d);
            free(vs);
            list_append(&vars[vi].val, item);
            return make_num(0);
        }
        if (!strcmp(fn, "pop")) {
            if (argc < 2) fatal("line %d: list pop expects list name", line);
            char *vname = args[1];
            if (vname[0] == '\1') vname++;
            int vi = var_find(vname);
            if (vi < 0 || vars[vi].val.type != VAL_LIST) fatal("line %d: list '%s' not found", line, vname);
            if (vars[vi].val.data.list.count == 0) return make_num(0);
            Value item = vars[vi].val.data.list.items[--vars[vi].val.data.list.count];
            return item;
        }
        if (!strcmp(fn, "len")) {
            if (argc < 2) fatal("line %d: list len expects list name", line);
            char *vname = args[1];
            if (vname[0] == '\1') vname++;
            int vi = var_find(vname);
            if (vi < 0) return make_num(0);
            if (vars[vi].val.type != VAL_LIST) return make_num(0);
            return make_num(vars[vi].val.data.list.count);
        }
        if (!strcmp(fn, "at")) {
            if (argc < 3) fatal("line %d: list get expects list name and index", line);
            char *vname = args[1];
            if (vname[0] == '\1') vname++;
            int vi = var_find(vname);
            if (vi < 0 || vars[vi].val.type != VAL_LIST) fatal("line %d: list '%s' not found", line, vname);
            char *is = resolve_arg(args[2]);
            int idx = (int)strtod(is, NULL); free(is);
            return list_get(vars[vi].val, idx);
        }
        if (!strcmp(fn, "map")) {
            if (argc < 3) fatal("line %d: list map expects list name and function name", line);
            char *vname = args[1];
            if (vname[0] == '\1') vname++;
            int vi = var_find(vname);
            if (vi < 0 || vars[vi].val.type != VAL_LIST) fatal("line %d: list '%s' not found", line, vname);
            int fi = resolve_fn_idx(args[2], line);
            if (funcs[fi].nparams < 1) fatal("line %d: function '%s' expects at least 1 parameter", line, args[2]);
            Value lst = vars[vi].val;
            Value result = make_list();
            Var saved[MAX_VARS];
            int saved_count = var_count;
            memcpy(saved, vars, sizeof(Var) * var_count);
            for (int i = 0; i < lst.data.list.count; i++) {
                var_count = saved_count;
                memcpy(vars, saved, sizeof(Var) * var_count);
                int pi = var_ensure(funcs[fi].params[0]);
                vars[pi].val = copy_val(lst.data.list.items[i]);
                _last_expr_val = make_num(0);
                exec_stmt(funcs[fi].body);
                list_append(&result, copy_val(_last_expr_val));
            }
            var_count = saved_count;
            memcpy(vars, saved, sizeof(Var) * var_count);
            return result;
        }
        if (!strcmp(fn, "filter")) {
            if (argc < 3) fatal("line %d: list filter expects list name and function name", line);
            char *vname = args[1];
            if (vname[0] == '\1') vname++;
            int vi = var_find(vname);
            if (vi < 0 || vars[vi].val.type != VAL_LIST) fatal("line %d: list '%s' not found", line, vname);
            int fi = resolve_fn_idx(args[2], line);
            if (funcs[fi].nparams < 1) fatal("line %d: function '%s' expects at least 1 parameter", line, args[2]);
            Value lst = vars[vi].val;
            Value result = make_list();
            Var saved[MAX_VARS];
            int saved_count = var_count;
            memcpy(saved, vars, sizeof(Var) * var_count);
            for (int i = 0; i < lst.data.list.count; i++) {
                var_count = saved_count;
                memcpy(vars, saved, sizeof(Var) * var_count);
                int pi = var_ensure(funcs[fi].params[0]);
                vars[pi].val = copy_val(lst.data.list.items[i]);
                _last_expr_val = make_num(0);
                exec_stmt(funcs[fi].body);
                if (truthy(_last_expr_val))
                    list_append(&result, copy_val(lst.data.list.items[i]));
            }
            var_count = saved_count;
            memcpy(vars, saved, sizeof(Var) * var_count);
            return result;
        }
        if (!strcmp(fn, "reduce")) {
            if (argc < 4) fatal("line %d: list reduce expects list name, function name, and initial value", line);
            char *vname = args[1];
            if (vname[0] == '\1') vname++;
            int vi = var_find(vname);
            if (vi < 0 || vars[vi].val.type != VAL_LIST) fatal("line %d: list '%s' not found", line, vname);
            int fi = resolve_fn_idx(args[2], line);
            if (funcs[fi].nparams < 2) fatal("line %d: function '%s' expects at least 2 parameters for reduce", line, args[2]);
            Value lst = vars[vi].val;
            char *is = resolve_arg(args[3]);
            char *end;
            double d = strtod(is, &end);
            Value accum;
            if (*end) accum = make_str(is);
            else accum = make_num(d);
            free(is);
            Var saved[MAX_VARS];
            int saved_count = var_count;
            memcpy(saved, vars, sizeof(Var) * var_count);
            for (int i = 0; i < lst.data.list.count; i++) {
                var_count = saved_count;
                memcpy(vars, saved, sizeof(Var) * var_count);
                int pa = var_ensure(funcs[fi].params[0]);
                int pb = var_ensure(funcs[fi].params[1]);
                vars[pa].val = copy_val(accum);
                vars[pb].val = copy_val(lst.data.list.items[i]);
                val_free(accum);
                _last_expr_val = make_num(0);
                exec_stmt(funcs[fi].body);
                accum = copy_val(_last_expr_val);
            }
            var_count = saved_count;
            memcpy(vars, saved, sizeof(Var) * var_count);
            return accum;
        }
        fatal("line %d: unknown list function '%s'", line, fn);
    }

    if (!strcmp(lib, "dict")) {
        if (!strcmp(fn, "new")) {
            return make_dict();
        }
        if (!strcmp(fn, "set")) {
            if (argc < 4) fatal("line %d: dict set expects dict name, key, and value", line);
            char *vname = args[1];
            if (vname[0] == '\1') vname++;
            int vi = var_find(vname);
            if (vi < 0) { var_set(vname, make_dict()); vi = var_find(vname); }
            if (vars[vi].val.type != VAL_DICT) fatal("line %d: '%s' is not a dict", line, vname);
            char *key = resolve_arg(args[2]);
            char *vs = resolve_arg(args[3]);
            char *end;
            double d = strtod(vs, &end);
            Value item;
            if (*end) {
                if (args[3][0] == '$') {
                    int oi = var_find(args[3] + 1);
                    if (oi >= 0) item = copy_val(vars[oi].val);
                    else item = make_str(vs);
                } else {
                    item = make_str(vs);
                }
            } else item = make_num(d);
            free(vs);
            dict_set(&vars[vi].val, key, item);
            free(key);
            return make_num(0);
        }
        if (!strcmp(fn, "get")) {
            if (argc < 3) fatal("line %d: dict get expects dict name and key", line);
            char *vname = args[1];
            if (vname[0] == '\1') vname++;
            int vi = var_find(vname);
            if (vi < 0 || vars[vi].val.type != VAL_DICT) fatal("line %d: dict '%s' not found", line, vname);
            char *key = resolve_arg(args[2]);
            Value v = dict_get(vars[vi].val, key);
            free(key);
            return v;
        }
        if (!strcmp(fn, "contains")) {
            if (argc < 3) fatal("line %d: dict has expects dict name and key", line);
            char *vname = args[1];
            if (vname[0] == '\1') vname++;
            int vi = var_find(vname);
            if (vi < 0 || vars[vi].val.type != VAL_DICT) return make_num(0);
            char *key = resolve_arg(args[2]);
            int r = dict_has(vars[vi].val, key);
            free(key);
            return make_num(r);
        }
        if (!strcmp(fn, "keys")) {
            if (argc < 2) fatal("line %d: dict keys expects dict name", line);
            char *vname = args[1];
            if (vname[0] == '\1') vname++;
            int vi = var_find(vname);
            if (vi < 0 || vars[vi].val.type != VAL_DICT) return make_list();
            return dict_keys(vars[vi].val);
        }
        if (!strcmp(fn, "len")) {
            if (argc < 2) fatal("line %d: dict len expects dict name", line);
            char *vname = args[1];
            if (vname[0] == '\1') vname++;
            int vi = var_find(vname);
            if (vi < 0 || vars[vi].val.type != VAL_DICT) return make_num(0);
            return make_num(vars[vi].val.data.dict.count);
        }
        if (!strcmp(fn, "remove")) {
            if (argc < 3) fatal("line %d: dict remove expects dict name and key", line);
            char *vname = args[1];
            if (vname[0] == '\1') vname++;
            int vi = var_find(vname);
            if (vi < 0 || vars[vi].val.type != VAL_DICT) return make_num(0);
            char *key = resolve_arg(args[2]);
            int r = dict_remove(&vars[vi].val, key);
            free(key);
            return make_num(r);
        }
        if (!strcmp(fn, "clear")) {
            if (argc < 2) fatal("line %d: dict clear expects dict name", line);
            char *vname = args[1];
            if (vname[0] == '\1') vname++;
            int vi = var_find(vname);
            if (vi < 0 || vars[vi].val.type != VAL_DICT) return make_num(0);
            dict_clear(&vars[vi].val);
            return make_num(0);
        }
        fatal("line %d: unknown dict function '%s'", line, fn);
    }

    if (!strcmp(lib, "sys")) {
        if (!strcmp(fn, "eval")) {
            if (argc < 2) fatal("line %d: sys eval expects code string", line);
            char *code = resolve_arg(args[1]);
            jmp_buf old;
            memcpy(old, error_jmp, sizeof(jmp_buf));
            if (setjmp(error_jmp) == 0) {
                lex_init(code);
                lex_next();
                while (lex_cur.type != TOK_EOF) {
                    ASTNode *s = parse_stmt();
                    if (s->type != NODE_EMPTY) {
                        int r = exec_stmt(s);
                        if (r == 1) { memcpy(error_jmp, old, sizeof(jmp_buf)); free(code); return make_num(0); }
                    }
                    if (lex_cur.type == TOK_NEWLINE) lex_next();
                }
                memcpy(error_jmp, old, sizeof(jmp_buf));
                free(code);
                return copy_val(_last_expr_val);
            } else {
                memcpy(error_jmp, old, sizeof(jmp_buf));
                free(code);
                return make_num(0);
            }
        }
        if (!strcmp(fn, "cmd")) {
            if (argc < 2) fatal("line %d: sys cmd expects a command", line);
            char *cmd = resolve_arg(args[1]);
            size_t clen = strlen(cmd);
            while (clen > 0 && (cmd[clen-1] == ' ' || cmd[clen-1] == '\t')) clen--;
            if (clen > 0 && cmd[clen-1] == '&') {
                int rc = system(cmd);
                free(cmd);
                char buf[32];
                snprintf(buf, sizeof(buf), "%d", rc);
                return make_str(buf);
            }
            FILE *fp = popen(cmd, "r");
            if (!fp) { snprintf(last_error, sizeof(last_error), "popen failed: '%s'", cmd); free(cmd); return make_str(""); }
            last_error[0] = 0;
            char buf[65536] = {0}; size_t total = 0;
            while (fgets(buf + total, sizeof(buf) - total, fp)) total = strlen(buf);
            pclose(fp); free(cmd);
            return make_str(buf);
        }
        if (!strcmp(fn, "env")) {
            if (argc < 2) fatal("line %d: sys env expects a variable name", line);
            char *name = resolve_arg(args[1]);
            char *val = getenv(name);
            Value v = val ? make_str(val) : make_str("");
            free(name);
            return v;
        }
        if (!strcmp(fn, "log")) {
            if (argc < 2) fatal("line %d: sys log expects a message", line);
            char *msg = resolve_arg(args[1]);
            fprintf(stderr, "%s\n", msg);
            free(msg);
            return make_num(0);
        }
        if (!strcmp(fn, "last_error")) {
            return make_str(last_error[0] ? last_error : "");
        }
        if (!strcmp(fn, "clear_error")) {
            last_error[0] = 0;
            return make_num(0);
        }
        fatal("line %d: unknown sys function '%s'", line, fn);
    }

    fatal("line %d: unknown library '%s'", line, lib);
    return make_num(0);
}
