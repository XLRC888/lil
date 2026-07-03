#include "lil.h"

Token lex_cur;
static Token lex_peek;
static int lex_has_peek;
static const char *lex_src;
static int lex_pos;
static int lex_len;
static int lex_line;

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
            if (!strcmp(word, "stringify")) { free(word); t.type = TOK_STRINGIFY; return t; }
            if (!strcmp(word, "strify"))   { free(word); fprintf(stderr, "line %d: warning: \"strify\" is deprecated, use \"stringify\" instead\n", lex_line); t.type = TOK_STRINGIFY; return t; }
            if (!strcmp(word, "intify"))   { free(word); t.type = TOK_INTIFY; return t; }
            if (!strcmp(word, "toggle"))   { free(word); t.type = TOK_TOGGLE; return t; }
            if (!strcmp(word, "swify"))    { free(word); fprintf(stderr, "line %d: warning: \"swify\" is deprecated, use \"toggle\" instead\n", lex_line); t.type = TOK_TOGGLE; return t; }
            if (!strcmp(word, "try"))     { free(word); t.type = TOK_TRY; return t; }
            if (!strcmp(word, "catch"))   { free(word); t.type = TOK_CATCH; return t; }
            if (!strcmp(word, "loop"))    { free(word); t.type = TOK_LOOP; return t; }
            if (!strcmp(word, "stop"))    { free(word); t.type = TOK_STOP; return t; }
            if (!strcmp(word, "break"))   { free(word); t.type = TOK_BREAK; return t; }
            if (!strcmp(word, "continue")) { free(word); t.type = TOK_CONTINUE; return t; }
            if (!strcmp(word, "include")) { free(word); t.type = TOK_INCLUDE; return t; }
            if (!strcmp(word, "uninclude")) { free(word); t.type = TOK_UNINCLUDE; return t; }
            if (!strcmp(word, "has"))     { free(word); t.type = TOK_HAS; return t; }
            if (!strcmp(word, "nocase"))  { free(word); t.type = TOK_NOCASE; return t; }
            if (!strcmp(word, "anywhere")) { free(word); t.type = TOK_ANYWHERE; return t; }
            if (!strcmp(word, "word"))    { free(word); t.type = TOK_WORD; return t; }
            if (!strcmp(word, "force"))   { free(word); t.type = TOK_FORCE; return t; }
            if (!strcmp(word, "unforce")) { free(word); t.type = TOK_UNFORCE; return t; }
            if (!strcmp(word, "get"))     { free(word); t.type = TOK_GET; return t; }
            if (!strcmp(word, "from"))    { free(word); t.type = TOK_FROM; return t; }
            if (!strcmp(word, "struct"))  { free(word); t.type = TOK_STRUCT; return t; }

            t.type = TOK_ID;
            t.val.str = word;
            return t;
        }

        lex_pos++;
        switch (c) {
            case '+': t.type = TOK_PLUS; return t;
            case '-': t.type = TOK_MINUS; return t;
            case '*': t.type = TOK_STAR; return t;
            case '/':
                if (lex_pos < lex_len && lex_src[lex_pos] == '/') {
                    lex_pos++;
                    while (lex_pos < lex_len && lex_src[lex_pos] != '\n') lex_pos++;
                    return lex_scan();
                }
                t.type = TOK_SLASH; return t;
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
                t.type = TOK_AMPERSAND; return t;
            case '|':
                if (lex_pos < lex_len && lex_src[lex_pos] == '|') { lex_pos++; t.type = TOK_OR; return t; }
                t.type = TOK_PIPE; return t;
            case '#':
                while (lex_pos < lex_len && lex_src[lex_pos] != '\n') lex_pos++;
                return lex_scan();
            case '@': t.type = TOK_AT; return t;
            case '^': t.type = TOK_CARET; return t;
            case '?': t.type = TOK_QMARK; return t;
            case '.': t.type = TOK_DOT; return t;
            case ':': t.type = TOK_COLON; return t;            case '$': {
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

Token lex_next(void) {
    if (lex_has_peek) {
        lex_has_peek = 0;
        return lex_cur = lex_peek;
    }
    return lex_cur = lex_scan();
}

Token lex_peek_next(void) {
    if (!lex_has_peek) {
        lex_peek = lex_scan();
        lex_has_peek = 1;
    }
    return lex_peek;
}

void lex_init(const char *src) {
    lex_src = src;
    lex_pos = 0;
    lex_len = strlen(src);
    lex_line = 1;
    lex_has_peek = 0;
}
