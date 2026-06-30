#ifndef ALZ_CHUNK_H
#define ALZ_CHUNK_H

#include "opcode.h"
#include "value.h"
#include <stddef.h>

// ── Chunk — a compiled unit of bytecode ──────────────────────────────────────
// One chunk per function / script
typedef struct {
    // Instructions
    Instruction *code;
    size_t       count;
    size_t       cap;

    // Constant pool — strings and numbers referenced by instructions
    AlzValue   **constants;
    size_t       const_count;
    size_t       const_cap;

    // Name pool — variable/property names
    char       **names;
    size_t       name_count;
    size_t       name_cap;

    // Line info (for error messages)
    int         *lines;

    // Chunk name (function name or "script")
    char        *name;
} Chunk;

// ── Chunk API ─────────────────────────────────────────────────────────────────
Chunk *chunk_new(const char *name);
void   chunk_free(Chunk *chunk);

// Add an instruction, returns its index
int    chunk_write(Chunk *chunk, OpCode op, int32_t arg, int line);

// Add a constant to the pool, returns its index
int    chunk_add_const(Chunk *chunk, AlzValue *val);

// Add a name to the name pool, returns its index
// Returns existing index if name already exists
int    chunk_add_name(Chunk *chunk, const char *name);

// Debug: disassemble chunk to stdout
void   chunk_disasm(Chunk *chunk);

#endif
