#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../include/chunk.h"

Chunk *chunk_new(const char *name) {
    Chunk *c = malloc(sizeof(Chunk));
    c->code        = NULL; c->count      = 0; c->cap        = 0;
    c->constants   = NULL; c->const_count = 0; c->const_cap  = 0;
    c->names       = NULL; c->name_count  = 0; c->name_cap   = 0;
    c->lines       = NULL;
    c->name        = strdup(name ? name : "script");
    return c;
}

void chunk_free(Chunk *chunk) {
    free(chunk->code);
    free(chunk->lines);
    for (size_t i = 0; i < chunk->const_count; i++) alz_free(chunk->constants[i]);
    free(chunk->constants);
    for (size_t i = 0; i < chunk->name_count; i++) free(chunk->names[i]);
    free(chunk->names);
    free(chunk->name);
    free(chunk);
}

int chunk_write(Chunk *chunk, OpCode op, int32_t arg, int line) {
    if (chunk->count >= chunk->cap) {
        chunk->cap  = chunk->cap < 8 ? 8 : chunk->cap * 2;
        chunk->code = realloc(chunk->code, chunk->cap * sizeof(Instruction));
        chunk->lines = realloc(chunk->lines, chunk->cap * sizeof(int));
    }
    chunk->code[chunk->count].op  = op;
    chunk->code[chunk->count].arg = arg;
    chunk->lines[chunk->count]    = line;
    return (int)chunk->count++;
}

int chunk_add_const(Chunk *chunk, AlzValue *val) {
    if (chunk->const_count >= chunk->const_cap) {
        chunk->const_cap   = chunk->const_cap < 8 ? 8 : chunk->const_cap * 2;
        chunk->constants   = realloc(chunk->constants, chunk->const_cap * sizeof(AlzValue *));
    }
    chunk->constants[chunk->const_count] = val;
    return (int)chunk->const_count++;
}

int chunk_add_name(Chunk *chunk, const char *name) {
    // Return existing index if already present
    for (size_t i = 0; i < chunk->name_count; i++)
        if (strcmp(chunk->names[i], name) == 0) return (int)i;
    // Add new
    if (chunk->name_count >= chunk->name_cap) {
        chunk->name_cap = chunk->name_cap < 8 ? 8 : chunk->name_cap * 2;
        chunk->names    = realloc(chunk->names, chunk->name_cap * sizeof(char *));
    }
    chunk->names[chunk->name_count] = strdup(name);
    return (int)chunk->name_count++;
}

// ── Disassembler (debug) ──────────────────────────────────────────────────────
static const char *op_name(OpCode op) {
    switch (op) {
        case OP_PUSH_NULL:     return "PUSH_NULL";
        case OP_PUSH_BOOL:     return "PUSH_BOOL";
        case OP_PUSH_NUM:      return "PUSH_NUM";
        case OP_PUSH_STR:      return "PUSH_STR";
        case OP_POP:           return "POP";
        case OP_STORE:         return "STORE";
        case OP_LOAD:          return "LOAD";
        case OP_STORE_GLOBAL:  return "STORE_GLOBAL";
        case OP_LOAD_GLOBAL:   return "LOAD_GLOBAL";
        case OP_ADD:           return "ADD";
        case OP_SUB:           return "SUB";
        case OP_MUL:           return "MUL";
        case OP_DIV:           return "DIV";
        case OP_MOD:           return "MOD";
        case OP_POW:           return "POW";
        case OP_NEG:           return "NEG";
        case OP_EQ:            return "EQ";
        case OP_NEQ:           return "NEQ";
        case OP_LT:            return "LT";
        case OP_LTE:           return "LTE";
        case OP_GT:            return "GT";
        case OP_GTE:           return "GTE";
        case OP_AND:           return "AND";
        case OP_OR:            return "OR";
        case OP_NOT:           return "NOT";
        case OP_CONCAT:        return "CONCAT";
        case OP_MAKE_LIST:     return "MAKE_LIST";
        case OP_MAKE_OBJ:      return "MAKE_OBJ";
        case OP_LIST_PUSH:     return "LIST_PUSH";
        case OP_LIST_POP:      return "LIST_POP";
        case OP_LIST_LEN:      return "LIST_LEN";
        case OP_GET_INDEX:     return "GET_INDEX";
        case OP_SET_INDEX:     return "SET_INDEX";
        case OP_GET_PROP:      return "GET_PROP";
        case OP_SET_PROP:      return "SET_PROP";
        case OP_JUMP:          return "JUMP";
        case OP_JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case OP_JUMP_IF_TRUE:  return "JUMP_IF_TRUE";
        case OP_LOOP:          return "LOOP";
        case OP_CALL:          return "CALL";
        case OP_RETURN:        return "RETURN";
        case OP_RETURN_NULL:   return "RETURN_NULL";
        case OP_PRINT:         return "PRINT";
        case OP_PRINT_FMT:     return "PRINT_FMT";
        case OP_ASK:           return "ASK";
        case OP_TO_NUM:        return "TO_NUM";
        case OP_TO_STR:        return "TO_STR";
        case OP_TO_BOOL:       return "TO_BOOL";
        case OP_FILE_READ:     return "FILE_READ";
        case OP_FILE_WRITE:    return "FILE_WRITE";
        case OP_FILE_EXISTS:   return "FILE_EXISTS";
        case OP_FILE_DELETE:   return "FILE_DELETE";
        case OP_HTTP_GET:      return "HTTP_GET";
        case OP_HTTP_POST:     return "HTTP_POST";
        case OP_SERVE:         return "SERVE";
        case OP_DB_SAVE:       return "DB_SAVE";
        case OP_DB_FIND:       return "DB_FIND";
        case OP_DB_ALL:        return "DB_ALL";
        case OP_DB_WHERE:      return "DB_WHERE";
        case OP_DB_UPDATE:     return "DB_UPDATE";
        case OP_DB_REMOVE:     return "DB_REMOVE";
        case OP_DB_COUNT:      return "DB_COUNT";
        case OP_DB_SEARCH:     return "DB_SEARCH";
        case OP_HALT:          return "HALT";
        case OP_NOP:           return "NOP";
        default:               return "UNKNOWN";
    }
}

void chunk_disasm(Chunk *chunk) {
    printf("=== %s (%zu instructions) ===\n", chunk->name, chunk->count);
    for (size_t i = 0; i < chunk->count; i++) {
        Instruction ins = chunk->code[i];
        printf("%04zu  line%-4d  %-18s", i, chunk->lines[i], op_name(ins.op));
        // Print argument details
        switch (ins.op) {
            case OP_PUSH_NUM: {
                char *s = alz_to_string(chunk->constants[ins.arg]);
                printf("  [%d] = %s", ins.arg, s);
                free(s);
                break;
            }
            case OP_PUSH_STR:
            case OP_PRINT_FMT: {
                int idx = ins.arg & 0xFFFFFF;
                if (idx >= 0 && (size_t)idx < chunk->const_count)
                    printf("  [%d] = \"%s\"", idx, chunk->constants[idx]->as.string);
                break;
            }
            case OP_STORE: case OP_LOAD:
            case OP_STORE_GLOBAL: case OP_LOAD_GLOBAL:
            case OP_GET_PROP: case OP_SET_PROP:
                if (ins.arg >= 0 && (size_t)ins.arg < chunk->name_count)
                    printf("  '%s'", chunk->names[ins.arg]);
                break;
            case OP_PUSH_BOOL:
                printf("  %s", ins.arg ? "true" : "false");
                break;
            case OP_JUMP: case OP_JUMP_IF_FALSE:
            case OP_JUMP_IF_TRUE: case OP_LOOP:
                printf("  → %d", ins.arg);
                break;
            case OP_CALL: case OP_MAKE_LIST: case OP_MAKE_OBJ:
                printf("  (%d)", ins.arg);
                break;
            default:
                if (ins.arg != 0) printf("  %d", ins.arg);
                break;
        }
        printf("\n");
    }
    printf("\n");
}
