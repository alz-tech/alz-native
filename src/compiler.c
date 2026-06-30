#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "../include/compiler.h"

// ── Forward declarations ──────────────────────────────────────────────────────
static void statement(Compiler *c);
static void expression(Compiler *c);
static void block(Compiler *c);

// ── Compiler helpers ──────────────────────────────────────────────────────────
static void advance_tok(Compiler *c) {
    c->previous = c->current;
    for (;;) {
        c->current = lexer_next(&c->lex);
        if (c->current.type != TOK_ERROR) break;
        fprintf(stderr, "[line %d] Lexer error: %.*s\n",
            c->current.line, c->current.length, c->current.start);
        c->had_error = 1;
    }
}

static int check(Compiler *c, TokenType type) {
    return c->current.type == type;
}

static int match_tok(Compiler *c, TokenType type) {
    if (!check(c, type)) return 0;
    advance_tok(c);
    return 1;
}

static void consume(Compiler *c, TokenType type, const char *msg) {
    if (c->current.type == type) { advance_tok(c); return; }
    fprintf(stderr, "\n🔴  AlzScript Compile Error [line %d]\n", c->current.line);
    fprintf(stderr, "    %s\n", msg);
    fprintf(stderr, "    Got: '%.*s'\n\n", c->current.length, c->current.start);
    c->had_error = 1;
}

static void skip_newlines(Compiler *c) {
    while (check(c, TOK_NEWLINE)) advance_tok(c);
}

// ── Emit helpers ──────────────────────────────────────────────────────────────
static int emit(Compiler *c, OpCode op, int arg) {
    return chunk_write(c->chunk, op, arg, c->previous.line);
}

static int emit_jump(Compiler *c, OpCode op) {
    return emit(c, op, 0); // patched later
}

static void patch_jump(Compiler *c, int idx) {
    c->chunk->code[idx].arg = (int)c->chunk->count;
}

// ── String extraction (strips quotes, handles {interpolation}) ────────────────
static char *extract_string(const char *start, int length) {
    // Strip surrounding quotes
    const char *s   = start + 1;
    int         len = length - 2;
    char       *out = malloc(len + 1);
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

static int has_interpolation(const char *s) {
    while (*s) { if (*s == '{') return 1; s++; }
    return 0;
}

// ── Primary expressions ───────────────────────────────────────────────────────
static void primary(Compiler *c) {
    // Number
    if (match_tok(c, TOK_NUMBER)) {
        int idx = chunk_add_const(c->chunk, alz_number(c->previous.num_val));
        emit(c, OP_PUSH_NUM, idx);
        return;
    }

    // String
    if (match_tok(c, TOK_STRING)) {
        char *s = extract_string(c->previous.start, c->previous.length);
        int idx = chunk_add_const(c->chunk, alz_string(s));
        // Mark interpolated strings with a special flag in arg
        // OP_PUSH_STR arg: 0 = plain, 1 = interpolated
        if (has_interpolation(s)) {
            // Use a separate opcode to push interpolated string (resolved at runtime)
            emit(c, OP_PUSH_STR, idx);
            // Tag the instruction so VM knows to interpolate
            c->chunk->code[c->chunk->count - 1].arg = idx | (1 << 24);
        } else {
            emit(c, OP_PUSH_STR, idx);
        }
        free(s);
        return;
    }

    // Boolean
    if (match_tok(c, TOK_TRUE))  { emit(c, OP_PUSH_BOOL, 1); return; }
    if (match_tok(c, TOK_FALSE)) { emit(c, OP_PUSH_BOOL, 0); return; }
    if (match_tok(c, TOK_NULL))  { emit(c, OP_PUSH_NULL, 0); return; }

    // Grouped expression
    if (match_tok(c, TOK_LPAREN)) {
        expression(c);
        consume(c, TOK_RPAREN, "Expected ')' after expression");
        return;
    }

    // List literal: [a, b, c]
    if (match_tok(c, TOK_LBRACKET)) {
        int count = 0;
        skip_newlines(c);
        if (!check(c, TOK_RBRACKET)) {
            do {
                skip_newlines(c);
                expression(c);
                count++;
                skip_newlines(c);
            } while (match_tok(c, TOK_COMMA));
        }
        skip_newlines(c);
        consume(c, TOK_RBRACKET, "Expected ']' after list");
        emit(c, OP_MAKE_LIST, count);
        return;
    }

    // Object literal: { key: val, ... }
    if (match_tok(c, TOK_LBRACE)) {
        int count = 0;
        skip_newlines(c);
        if (!check(c, TOK_RBRACE)) {
            do {
                skip_newlines(c);
                if (check(c, TOK_RBRACE)) break;
                // key must be ident or string
                if (check(c, TOK_IDENT) || check(c, TOK_STRING)) {
                    advance_tok(c);
                    char *key;
                    if (c->previous.type == TOK_STRING)
                        key = extract_string(c->previous.start, c->previous.length);
                    else
                        key = strndup(c->previous.start, c->previous.length);
                    int ki = chunk_add_const(c->chunk, alz_string(key));
                    free(key);
                    emit(c, OP_PUSH_STR, ki);
                    // Accept both : and = as separator
                    if (!match_tok(c, TOK_COLON) && !match_tok(c, TOK_ASSIGN)) {
                        fprintf(stderr, "[line %d] Expected ':' or '=' after object key\n", c->current.line);
                        c->had_error = 1;
                    }
                    expression(c);
                    count++;
                } else {
                    break;
                }
                skip_newlines(c);
            } while (match_tok(c, TOK_COMMA));
        }
        skip_newlines(c);
        consume(c, TOK_RBRACE, "Expected '}' after object");
        emit(c, OP_MAKE_OBJ, count);
        return;
    }

    // ask "prompt"  →  push prompt, OP_ASK
    if (match_tok(c, TOK_ASK)) {
        if (check(c, TOK_STRING)) {
            advance_tok(c);
            char *s = extract_string(c->previous.start, c->previous.length);
            int idx = chunk_add_const(c->chunk, alz_string(s));
            free(s);
            emit(c, OP_PUSH_STR, idx);
        } else {
            emit(c, OP_PUSH_STR, chunk_add_const(c->chunk, alz_string("")));
        }
        emit(c, OP_ASK, 0);
        return;
    }

    // Identifier — variable load or function call
    if (match_tok(c, TOK_IDENT)) {
        char *name = strndup(c->previous.start, c->previous.length);

        // Method/property chain: name.prop or name.method(args)
        if (check(c, TOK_DOT)) {
            int ni = chunk_add_name(c->chunk, name);
            emit(c, OP_LOAD, ni);
            free(name);

            while (match_tok(c, TOK_DOT)) {
                // Allow any token as property name after dot (including keywords)
                // This handles: file.delete, date.string, etc.
                if (c->current.type == TOK_IDENT ||
                    c->current.type >= TOK_PRINT) { // keywords
                    advance_tok(c);
                } else {
                    fprintf(stderr, "[line %d] Expected property name after '.'\n",
                        c->current.line);
                    c->had_error = 1;
                    break;
                }
                char *prop = strndup(c->previous.start, c->previous.length);
                int pi = chunk_add_name(c->chunk, prop);

                if (match_tok(c, TOK_LPAREN)) {
                    // method call — args on stack, then obj, then CALL
                    // For now emit GET_PROP then handle as function
                    emit(c, OP_GET_PROP, pi);
                    int argc = 0;
                    if (!check(c, TOK_RPAREN)) {
                        do { expression(c); argc++; } while (match_tok(c, TOK_COMMA));
                    }
                    consume(c, TOK_RPAREN, "Expected ')'");
                    emit(c, OP_CALL, argc);
                } else {
                    emit(c, OP_GET_PROP, pi);
                }
                free(prop);
            }

            // Index access after chain: obj.list[0]
            if (match_tok(c, TOK_LBRACKET)) {
                expression(c);
                consume(c, TOK_RBRACKET, "Expected ']'");
                emit(c, OP_GET_INDEX, 0);
            }
            return;
        }

        // Index access: name[idx]
        if (match_tok(c, TOK_LBRACKET)) {
            int ni = chunk_add_name(c->chunk, name);
            free(name);
            emit(c, OP_LOAD, ni);
            expression(c);
            consume(c, TOK_RBRACKET, "Expected ']'");
            emit(c, OP_GET_INDEX, 0);
            return;
        }

        // Function call: name(args)
        if (match_tok(c, TOK_LPAREN)) {
            int ni = chunk_add_name(c->chunk, name);
            free(name);
            emit(c, OP_LOAD, ni);
            int argc = 0;
            if (!check(c, TOK_RPAREN)) {
                do { expression(c); argc++; } while (match_tok(c, TOK_COMMA));
            }
            consume(c, TOK_RPAREN, "Expected ')'");
            emit(c, OP_CALL, argc);
            return;
        }

        // Plain variable load
        int ni = chunk_add_name(c->chunk, name);
        free(name);
        emit(c, OP_LOAD, ni);
        return;
    }

    fprintf(stderr, "[line %d] Unexpected token in expression: '%.*s'\n",
        c->current.line, c->current.length, c->current.start);
    c->had_error = 1;
    advance_tok(c); // recover
}

// ── Unary ─────────────────────────────────────────────────────────────────────
static void unary(Compiler *c) {
    if (match_tok(c, TOK_MINUS)) { unary(c); emit(c, OP_NEG, 0); return; }
    if (match_tok(c, TOK_NOT) || match_tok(c, TOK_NOT_KW)) { unary(c); emit(c, OP_NOT, 0); return; }
    primary(c);
}

// ── Binary (precedence climbing) ──────────────────────────────────────────────
typedef enum { PREC_NONE, PREC_OR, PREC_AND, PREC_EQ, PREC_CMP, PREC_ADD, PREC_MUL, PREC_POW } Prec;

static Prec get_prec(TokenType t) {
    switch (t) {
        case TOK_OR_KW:  return PREC_OR;
        case TOK_AND_KW: return PREC_AND;
        case TOK_IS: case TOK_ISNT: case TOK_EQ: case TOK_NEQ: return PREC_EQ;
        case TOK_LT: case TOK_LTE: case TOK_GT: case TOK_GTE:  return PREC_CMP;
        case TOK_PLUS: case TOK_MINUS:   return PREC_ADD;
        case TOK_STAR: case TOK_SLASH: case TOK_PERCENT: return PREC_MUL;
        case TOK_CARET: return PREC_POW;
        default: return PREC_NONE;
    }
}

static void parse_prec(Compiler *c, Prec min_prec) {
    unary(c);
    while (get_prec(c->current.type) >= min_prec && get_prec(c->current.type) > PREC_NONE) {
        TokenType op = c->current.type;
        advance_tok(c);
        parse_prec(c, get_prec(op) + 1);
        switch (op) {
            case TOK_PLUS:    emit(c, OP_ADD, 0); break;
            case TOK_MINUS:   emit(c, OP_SUB, 0); break;
            case TOK_STAR:    emit(c, OP_MUL, 0); break;
            case TOK_SLASH:   emit(c, OP_DIV, 0); break;
            case TOK_PERCENT: emit(c, OP_MOD, 0); break;
            case TOK_CARET:   emit(c, OP_POW, 0); break;
            case TOK_EQ: case TOK_IS:   emit(c, OP_EQ,  0); break;
            case TOK_NEQ: case TOK_ISNT: emit(c, OP_NEQ, 0); break;
            case TOK_LT:      emit(c, OP_LT,  0); break;
            case TOK_LTE:     emit(c, OP_LTE, 0); break;
            case TOK_GT:      emit(c, OP_GT,  0); break;
            case TOK_GTE:     emit(c, OP_GTE, 0); break;
            case TOK_AND_KW:  emit(c, OP_AND, 0); break;
            case TOK_OR_KW:   emit(c, OP_OR,  0); break;
            default: break;
        }
    }
}

static void expression(Compiler *c) {
    parse_prec(c, PREC_OR);
}

// ── Block (indented body) ─────────────────────────────────────────────────────
static void block(Compiler *c) {
    consume(c, TOK_COLON,  "Expected ':' before block");
    skip_newlines(c);
    consume(c, TOK_INDENT, "Expected indented block");
    skip_newlines(c);
    while (!check(c, TOK_DEDENT) && !check(c, TOK_EOF)) {
        statement(c);
        skip_newlines(c);
    }
    match_tok(c, TOK_DEDENT);
}

// ── Statements ────────────────────────────────────────────────────────────────
static void stmt_print(Compiler *c) {
    expression(c);
    // If top constant is a string with {}, use PRINT_FMT
    // VM handles interpolation on OP_PRINT too via the stack value
    emit(c, OP_PRINT, 0);
}

static void stmt_if(Compiler *c) {
    expression(c);
    int jump_false = emit_jump(c, OP_JUMP_IF_FALSE);

    block(c);
    skip_newlines(c);

    skip_newlines(c);

    // else if / elif
    if (check(c, TOK_ELIF)) {
        int jump_end = emit_jump(c, OP_JUMP);
        patch_jump(c, jump_false);
        advance_tok(c); // consume elif
        stmt_if(c);
        patch_jump(c, jump_end);
        return;
    }

    // else
    if (check(c, TOK_ELSE)) {
        advance_tok(c); // consume else
        // Check for "else if"
        skip_newlines(c);
        if (check(c, TOK_IF)) {
            int jump_end = emit_jump(c, OP_JUMP);
            patch_jump(c, jump_false);
            advance_tok(c); // consume if
            stmt_if(c);
            patch_jump(c, jump_end);
            return;
        }
        // plain else:
        int jump_end = emit_jump(c, OP_JUMP);
        patch_jump(c, jump_false);
        // consume the colon directly here
        consume(c, TOK_COLON, "Expected ':' after else");
        skip_newlines(c);
        consume(c, TOK_INDENT, "Expected indented block after else");
        skip_newlines(c);
        while (!check(c, TOK_DEDENT) && !check(c, TOK_EOF)) {
            statement(c);
            skip_newlines(c);
        }
        match_tok(c, TOK_DEDENT);
        patch_jump(c, jump_end);
        return;
    }

    patch_jump(c, jump_false);
}

static void stmt_while(Compiler *c) {
    int loop_start = (int)c->chunk->count;
    expression(c);
    int exit_jump = emit_jump(c, OP_JUMP_IF_FALSE);
    block(c);
    emit(c, OP_LOOP, loop_start);
    patch_jump(c, exit_jump);
}

static void stmt_each(Compiler *c) {
    // each item in list:
    consume(c, TOK_IDENT, "Expected loop variable after 'each'");
    char *var = strndup(c->previous.start, c->previous.length);
    consume(c, TOK_IN, "Expected 'in' after loop variable");
    expression(c);  // pushes the iterable

    // Store list in hidden var, index in hidden var
    char idx_name[64], list_name[64];
    snprintf(idx_name,  sizeof(idx_name),  "__idx_%s",  var);
    snprintf(list_name, sizeof(list_name), "__list_%s", var);

    int li = chunk_add_name(c->chunk, list_name);
    int ii = chunk_add_name(c->chunk, idx_name);
    int vi = chunk_add_name(c->chunk, var);

    emit(c, OP_STORE, li);   // store list
    int zero = chunk_add_const(c->chunk, alz_number(0));
    int one  = chunk_add_const(c->chunk, alz_number(1));
    emit(c, OP_PUSH_NUM, zero);
    emit(c, OP_STORE, ii);   // idx = 0

    // Loop start
    int loop_start = (int)c->chunk->count;

    // Check idx < list.length
    emit(c, OP_LOAD, ii);
    emit(c, OP_LOAD, li);
    emit(c, OP_LIST_LEN, 0);
    emit(c, OP_LT, 0);
    int exit_jump = emit_jump(c, OP_JUMP_IF_FALSE);

    // var = list[idx]
    emit(c, OP_LOAD, li);
    emit(c, OP_LOAD, ii);
    emit(c, OP_GET_INDEX, 0);
    emit(c, OP_STORE, vi);

    block(c);

    // idx++
    emit(c, OP_LOAD, ii);
    emit(c, OP_PUSH_NUM, one);
    emit(c, OP_ADD, 0);
    emit(c, OP_STORE, ii);

    emit(c, OP_LOOP, loop_start);
    patch_jump(c, exit_jump);

    free(var);
}

static void stmt_assign(Compiler *c, const char *name, int name_len) {
    char *n = strndup(name, name_len);
    int ni = chunk_add_name(c->chunk, n);
    free(n);

    // Check for += or -=
    if (match_tok(c, TOK_PLUS_EQ)) {
        emit(c, OP_LOAD, ni);
        expression(c);
        emit(c, OP_ADD, 0);
    } else if (match_tok(c, TOK_MINUS_EQ)) {
        emit(c, OP_LOAD, ni);
        expression(c);
        emit(c, OP_SUB, 0);
    } else {
        consume(c, TOK_ASSIGN, "Expected '=' in assignment");
        expression(c);
    }
    emit(c, OP_STORE, ni);
}

static void stmt_define(Compiler *c) {
    consume(c, TOK_IDENT, "Expected function name");
    char *fname = strndup(c->previous.start, c->previous.length);

    consume(c, TOK_LPAREN, "Expected '(' after function name");

    char *params[32];
    int   param_count = 0;
    if (!check(c, TOK_RPAREN)) {
        do {
            consume(c, TOK_IDENT, "Expected parameter name");
            params[param_count++] = strndup(c->previous.start, c->previous.length);
        } while (match_tok(c, TOK_COMMA));
    }
    consume(c, TOK_RPAREN, "Expected ')' after parameters");

    /* Compile body into a child chunk */
    Chunk    *fn_chunk = chunk_new(fname);
    Compiler  fn_c;
    memset(&fn_c, 0, sizeof(Compiler));
    fn_c.chunk        = fn_chunk;
    fn_c.loop_start   = -1;
    fn_c.lex          = c->lex;
    fn_c.current      = c->current;
    fn_c.previous     = c->previous;

    block(&fn_c);
    chunk_write(fn_chunk, OP_RETURN_NULL, 0, 0);

    /* Sync lexer back to parent */
    c->lex      = fn_c.lex;
    c->current  = fn_c.current;
    c->previous = fn_c.previous;
    if (fn_c.had_error) c->had_error = 1;

    /* Store function value as a constant and push it */
    AlzValue *fn_val = alz_function(fname, params, param_count, fn_chunk);
    int fn_idx = chunk_add_const(c->chunk, fn_val);
    emit(c, OP_PUSH_NUM, fn_idx);   /* reuse PUSH_NUM slot — VM checks type */
    /* Actually use a dedicated approach: store fn_val as string constant
       tagged with __fn__ prefix pointing to address */
    /* Simpler: use chunk constant pool directly */
    /* Override the last emit: use OP_PUSH_STR with special tag */
    /* Best approach: OP_PUSH_NUM already pushes the constant copy —
       but alz_copy on VAL_FUNCTION returns same pointer (shared).
       So this works: the fn_val is in the const pool, copy returns it. */

    /* Store the function in a variable */
    int ni = chunk_add_name(c->chunk, fname);
    emit(c, OP_STORE, ni);

    /* Free param name strings (fn_val copied them) */
    for (int i = 0; i < param_count; i++) free(params[i]);
    free(fname);
}

static void statement(Compiler *c) {
    skip_newlines(c);
    if (check(c, TOK_EOF)) return;

    // print / display
    if (match_tok(c, TOK_PRINT)) {
        stmt_print(c);
        return;
    }

    // if
    if (match_tok(c, TOK_IF)) {
        stmt_if(c);
        return;
    }

    // while
    if (match_tok(c, TOK_WHILE)) {
        stmt_while(c);
        return;
    }

    // each / for
    if (match_tok(c, TOK_EACH) || match_tok(c, TOK_FOR)) {
        stmt_each(c);
        return;
    }

    // define / fn
    if (match_tok(c, TOK_DEFINE)) {
        stmt_define(c);
        return;
    }

    // return
    if (match_tok(c, TOK_RETURN)) {
        if (!check(c, TOK_NEWLINE) && !check(c, TOK_EOF)) {
            expression(c);
            emit(c, OP_RETURN, 0);
        } else {
            emit(c, OP_RETURN_NULL, 0);
        }
        return;
    }

    // Assignment: ident = expr  OR  ident += expr
    if (check(c, TOK_IDENT)) {
        // Peek ahead — is next token = or += or -=?
        Lexer saved_lex = c->lex;
        Token saved_cur = c->current;
        Token saved_pre = c->previous;

        advance_tok(c);
        const char *name = c->previous.start;
        int         nlen = c->previous.length;

        if (check(c, TOK_ASSIGN) || check(c, TOK_PLUS_EQ) || check(c, TOK_MINUS_EQ)) {
            stmt_assign(c, name, nlen);
            return;
        }

        // Not an assignment — restore and parse as expression statement
        c->lex     = saved_lex;
        c->current = saved_cur;
        c->previous = saved_pre;
    }

    // Expression statement (function call, etc.)
    expression(c);
    emit(c, OP_POP, 0);  // discard result
}

// ── Entry points ──────────────────────────────────────────────────────────────
Chunk *compile(const char *source, const char *filename) {
    Compiler c;
    lexer_init(&c.lex, source);
    c.chunk        = chunk_new(filename ? filename : "script");
    c.had_error    = 0;
    c.panic_mode   = 0;
    c.loop_start   = -1;
    c.break_count  = 0;
    c.indent_depth = 0;

    advance_tok(&c);  // prime the pump
    skip_newlines(&c);

    while (!check(&c, TOK_EOF)) {
        statement(&c);
        skip_newlines(&c);
    }

    emit(&c, OP_HALT, 0);

    if (c.had_error) {
        chunk_free(c.chunk);
        return NULL;
    }
    return c.chunk;
}

Chunk *compile_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "alz: cannot open '%s'\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);

    Chunk *chunk = compile(source, path);
    free(source);
    return chunk;
}
