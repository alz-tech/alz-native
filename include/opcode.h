#ifndef ALZ_OPCODE_H
#define ALZ_OPCODE_H

#include <stdint.h>

// ── Bytecode Instruction Set ──────────────────────────────────────────────────
// Stack-based VM — simple, fast, easy to debug
typedef enum {
    // ── Stack ops
    OP_PUSH_NULL,       // push null
    OP_PUSH_BOOL,       // push bool   | arg: 0/1
    OP_PUSH_NUM,        // push number | arg: constant pool index
    OP_PUSH_STR,        // push string | arg: constant pool index
    OP_POP,             // discard top of stack

    // ── Variables
    OP_STORE,           // pop → variable  | arg: name index
    OP_LOAD,            // variable → push | arg: name index
    OP_STORE_GLOBAL,    // pop → global    | arg: name index
    OP_LOAD_GLOBAL,     // global → push   | arg: name index

    // ── Arithmetic
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_POW,
    OP_NEG,             // negate top

    // ── Comparison
    OP_EQ,              // ==
    OP_NEQ,             // !=
    OP_LT,              // <
    OP_LTE,             // <=
    OP_GT,              // >
    OP_GTE,             // >=

    // ── Logic
    OP_AND,
    OP_OR,
    OP_NOT,

    // ── String
    OP_CONCAT,          // join two strings
    OP_STR_LEN,         // string.length
    OP_STR_UPPER,
    OP_STR_LOWER,
    OP_STR_TRIM,
    OP_STR_SPLIT,       // split(sep)
    OP_STR_REPLACE,     // replace(old, new)
    OP_STR_CONTAINS,    // contains(sub)
    OP_STR_STARTS,      // startsWith(sub)
    OP_STR_ENDS,        // endsWith(sub)

    // ── Collections
    OP_MAKE_LIST,       // arg: item count — pops N items, makes list
    OP_MAKE_OBJ,        // arg: pair count — pops N key/val pairs, makes object
    OP_LIST_PUSH,       // list.add(val)
    OP_LIST_POP,        // list.pop()
    OP_LIST_LEN,        // list.length
    OP_GET_INDEX,       // list[i] or obj[key]
    OP_SET_INDEX,       // list[i] = val or obj[key] = val
    OP_GET_PROP,        // obj.key  | arg: name index
    OP_SET_PROP,        // obj.key = val | arg: name index

    // ── Control flow
    OP_JUMP,            // unconditional | arg: offset
    OP_JUMP_IF_FALSE,   // pop, jump if falsy | arg: offset
    OP_JUMP_IF_TRUE,    // pop, jump if truthy | arg: offset (for 'or' short-circuit)
    OP_LOOP,            // jump back | arg: offset (negative)

    // ── Functions
    OP_CALL,            // call function | arg: arg count
    OP_RETURN,          // return top of stack
    OP_RETURN_NULL,     // return null

    // ── I/O
    OP_PRINT,           // print top of stack + newline
    OP_PRINT_FMT,       // print with {interpolation} | arg: string index
    OP_ASK,             // read line from stdin → push string

    // ── Type conversion
    OP_TO_NUM,          // to number
    OP_TO_STR,          // to string
    OP_TO_BOOL,         // to bool

    // ── File I/O
    OP_FILE_READ,       // read file → string
    OP_FILE_WRITE,      // write string to file
    OP_FILE_APPEND,     // append string to file
    OP_FILE_EXISTS,     // file exists? → bool
    OP_FILE_DELETE,     // delete file

    // ── HTTP (Phase 3 — compiled in but no-op until Phase 3)
    OP_HTTP_GET,        // fetch url → response
    OP_HTTP_POST,       // fetch url body → response
    OP_SERVE,           // start HTTP server | arg: port

    // ── Database (Phase 4)
    OP_DB_SAVE,         // db.save(table, obj)
    OP_DB_FIND,         // db.find(table, filter)
    OP_DB_ALL,          // db.all(table)
    OP_DB_WHERE,        // db.where(table, filter)
    OP_DB_UPDATE,       // db.update(table, filter, data)
    OP_DB_REMOVE,       // db.remove(table, filter)
    OP_DB_COUNT,        // db.count(table)
    OP_DB_SEARCH,       // db.search(table, query)

    // ── System
    OP_HALT,            // stop execution
    OP_NOP,             // no-op
} OpCode;

// ── Instruction (fixed width — 8 bytes) ──────────────────────────────────────
typedef struct {
    OpCode   op;
    int32_t  arg;    // optional argument (index, offset, count)
} Instruction;

#endif
