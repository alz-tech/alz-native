#ifndef ALZ_VM_H
#define ALZ_VM_H

#include "chunk.h"
#include "value.h"
#include <stddef.h>

#define ALZ_STACK_MAX  1024
#define ALZ_FRAMES_MAX 256
#define ALZ_VARS_MAX   512

// ── Call frame — one per function call ───────────────────────────────────────
typedef struct {
    Chunk   *chunk;       // bytecode being executed
    size_t   ip;          // instruction pointer
    size_t   base;        // stack base (local var offset)
} CallFrame;

// ── Variable slot ─────────────────────────────────────────────────────────────
typedef struct {
    char     *name;
    AlzValue *val;
} VarSlot;

// ── VM ────────────────────────────────────────────────────────────────────────
typedef struct {
    // Execution stack
    AlzValue  *stack[ALZ_STACK_MAX];
    size_t     stack_top;

    // Call frames
    CallFrame  frames[ALZ_FRAMES_MAX];
    size_t     frame_count;

    // Variables (locals + globals flat for now)
    VarSlot    vars[ALZ_VARS_MAX];
    size_t     var_count;

    // Global scope (separate from locals)
    VarSlot    globals[ALZ_VARS_MAX];
    size_t     global_count;

    // Last error message
    char       error[512];
    int        had_error;
} VM;

// ── Result ────────────────────────────────────────────────────────────────────
typedef enum {
    VM_OK,
    VM_ERROR,
    VM_HALT,
} VMResult;

// ── VM API ────────────────────────────────────────────────────────────────────
VM      *vm_new();
void     vm_free(VM *vm);
VMResult vm_run(VM *vm, Chunk *chunk);

// Stack ops (used by compiler tests)
void     vm_push(VM *vm, AlzValue *val);
AlzValue *vm_pop(VM *vm);
AlzValue *vm_peek(VM *vm, int offset);  // peek without popping

// Variable ops
void      vm_set_var(VM *vm, const char *name, AlzValue *val);
AlzValue *vm_get_var(VM *vm, const char *name);
void      vm_set_global(VM *vm, const char *name, AlzValue *val);
AlzValue *vm_get_global(VM *vm, const char *name);

#endif
