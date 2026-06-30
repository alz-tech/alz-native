#ifndef ALZ_COMPILER_H
#define ALZ_COMPILER_H

#include "lexer.h"
#include "chunk.h"

// ── Compiler ──────────────────────────────────────────────────────────────────
// Single-pass, no AST — lexer feeds directly into bytecode emission
typedef struct {
    Lexer   lex;
    Token   current;
    Token   previous;
    Chunk  *chunk;

    // Error state
    int     had_error;
    int     panic_mode;
    char    error_msg[512];

    // Loop context (for break/continue)
    int     loop_start;        // ip to jump back to
    int     break_jumps[64];   // patches needed for break
    int     break_count;

    // Current indent depth
    int     indent_depth;
} Compiler;

// ── Compiler API ──────────────────────────────────────────────────────────────
// Compile source string into a Chunk
// Returns NULL on error (error printed to stderr)
Chunk *compile(const char *source, const char *filename);

// Compile a file
Chunk *compile_file(const char *path);

#endif
