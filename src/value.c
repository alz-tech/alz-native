#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../include/value.h"

// ── Helpers ───────────────────────────────────────────────────────────────────
static AlzValue *new_val(ValType type) {
    AlzValue *v = malloc(sizeof(AlzValue));
    if (!v) { fprintf(stderr, "alz: out of memory\n"); exit(1); }
    v->type = type;
    return v;
}

// ── Constructors ──────────────────────────────────────────────────────────────
AlzValue *alz_null() {
    return new_val(VAL_NULL);
}

AlzValue *alz_bool(int b) {
    AlzValue *v = new_val(VAL_BOOL);
    v->as.boolean = b ? 1 : 0;
    return v;
}

AlzValue *alz_number(double n) {
    AlzValue *v = new_val(VAL_NUMBER);
    v->as.number = n;
    return v;
}

AlzValue *alz_string(const char *s) {
    AlzValue *v = new_val(VAL_STRING);
    v->as.string = strdup(s ? s : "");
    return v;
}

AlzValue *alz_list() {
    AlzValue *v = new_val(VAL_LIST);
    AlzList  *l = malloc(sizeof(AlzList));
    l->items = NULL;
    l->count = 0;
    l->cap   = 0;
    v->as.list = l;
    return v;
}

AlzValue *alz_object() {
    AlzValue *v = new_val(VAL_OBJECT);
    AlzObj   *o = malloc(sizeof(AlzObj));
    o->entries = NULL;
    o->count   = 0;
    o->cap     = 0;
    v->as.object = o;
    return v;
}

// ── List ops ──────────────────────────────────────────────────────────────────
void alz_list_push(AlzList *list, AlzValue *val) {
    if (list->count >= list->cap) {
        list->cap   = list->cap < 8 ? 8 : list->cap * 2;
        list->items = realloc(list->items, list->cap * sizeof(AlzValue *));
    }
    list->items[list->count++] = val;
}

AlzValue *alz_list_get(AlzList *list, int idx) {
    if (idx < 0) idx = (int)list->count + idx;  // negative index
    if (idx < 0 || (size_t)idx >= list->count) return alz_null();
    return list->items[idx];
}

// ── Object ops ────────────────────────────────────────────────────────────────
void alz_obj_set(AlzObj *obj, const char *key, AlzValue *val) {
    // Update existing key
    for (size_t i = 0; i < obj->count; i++) {
        if (strcmp(obj->entries[i].key, key) == 0) {
            alz_free(obj->entries[i].val);
            obj->entries[i].val = val;
            return;
        }
    }
    // New key
    if (obj->count >= obj->cap) {
        obj->cap     = obj->cap < 8 ? 8 : obj->cap * 2;
        obj->entries = realloc(obj->entries, obj->cap * sizeof(AlzEntry));
    }
    obj->entries[obj->count].key = strdup(key);
    obj->entries[obj->count].val = val;
    obj->count++;
}

AlzValue *alz_obj_get(AlzObj *obj, const char *key) {
    for (size_t i = 0; i < obj->count; i++) {
        if (strcmp(obj->entries[i].key, key) == 0)
            return obj->entries[i].val;
    }
    return alz_null();
}

// ── Truthy check ──────────────────────────────────────────────────────────────
int alz_is_truthy(AlzValue *val) {
    if (!val) return 0;
    switch (val->type) {
        case VAL_NULL:   return 0;
        case VAL_BOOL:   return val->as.boolean;
        case VAL_NUMBER: return val->as.number != 0.0;
        case VAL_STRING:
        case VAL_MODULE:   return val->as.string && val->as.string[0] != '\0';
        case VAL_FUNCTION: return 1;
        case VAL_LIST:   return val->as.list && val->as.list->count > 0;
        case VAL_OBJECT: return val->as.object && val->as.object->count > 0;
    }
    return 0;
}

// ── To string ─────────────────────────────────────────────────────────────────
char *alz_to_string(AlzValue *val) {
    if (!val) return strdup("null");
    char buf[64];
    switch (val->type) {
        case VAL_NULL:   return strdup("null");
        case VAL_BOOL:   return strdup(val->as.boolean ? "true" : "false");
        case VAL_NUMBER:
            if (val->as.number == (long long)val->as.number)
                snprintf(buf, sizeof(buf), "%lld", (long long)val->as.number);
            else
                snprintf(buf, sizeof(buf), "%g", val->as.number);
            return strdup(buf);
        case VAL_STRING: return strdup(val->as.string);
        case VAL_MODULE:   return strdup(val->as.string);
        case VAL_FUNCTION: { char b[64]; snprintf(b,sizeof(b),"<fn %s>",val->as.func->name); return strdup(b); }
        case VAL_LIST: {
            // Basic JSON-like output
            size_t len = 2;
            for (size_t i = 0; i < val->as.list->count; i++) {
                char *s = alz_to_string(val->as.list->items[i]);
                len += strlen(s) + 2;
                free(s);
            }
            char *out = malloc(len + 64);
            out[0] = '['; out[1] = '\0';
            for (size_t i = 0; i < val->as.list->count; i++) {
                char *s = alz_to_string(val->as.list->items[i]);
                if (i > 0) strcat(out, ", ");
                strcat(out, s);
                free(s);
            }
            strcat(out, "]");
            return out;
        }
        case VAL_OBJECT: {
            size_t len = 2;
            for (size_t i = 0; i < val->as.object->count; i++) {
                char *s = alz_to_string(val->as.object->entries[i].val);
                len += strlen(val->as.object->entries[i].key) + strlen(s) + 6;
                free(s);
            }
            char *out = malloc(len + 64);
            out[0] = '{'; out[1] = '\0';
            for (size_t i = 0; i < val->as.object->count; i++) {
                char *s = alz_to_string(val->as.object->entries[i].val);
                if (i > 0) strcat(out, ", ");
                strcat(out, val->as.object->entries[i].key);
                strcat(out, ": ");
                strcat(out, s);
                free(s);
            }
            strcat(out, "}");
            return out;
        }
    }
    return strdup("null");
}

// ── Print ─────────────────────────────────────────────────────────────────────
void alz_print(AlzValue *val) {
    char *s = alz_to_string(val);
    printf("%s", s);
    free(s);
}

// ── Copy ──────────────────────────────────────────────────────────────────────
AlzValue *alz_copy(AlzValue *val) {
    if (!val) return alz_null();
    switch (val->type) {
        case VAL_NULL:   return alz_null();
        case VAL_BOOL:   return alz_bool(val->as.boolean);
        case VAL_NUMBER: return alz_number(val->as.number);
        case VAL_STRING: return alz_string(val->as.string);
        case VAL_MODULE:   return alz_module(val->as.string);
        case VAL_FUNCTION: {
            /* Shallow copy — new AlzValue wrapper, shared AlzFunc */
            AlzValue *copy = malloc(sizeof(AlzValue));
            copy->type     = VAL_FUNCTION;
            copy->as.func  = val->as.func;  /* shared pointer */
            return copy;
        }
        case VAL_LIST: {
            // Deep copy list
            AlzValue *copy = alz_list();
            for (size_t i = 0; i < val->as.list->count; i++)
                alz_list_push(copy->as.list, alz_copy(val->as.list->items[i]));
            return copy;
        }
        case VAL_OBJECT: {
            // Deep copy object
            AlzValue *copy = alz_object();
            for (size_t i = 0; i < val->as.object->count; i++)
                alz_obj_set(copy->as.object,
                    val->as.object->entries[i].key,
                    alz_copy(val->as.object->entries[i].val));
            return copy;
        }
    }
    return alz_null();
}

// ── Free ──────────────────────────────────────────────────────────────────────
void alz_free(AlzValue *val) {
    if (!val) return;
    switch (val->type) {
        case VAL_STRING:
        case VAL_MODULE:
            free(val->as.string);
            break;
        case VAL_FUNCTION:
            /* Only free the wrapper — AlzFunc is shared across copies */
            /* The original AlzFunc (in const pool) owns its memory */
            break;
        case VAL_LIST:
            for (size_t i = 0; i < val->as.list->count; i++)
                alz_free(val->as.list->items[i]);
            free(val->as.list->items);
            free(val->as.list);
            break;
        case VAL_OBJECT:
            for (size_t i = 0; i < val->as.object->count; i++) {
                free(val->as.object->entries[i].key);
                alz_free(val->as.object->entries[i].val);
            }
            free(val->as.object->entries);
            free(val->as.object);
            break;
        default: break;
    }
    free(val);
}

AlzValue *alz_module(const char *name) {
    AlzValue *v = malloc(sizeof(AlzValue));
    v->type      = VAL_MODULE;
    v->as.string = strdup(name ? name : "");
    return v;
}

AlzValue *alz_function(const char *name, char **params, int nparams, void *chunk) {
    AlzValue *v = malloc(sizeof(AlzValue));
    v->type      = VAL_FUNCTION;
    AlzFunc *fn  = malloc(sizeof(AlzFunc));
    fn->name     = strdup(name ? name : "anonymous");
    fn->params   = malloc(nparams * sizeof(char *));
    fn->param_count = nparams;
    for (int i = 0; i < nparams; i++)
        fn->params[i] = strdup(params[i]);
    fn->chunk    = chunk;
    v->as.func   = fn;
    return v;
}
