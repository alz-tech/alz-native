#ifndef ALZ_LEXER_H
#define ALZ_LEXER_H

// ── Token Types ───────────────────────────────────────────────────────────────
typedef enum {
    // Literals
    TOK_NUMBER, TOK_STRING, TOK_IDENT, TOK_BOOL, TOK_NULL,

    // Operators
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT, TOK_CARET,
    TOK_EQ, TOK_NEQ, TOK_LT, TOK_LTE, TOK_GT, TOK_GTE,
    TOK_AND, TOK_OR, TOK_NOT,
    TOK_ASSIGN,       // =
    TOK_PLUS_EQ,      // +=
    TOK_MINUS_EQ,     // -=
    TOK_DOT,          // .
    TOK_COMMA,        // ,
    TOK_COLON,        // :
    TOK_LBRACKET,     // [
    TOK_RBRACKET,     // ]
    TOK_LBRACE,       // {
    TOK_RBRACE,       // }
    TOK_LPAREN,       // (
    TOK_RPAREN,       // )
    TOK_ARROW,        // ->

    // Keywords
    TOK_PRINT,        // print
    TOK_ASK,          // ask
    TOK_IF,           // if
    TOK_ELSE,         // else
    TOK_ELIF,         // else if / elif
    TOK_EACH,         // each
    TOK_FOR,          // for
    TOK_WHILE,        // while
    TOK_IN,           // in
    TOK_DEFINE,       // define
    TOK_RETURN,       // return
    TOK_BREAK,        // break
    TOK_CONTINUE,     // continue
    TOK_USE,          // use (imports)
    TOK_FROM,         // from
    TOK_SERVE,        // serve
    TOK_ON,           // on
    TOK_ROUTE,        // route
    TOK_GET,          // get
    TOK_POST,         // post
    TOK_PUT,          // put
    TOK_DELETE,       // delete
    TOK_RESPOND,      // respond
    TOK_FETCH,        // fetch
    TOK_RUN,          // run (shell)
    TOK_NEW,          // new
    TOK_TRUE,         // true
    TOK_FALSE,        // false
    TOK_AND_KW,       // and
    TOK_OR_KW,        // or
    TOK_NOT_KW,       // not
    TOK_IS,           // is (==)
    TOK_ISNT,         // isnt (!=)

    // Structure
    TOK_NEWLINE,
    TOK_INDENT,
    TOK_DEDENT,
    TOK_EOF,
    TOK_ERROR,
} TokenType;

// ── Token ─────────────────────────────────────────────────────────────────────
typedef struct {
    TokenType   type;
    const char *start;   // pointer into source
    int         length;
    int         line;
    double      num_val; // for TOK_NUMBER
} Token;

// ── Lexer ─────────────────────────────────────────────────────────────────────
typedef struct {
    const char *source;
    const char *current;
    int         line;

    // Indent tracking
    int         indent_stack[64];
    int         indent_top;
    int         pending_dedents;
    int         at_line_start;
} Lexer;

// ── Lexer API ─────────────────────────────────────────────────────────────────
void  lexer_init(Lexer *lex, const char *source);
Token lexer_next(Lexer *lex);
Token lexer_peek(Lexer *lex);

// Debug
const char *token_type_name(TokenType type);
void        token_print(Token tok);

#endif
