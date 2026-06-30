#define _POSIX_C_SOURCE 199309L
#include "lil.h"
#include <sys/wait.h>

int lib_imported[7];

int lib_idx(const char *name) {
    if (!strcmp(name, "math")) return 0;
    if (!strcmp(name, "date")) return 1;
    if (!strcmp(name, "string")) return 2;
    if (!strcmp(name, "file")) return 3;
    if (!strcmp(name, "sys")) return 4;
    if (!strcmp(name, "gtk")) return 5;
    if (!strcmp(name, "wlroots")) return 6;
    return -1;
}

char *resolve_arg(char *arg) {
    if (arg[0] == '$') {
        Value v = var_get(arg + 1);
        char *s = val_tostr(v);
        return s;
    }
    return sdup(arg);
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
        fatal("line %d: unknown string function '%s'", line, fn);
    }

    if (!strcmp(lib, "file")) {
        if (!strcmp(fn, "read")) {
            if (argc < 2) fatal("line %d: file read expects a path", line);
            char *path = resolve_arg(args[1]);
            FILE *fp = fopen(path, "rb");
            if (!fp) { free(path); return make_str(""); }
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
            char buf[65536]; size_t total = 0;
            while (fgets(buf + total, sizeof(buf) - total, fp)) total = strlen(buf);
            pclose(fp); free(path);
            return make_str(buf);
        }
        fatal("line %d: unknown file function '%s'", line, fn);
    }

    if (!strcmp(lib, "sys")) {
        if (!strcmp(fn, "cmd")) {
            if (argc < 2) fatal("line %d: sys cmd expects a command", line);
            char *cmd = resolve_arg(args[1]);
            FILE *fp = popen(cmd, "r");
            if (!fp) { free(cmd); return make_str(""); }
            char buf[65536]; size_t total = 0;
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
        fatal("line %d: unknown sys function '%s'", line, fn);
    }

    fatal("line %d: unknown library '%s'", line, lib);
    return make_num(0);
}
