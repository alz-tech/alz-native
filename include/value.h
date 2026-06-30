#ifndef ALZ_VALUE_H
#define ALZ_VALUE_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── strndup polyfill for Windows/MSVC ─────────────────────────────────── */
#ifdef _WIN32
#include <string.h>
#include <stdlib.h>
static inline char *strndup(const char *s, size_t n) {
    size_t len = strlen(s);
    if (len > n) len = n;
    char *out = (char*)malloc(len + 1);
    if (out) { memcpy(out, s, len); out[len] = '\0'; }
    return out;
}
#endif



typedef enum {
    VAL_NULL,
    VAL_BOOL,
    VAL_NUMBER,
    VAL_STRING,
    VAL_LIST,
    VAL_OBJECT,
    VAL_MODULE,   /* stdlib module sentinel — stores module name only */
    VAL_FUNCTION, /* compiled function — stores AlzFunc* */
} ValType;

typedef struct AlzValue  AlzValue;
typedef struct AlzList   AlzList;
typedef struct AlzObj    AlzObj;
typedef struct AlzFunc   AlzFunc;

struct AlzList {
    AlzValue **items;
    size_t     count;
    size_t     cap;
};

typedef struct { char *key; AlzValue *val; } AlzEntry;

struct AlzObj {
    AlzEntry *entries;
    size_t    count;
    size_t    cap;
};

struct AlzValue {
    ValType type;
    union {
        int      boolean;
        double   number;
        char    *string;   /* also used for VAL_MODULE (module name) */
        AlzList *list;
        AlzObj   *object;
        AlzFunc  *func;
    } as;
};

/* ── Function value ──────────────────────────────────────────────────────── */
struct AlzFunc {
    char   *name;
    char  **params;
    int     param_count;
    void  *chunk;    /* Chunk* — void* to avoid circular include */
};

AlzValue *alz_null();
AlzValue *alz_bool(int b);
AlzValue *alz_number(double n);
AlzValue *alz_string(const char *s);
AlzValue *alz_list();
AlzValue *alz_object();
AlzValue *alz_module(const char *name);
AlzValue *alz_function(const char *name, char **params, int nparams, void *chunk);

void      alz_list_push(AlzList *list, AlzValue *val);
AlzValue *alz_list_get(AlzList *list, int idx);
void      alz_obj_set(AlzObj *obj, const char *key, AlzValue *val);
AlzValue *alz_obj_get(AlzObj *obj, const char *key);

char     *alz_to_string(AlzValue *val);
int       alz_is_truthy(AlzValue *val);
AlzValue *alz_copy(AlzValue *val);
void      alz_free(AlzValue *val);
void      alz_print(AlzValue *val);

#endif
