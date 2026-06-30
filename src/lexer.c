#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "../include/lexer.h"

void lexer_init(Lexer *lex, const char *source) {
    lex->source        = source;
    lex->current       = source;
    lex->line          = 1;
    lex->indent_top    = 0;
    lex->indent_stack[0] = 0;
    lex->pending_dedents = 0;
    lex->at_line_start = 1;
}

// ── Helpers ───────────────────────────────────────────────────────────────────
static int  is_at_end(Lexer *l)      { return *l->current == '\0'; }
static char advance(Lexer *l)        { return *l->current++; }
static char peek(Lexer *l)           { return *l->current; }
static char peek_next(Lexer *l)      { return l->current[1]; }
static int  match(Lexer *l, char c)  {
    if (*l->current != c) return 0;
    l->current++; return 1;
}

static Token make_tok(Lexer *l, TokenType type, const char *start) {
    Token t;
    t.type    = type;
    t.start   = start;
    t.length  = (int)(l->current - start);
    t.line    = l->line;
    t.num_val = 0;
    return t;
}

static Token error_tok(Lexer *l, const char *msg) {
    Token t;
    t.type    = TOK_ERROR;
    t.start   = msg;
    t.length  = (int)strlen(msg);
    t.line    = l->line;
    t.num_val = 0;
    return t;
}

// ── Keyword check ─────────────────────────────────────────────────────────────
static TokenType check_keyword(const char *s, int len) {
    struct { const char *w; TokenType t; } kw[] = {
        {"print",   TOK_PRINT},
        {"display", TOK_PRINT},   // alias
        {"ask",     TOK_ASK},
        {"input",   TOK_ASK},     // alias
        {"if",      TOK_IF},
        {"else",    TOK_ELSE},
        {"elif",    TOK_ELIF},
        {"each",    TOK_EACH},
        {"for",     TOK_FOR},
        {"while",   TOK_WHILE},
        {"in",      TOK_IN},
        {"define",  TOK_DEFINE},
        {"fn",      TOK_DEFINE},  // alias
        {"return",  TOK_RETURN},
        {"break",   TOK_BREAK},
        {"continue",TOK_CONTINUE},
        {"use",     TOK_USE},
        {"from",    TOK_FROM},
        {"serve",   TOK_SERVE},
        {"on",      TOK_ON},
        {"route",   TOK_ROUTE},
        {"get",     TOK_GET},
        {"post",    TOK_POST},
        {"put",     TOK_PUT},
        {"delete",  TOK_DELETE},
        {"respond", TOK_RESPOND},
        {"fetch",   TOK_FETCH},
        {"run",     TOK_RUN},
        {"new",     TOK_NEW},
        {"true",    TOK_TRUE},
        {"false",   TOK_FALSE},
        {"and",     TOK_AND_KW},
        {"or",      TOK_OR_KW},
        {"not",     TOK_NOT_KW},
        {"is",      TOK_IS},
        {"isnt",    TOK_ISNT},
        {"null",    TOK_NULL},
        {"none",    TOK_NULL},    // alias
        {NULL, 0}
    };
    for (int i = 0; kw[i].w; i++)
        if ((int)strlen(kw[i].w) == len && memcmp(s, kw[i].w, len) == 0)
            return kw[i].t;
    return TOK_IDENT;
}

// ── Skip whitespace and comments ──────────────────────────────────────────────
static void skip_inline_whitespace(Lexer *l) {
    while (peek(l) == ' ' || peek(l) == '\t' || peek(l) == '\r') advance(l);
}

// ── Indent handling ───────────────────────────────────────────────────────────
static Token handle_indent(Lexer *l) {
    // Count leading spaces/tabs at start of line
    int spaces = 0;
    const char *start = l->current;
    while (peek(l) == ' ' || peek(l) == '\t') {
        if (peek(l) == '\t') spaces += 4; else spaces++;
        advance(l);
    }

    // Blank line or comment — skip
    if (peek(l) == '\n' || peek(l) == '#' || peek(l) == '-') {
        return make_tok(l, TOK_NEWLINE, start);
    }

    int current_indent = l->indent_stack[l->indent_top];

    if (spaces > current_indent) {
        // INDENT
        if (l->indent_top >= 63) return error_tok(l, "Too deeply nested");
        l->indent_stack[++l->indent_top] = spaces;
        return make_tok(l, TOK_INDENT, start);
    } else if (spaces < current_indent) {
        // DEDENT — may need multiple
        while (l->indent_top > 0 && l->indent_stack[l->indent_top] > spaces) {
            l->indent_top--;
            l->pending_dedents++;
        }
        l->pending_dedents--; // emit first now
        return make_tok(l, TOK_DEDENT, start);
    }

    // Same level — no indent token, just continue
    return make_tok(l, TOK_NEWLINE, start);
}

// ── Main tokenizer ────────────────────────────────────────────────────────────
Token lexer_next(Lexer *l) {
    // Pending dedents from previous line
    if (l->pending_dedents > 0) {
        l->pending_dedents--;
        const char *s = l->current;
        return make_tok(l, TOK_DEDENT, s);
    }

    // Handle start of new line (indentation)
    if (l->at_line_start) {
        l->at_line_start = 0;
        Token t = handle_indent(l);
        if (t.type == TOK_INDENT || t.type == TOK_DEDENT) return t;
        // else NEWLINE with same indent — fall through to normal scanning
    }

    skip_inline_whitespace(l);

    if (is_at_end(l)) {
        // Emit remaining DEDENTs before EOF
        if (l->indent_top > 0) {
            l->indent_top--;
            const char *s = l->current;
            return make_tok(l, TOK_DEDENT, s);
        }
        return make_tok(l, TOK_EOF, l->current);
    }

    const char *start = l->current;
    char c = advance(l);

    // ── Newline ───────────────────────────────────────────────────────────────
    if (c == '\n') {
        l->line++;
        l->at_line_start = 1;
        return make_tok(l, TOK_NEWLINE, start);
    }

    // ── Comments: -- or # ─────────────────────────────────────────────────────
    if (c == '-' && peek(l) == '-') {
        while (peek(l) != '\n' && !is_at_end(l)) advance(l);
        return lexer_next(l);
    }
    if (c == '#') {
        while (peek(l) != '\n' && !is_at_end(l)) advance(l);
        return lexer_next(l);
    }

    // String: "..." - nested quotes inside {expr} are allowed
    if (c == '"' || c == '\'') {
        char quote = c;
        int  depth = 0;
        while (!is_at_end(l)) {
            char ch = peek(l);
            if (ch == '{') depth++;
            else if (ch == '}' && depth > 0) depth--;
            else if (ch == quote && depth == 0) break;
            if (ch == '\n') l->line++;
            advance(l);
        }
        if (is_at_end(l)) return error_tok(l, "Unterminated string");
        advance(l);
        return make_tok(l, TOK_STRING, start);
    }
    // ── Number ────────────────────────────────────────────────────────────────
    if (isdigit(c) || (c == '.' && isdigit(peek(l)))) {
        while (isdigit(peek(l))) advance(l);
        if (peek(l) == '.' && isdigit(peek_next(l))) {
            advance(l);
            while (isdigit(peek(l))) advance(l);
        }
        Token t = make_tok(l, TOK_NUMBER, start);
        t.num_val = atof(start);
        return t;
    }

    // ── Identifier / keyword ──────────────────────────────────────────────────
    if (isalpha(c) || c == '_') {
        while (isalnum(peek(l)) || peek(l) == '_') advance(l);
        int len  = (int)(l->current - start);
        TokenType type = check_keyword(start, len);
        return make_tok(l, type, start);
    }

    // ── Operators ─────────────────────────────────────────────────────────────
    switch (c) {
        case '+': return make_tok(l, match(l,'=') ? TOK_PLUS_EQ  : TOK_PLUS,    start);
        case '-': return make_tok(l, match(l,'=') ? TOK_MINUS_EQ :
                                     match(l,'>') ? TOK_ARROW     : TOK_MINUS,   start);
        case '*': return make_tok(l, TOK_STAR,    start);
        case '/': return make_tok(l, TOK_SLASH,   start);
        case '%': return make_tok(l, TOK_PERCENT, start);
        case '^': return make_tok(l, TOK_CARET,   start);
        case '=': return make_tok(l, match(l,'=') ? TOK_EQ     : TOK_ASSIGN,  start);
        case '!': return make_tok(l, match(l,'=') ? TOK_NEQ    : TOK_NOT,     start);
        case '<': return make_tok(l, match(l,'=') ? TOK_LTE    : TOK_LT,      start);
        case '>': return make_tok(l, match(l,'=') ? TOK_GTE    : TOK_GT,      start);
        case '.': return make_tok(l, TOK_DOT,     start);
        case ',': return make_tok(l, TOK_COMMA,   start);
        case ':': return make_tok(l, TOK_COLON,   start);
        case '[': return make_tok(l, TOK_LBRACKET,start);
        case ']': return make_tok(l, TOK_RBRACKET,start);
        case '{': return make_tok(l, TOK_LBRACE,  start);
        case '}': return make_tok(l, TOK_RBRACE,  start);
        case '(': return make_tok(l, TOK_LPAREN,  start);
        case ')': return make_tok(l, TOK_RPAREN,  start);
    }

    return error_tok(l, "Unexpected character");
}

// ── Peek without consuming ────────────────────────────────────────────────────
Token lexer_peek(Lexer *lex) {
    Lexer saved = *lex;
    Token t = lexer_next(lex);
    *lex = saved;
    return t;
}

// ── Debug ─────────────────────────────────────────────────────────────────────
const char *token_type_name(TokenType type) {
    switch (type) {
        case TOK_NUMBER:   return "NUMBER";
        case TOK_STRING:   return "STRING";
        case TOK_IDENT:    return "IDENT";
        case TOK_BOOL:     return "BOOL";
        case TOK_NULL:     return "NULL";
        case TOK_PLUS:     return "PLUS";
        case TOK_MINUS:    return "MINUS";
        case TOK_STAR:     return "STAR";
        case TOK_SLASH:    return "SLASH";
        case TOK_ASSIGN:   return "ASSIGN";
        case TOK_EQ:       return "EQ";
        case TOK_NEQ:      return "NEQ";
        case TOK_LT:       return "LT";
        case TOK_LTE:      return "LTE";
        case TOK_GT:       return "GT";
        case TOK_GTE:      return "GTE";
        case TOK_AND_KW:   return "AND";
        case TOK_OR_KW:    return "OR";
        case TOK_NOT_KW:   return "NOT";
        case TOK_IS:       return "IS";
        case TOK_ISNT:     return "ISNT";
        case TOK_DOT:      return "DOT";
        case TOK_COMMA:    return "COMMA";
        case TOK_COLON:    return "COLON";
        case TOK_LBRACKET: return "LBRACKET";
        case TOK_RBRACKET: return "RBRACKET";
        case TOK_LBRACE:   return "LBRACE";
        case TOK_RBRACE:   return "RBRACE";
        case TOK_LPAREN:   return "LPAREN";
        case TOK_RPAREN:   return "RPAREN";
        case TOK_PRINT:    return "PRINT";
        case TOK_ASK:      return "ASK";
        case TOK_IF:       return "IF";
        case TOK_ELSE:     return "ELSE";
        case TOK_ELIF:     return "ELIF";
        case TOK_EACH:     return "EACH";
        case TOK_FOR:      return "FOR";
        case TOK_WHILE:    return "WHILE";
        case TOK_IN:       return "IN";
        case TOK_DEFINE:   return "DEFINE";
        case TOK_RETURN:   return "RETURN";
        case TOK_BREAK:    return "BREAK";
        case TOK_CONTINUE: return "CONTINUE";
        case TOK_SERVE:    return "SERVE";
        case TOK_RESPOND:  return "RESPOND";
        case TOK_FETCH:    return "FETCH";
        case TOK_TRUE:     return "TRUE";
        case TOK_FALSE:    return "FALSE";
        case TOK_NEWLINE:  return "NEWLINE";
        case TOK_INDENT:   return "INDENT";
        case TOK_DEDENT:   return "DEDENT";
        case TOK_EOF:      return "EOF";
        case TOK_ERROR:    return "ERROR";
        default:           return "?";
    }
}

void token_print(Token tok) {
    printf("[%s line %d] '%.*s'\n",
        token_type_name(tok.type), tok.line, tok.length, tok.start);
}
