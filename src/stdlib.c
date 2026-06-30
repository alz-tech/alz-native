#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include "../include/alz_stdlib.h"

/* Fallback strndup for Windows/MSVC */
#if defined(_WIN32) || !defined(_GNU_SOURCE)
static inline char *alz_strndup(const char *s, size_t n) {
    char *out = malloc(n + 1);
    if (out) {
        strncpy(out, s, n);
        out[n] = '\0';
    }
    return out;
}
#endif

static AlzValue *stdlib_db_dispatch(VM *vm, const char *method, AlzValue **args, int argc);
static AlzValue *stdlib_http_dispatch(VM *vm, const char *method, AlzValue **args, int argc);

/* FILE I/O */

AlzValue *stdlib_file_read(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return alz_null();
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    AlzValue *v = alz_string(buf);
    free(buf);
    return v;
}

AlzValue *stdlib_file_write(const char *path, const char *data) {
    FILE *f = fopen(path, "w");
    if (!f) return alz_bool(0);
    fputs(data, f);
    fclose(f);
    return alz_bool(1);
}

AlzValue *stdlib_file_append(const char *path, const char *data) {
    FILE *f = fopen(path, "a");
    if (!f) return alz_bool(0);
    fputs(data, f);
    fclose(f);
    return alz_bool(1);
}

AlzValue *stdlib_file_exists(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return alz_bool(0);
    fclose(f);
    return alz_bool(1);
}

AlzValue *stdlib_file_delete(const char *path) {
    return alz_bool(remove(path) == 0);
}

/* STRING OPS */

AlzValue *stdlib_str_upper(const char *s) {
    char *out = strdup(s);
    for (int i = 0; out[i]; i++) out[i] = toupper((unsigned char)out[i]);
    AlzValue *v = alz_string(out);
    free(out);
    return v;
}

AlzValue *stdlib_str_lower(const char *s) {
    char *out = strdup(s);
    for (int i = 0; out[i]; i++) out[i] = tolower((unsigned char)out[i]);
    AlzValue *v = alz_string(out);
    free(out);
    return v;
}

AlzValue *stdlib_str_trim(const char *s) {
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return alz_string("");
    const char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    char *out = strndup(s, end - s + 1);
    AlzValue *v = alz_string(out);
    free(out);
    return v;
}

AlzValue *stdlib_str_split(const char *s, const char *sep) {
    AlzValue *list = alz_list();
    if (!sep || *sep == '\0') {
        for (const char *p = s; *p; p++) {
            char ch[2] = {*p, '\0'};
            alz_list_push(list->as.list, alz_string(ch));
        }
        return list;
    }
    size_t sep_len = strlen(sep);
    const char *p = s;
    while (*p) {
        const char *found = strstr(p, sep);
        if (!found) { alz_list_push(list->as.list, alz_string(p)); break; }
        char *part = strndup(p, found - p);
        alz_list_push(list->as.list, alz_string(part));
        free(part);
        p = found + sep_len;
    }
    return list;
}

AlzValue *stdlib_str_replace(const char *s, const char *from, const char *to) {
    if (!from || *from == '\0') return alz_string(s);
    size_t from_len = strlen(from);
    size_t to_len   = to ? strlen(to) : 0;
    size_t out_cap  = strlen(s) * 2 + 64;
    char  *out      = malloc(out_cap);
    size_t out_len  = 0;
    const char *p   = s;
    out[0] = '\0';
    while (*p) {
        const char *found = strstr(p, from);
        if (!found) {
            size_t rest = strlen(p);
            while (out_len + rest + 2 >= out_cap) { out_cap *= 2; out = realloc(out, out_cap); }
            memcpy(out + out_len, p, rest);
            out_len += rest;
            break;
        }
        size_t prefix = found - p;
        while (out_len + prefix + to_len + 2 >= out_cap) { out_cap *= 2; out = realloc(out, out_cap); }
        memcpy(out + out_len, p, prefix);
        out_len += prefix;
        if (to) { memcpy(out + out_len, to, to_len); out_len += to_len; }
        p = found + from_len;
    }
    out[out_len] = '\0';
    AlzValue *v = alz_string(out);
    free(out);
    return v;
}

AlzValue *stdlib_str_contains(const char *s, const char *sub) {
    return alz_bool(strstr(s, sub) != NULL);
}

AlzValue *stdlib_str_starts(const char *s, const char *sub) {
    return alz_bool(strncmp(s, sub, strlen(sub)) == 0);
}

AlzValue *stdlib_str_ends(const char *s, const char *sub) {
    size_t slen = strlen(s), sublen = strlen(sub);
    if (sublen > slen) return alz_bool(0);
    return alz_bool(strcmp(s + slen - sublen, sub) == 0);
}

AlzValue *stdlib_str_len(const char *s) {
    return alz_number((double)strlen(s));
}

AlzValue *stdlib_str_slice(const char *s, int start, int end) {
    int len = (int)strlen(s);
    if (start < 0) start = len + start;
    if (end   < 0) end   = len + end;
    if (start < 0) start = 0;
    if (end > len) end = len;
    if (start >= end) return alz_string("");
    char *out = strndup(s + start, end - start);
    AlzValue *v = alz_string(out);
    free(out);
    return v;
}

AlzValue *stdlib_str_reverse(const char *s) {
    size_t len = strlen(s);
    char  *out = malloc(len + 1);
    for (size_t i = 0; i < len; i++) out[i] = s[len - 1 - i];
    out[len] = '\0';
    AlzValue *v = alz_string(out);
    free(out);
    return v;
}

AlzValue *stdlib_str_repeat(const char *s, int n) {
    if (n <= 0) return alz_string("");
    size_t slen = strlen(s), total = slen * (size_t)n;
    char  *out  = malloc(total + 1);
    for (int i = 0; i < n; i++) memcpy(out + i * slen, s, slen);
    out[total] = '\0';
    AlzValue *v = alz_string(out);
    free(out);
    return v;
}

AlzValue *stdlib_str_index_of(const char *s, const char *sub) {
    const char *found = strstr(s, sub);
    return found ? alz_number((double)(found - s)) : alz_number(-1);
}

/* MATH */

AlzValue *stdlib_math_sqrt(double n)              { return alz_number(sqrt(n)); }
AlzValue *stdlib_math_floor(double n)             { return alz_number(floor(n)); }
AlzValue *stdlib_math_ceil(double n)              { return alz_number(ceil(n)); }
AlzValue *stdlib_math_round(double n)             { return alz_number(round(n)); }
AlzValue *stdlib_math_abs(double n)               { return alz_number(fabs(n)); }
AlzValue *stdlib_math_pow(double base, double e)  { return alz_number(pow(base, e)); }
AlzValue *stdlib_math_min(double a, double b)     { return alz_number(a < b ? a : b); }
AlzValue *stdlib_math_max(double a, double b)     { return alz_number(a > b ? a : b); }
AlzValue *stdlib_math_pi(void)                    { return alz_number(3.14159265358979323846); }
AlzValue *stdlib_math_random(void)                { return alz_number((double)rand() / 2147483648.0); }

/* DATE */

static struct tm *get_now(void) { time_t t = time(NULL); return localtime(&t); }

AlzValue *stdlib_date_now(void)    { return alz_number((double)time(NULL)); }
AlzValue *stdlib_date_year(void)   { return alz_number(get_now()->tm_year + 1900); }
AlzValue *stdlib_date_month(void)  { return alz_number(get_now()->tm_mon  + 1); }
AlzValue *stdlib_date_day(void)    { return alz_number(get_now()->tm_mday); }
AlzValue *stdlib_date_hour(void)   { return alz_number(get_now()->tm_hour); }
AlzValue *stdlib_date_minute(void) { return alz_number(get_now()->tm_min); }

AlzValue *stdlib_date_string(void) {
    char buf[32]; strftime(buf, sizeof(buf), "%Y-%m-%d", get_now()); return alz_string(buf);
}
AlzValue *stdlib_date_time(void) {
    char buf[32]; strftime(buf, sizeof(buf), "%H:%M:%S", get_now()); return alz_string(buf);
}

/* TYPE UTILS */

AlzValue *stdlib_type_of(AlzValue *val) {
    if (!val) return alz_string("null");
    switch (val->type) {
        case VAL_NULL:   return alz_string("null");
        case VAL_BOOL:   return alz_string("bool");
        case VAL_NUMBER: return alz_string("number");
        case VAL_STRING: return alz_string("string");
        case VAL_LIST:   return alz_string("list");
        case VAL_OBJECT: return alz_string("object");
        case VAL_MODULE:   return alz_string("module");
        case VAL_FUNCTION: return alz_string("function");
    }
    return alz_string("unknown");
}

AlzValue *stdlib_to_num(AlzValue *val) {
    if (!val) return alz_number(0);
    switch (val->type) {
        case VAL_NUMBER: return alz_copy(val);
        case VAL_BOOL:   return alz_number(val->as.boolean ? 1 : 0);
        case VAL_STRING: return alz_number(atof(val->as.string));
        default:         return alz_number(0);
    }
}

AlzValue *stdlib_to_str(AlzValue *val) {
    char *s = alz_to_string(val); AlzValue *v = alz_string(s); free(s); return v;
}

AlzValue *stdlib_to_bool(AlzValue *val) {
    return alz_bool(alz_is_truthy(val));
}

/* SYSTEM */

AlzValue *stdlib_env_get(const char *key) {
    const char *val = getenv(key);
    return val ? alz_string(val) : alz_null();
}

AlzValue *stdlib_sys_exit(int code) { exit(code); return alz_null(); }

/* DISPATCH */

AlzValue *stdlib_dispatch(VM *vm, const char *module, const char *method,
                          AlzValue **args, int argc)
{
    (void)vm;

    if (strcmp(module, "file") == 0) {
        if (strcmp(method, "read") == 0 && argc >= 1) {
            char *p = alz_to_string(args[0]);
            AlzValue *v = stdlib_file_read(p);
            free(p);
            return v;
        }
        if (strcmp(method, "write") == 0 && argc >= 2) {
            char *p = alz_to_string(args[0]);
            char *d = alz_to_string(args[1]);
            AlzValue *v = stdlib_file_write(p, d);
            free(p); free(d);
            return v;
        }
        if (strcmp(method, "append") == 0 && argc >= 2) {
            char *p = alz_to_string(args[0]);
            char *d = alz_to_string(args[1]);
            AlzValue *v = stdlib_file_append(p, d);
            free(p); free(d);
            return v;
        }
        if (strcmp(method, "exists") == 0 && argc >= 1) {
            char *p = alz_to_string(args[0]);
            AlzValue *v = stdlib_file_exists(p);
            free(p);
            return v;
        }
        if (strcmp(method, "delete") == 0 && argc >= 1) {
            char *p = alz_to_string(args[0]);
            AlzValue *v = stdlib_file_delete(p);
            free(p);
            return v;
        }
        return alz_null();
    }

    if (strcmp(module, "str") == 0) {
        if (argc < 1) return alz_null();
        char *s = alz_to_string(args[0]);
        AlzValue *v = NULL;
        if (strcmp(method, "upper") == 0) {
            v = stdlib_str_upper(s);
        } else if (strcmp(method, "lower") == 0) {
            v = stdlib_str_lower(s);
        } else if (strcmp(method, "trim") == 0) {
            v = stdlib_str_trim(s);
        } else if (strcmp(method, "reverse") == 0) {
            v = stdlib_str_reverse(s);
        } else if (strcmp(method, "len") == 0) {
            v = stdlib_str_len(s);
        } else if (strcmp(method, "split") == 0 && argc >= 2) {
            char *sep = alz_to_string(args[1]);
            v = stdlib_str_split(s, sep);
            free(sep);
        } else if (strcmp(method, "replace") == 0 && argc >= 3) {
            char *from = alz_to_string(args[1]);
            char *to   = alz_to_string(args[2]);
            v = stdlib_str_replace(s, from, to);
            free(from); free(to);
        } else if (strcmp(method, "contains") == 0 && argc >= 2) {
            char *sub = alz_to_string(args[1]);
            v = stdlib_str_contains(s, sub);
            free(sub);
        } else if (strcmp(method, "starts") == 0 && argc >= 2) {
            char *sub = alz_to_string(args[1]);
            v = stdlib_str_starts(s, sub);
            free(sub);
        } else if (strcmp(method, "ends") == 0 && argc >= 2) {
            char *sub = alz_to_string(args[1]);
            v = stdlib_str_ends(s, sub);
            free(sub);
        } else if (strcmp(method, "slice") == 0 && argc >= 3) {
            v = stdlib_str_slice(s, (int)args[1]->as.number, (int)args[2]->as.number);
        } else if (strcmp(method, "repeat") == 0 && argc >= 2) {
            v = stdlib_str_repeat(s, (int)args[1]->as.number);
        } else if (strcmp(method, "indexOf") == 0 && argc >= 2) {
            char *sub = alz_to_string(args[1]);
            v = stdlib_str_index_of(s, sub);
            free(sub);
        }
        free(s);
        AlzValue *ret = v ? v : alz_null();
        return ret;
    }

    if (strcmp(module, "math") == 0) {
        double a = (argc >= 1 && args[0]->type == VAL_NUMBER) ? args[0]->as.number : 0;
        double b = (argc >= 2 && args[1]->type == VAL_NUMBER) ? args[1]->as.number : 0;
        if      (strcmp(method, "sqrt")   == 0) return stdlib_math_sqrt(a);
        else if (strcmp(method, "floor")  == 0) return stdlib_math_floor(a);
        else if (strcmp(method, "ceil")   == 0) return stdlib_math_ceil(a);
        else if (strcmp(method, "round")  == 0) return stdlib_math_round(a);
        else if (strcmp(method, "abs")    == 0) return stdlib_math_abs(a);
        else if (strcmp(method, "pow")    == 0) return stdlib_math_pow(a, b);
        else if (strcmp(method, "min")    == 0) return stdlib_math_min(a, b);
        else if (strcmp(method, "max")    == 0) return stdlib_math_max(a, b);
        else if (strcmp(method, "random") == 0) return stdlib_math_random();
        else if (strcmp(method, "pi")     == 0) return stdlib_math_pi();
        return alz_null();
    }

    if (strcmp(module, "date") == 0) {
        if      (strcmp(method, "now")    == 0) return stdlib_date_now();
        else if (strcmp(method, "string") == 0) return stdlib_date_string();
        else if (strcmp(method, "time")   == 0) return stdlib_date_time();
        else if (strcmp(method, "year")   == 0) return stdlib_date_year();
        else if (strcmp(method, "month")  == 0) return stdlib_date_month();
        else if (strcmp(method, "day")    == 0) return stdlib_date_day();
        else if (strcmp(method, "hour")   == 0) return stdlib_date_hour();
        else if (strcmp(method, "minute") == 0) return stdlib_date_minute();
        return alz_null();
    }

    if (strcmp(module, "type") == 0) {
        if (argc < 1) return alz_null();
        if      (strcmp(method, "of")   == 0) return stdlib_type_of(args[0]);
        else if (strcmp(method, "num")  == 0) return stdlib_to_num(args[0]);
        else if (strcmp(method, "str")  == 0) return stdlib_to_str(args[0]);
        else if (strcmp(method, "bool") == 0) return stdlib_to_bool(args[0]);
        return alz_null();
    }

    if (strcmp(module, "env") == 0) {
        if (strcmp(method, "get") == 0 && argc >= 1) {
            char *k = alz_to_string(args[0]);
            AlzValue *v = stdlib_env_get(k);
            free(k);
            return v;
        }
        return alz_null();
    }

    if (strcmp(module, "sys") == 0) {
        if (strcmp(method, "exit") == 0) {
            int code = (argc >= 1 && args[0]->type == VAL_NUMBER)
                       ? (int)args[0]->as.number : 0;
            return stdlib_sys_exit(code);
        }
        return alz_null();
    }

    if (strcmp(module, "http") == 0) {
        return stdlib_http_dispatch(vm, method, args, argc);
    }

    if (strcmp(module, "db") == 0) {
        return stdlib_db_dispatch(vm, method, args, argc);
    }

    return alz_null();
}

/* REGISTER */

void stdlib_register(VM *vm) {
    srand((unsigned int)time(NULL));
    const char *mods[] = { "file", "str", "math", "date", "type", "env", "sys", "http", "db", NULL };
    for (int i = 0; mods[i]; i++) {
        /* Use VAL_MODULE — a simple tagged string, no heap object */
        vm_set_global(vm, mods[i], alz_module(mods[i]));
    }
}

/* ════════════════════════════════════════════════════════════════════
   HTTP MODULE DISPATCH
═════════════════════════════════════════════════════════════════════ */

#include "../include/http.h"

static AlzServer *g_server = NULL;

static AlzValue *stdlib_db_dispatch(VM *vm, const char *method, AlzValue **args, int argc);
static AlzValue *stdlib_http_dispatch(VM *vm, const char *method,
                               AlzValue **args, int argc) {

    if (strcmp(method, "serve") == 0 && argc >= 1) {
        int port = (args[0]->type == VAL_NUMBER)
                   ? (int)args[0]->as.number : 3000;
        if (!g_server) g_server = server_new(vm, port);
        server_start(g_server);
        return alz_null();
    }

    if (strcmp(method, "route") == 0 && argc >= 3) {
        if (!g_server) g_server = server_new(vm, 3000);
        char *m    = alz_to_string(args[0]);
        char *path = alz_to_string(args[1]);
        char *name = alz_to_string(args[2]);
        HttpMethod hm = http_method_parse(m);
        server_add_route(g_server, hm, path, name);
        free(m); free(path); free(name);
        return alz_null();
    }

    if (strcmp(method, "respond") == 0 && argc >= 1) {
        vm_set_global(vm, "__response__", alz_copy(args[0]));
        return alz_null();
    }

    if (strcmp(method, "json") == 0 && argc >= 1) {
        char *j = alz_to_json(args[0]);
        AlzValue *v = alz_string(j);
        free(j);
        return v;
    }

    if (strcmp(method, "parse") == 0 && argc >= 1) {
        char *s = alz_to_string(args[0]);
        AlzValue *v = alz_from_json(s);
        free(s);
        return v;
    }

    return alz_null();
}

/* ════════════════════════════════════════════════════════════════════
   DB MODULE DISPATCH
═════════════════════════════════════════════════════════════════════ */

#include "../include/db.h"

static AlzDB *g_db = NULL;

static AlzDB *get_db(VM *vm) {
    if (!g_db) g_db = db_new(vm, ALZ_DB_DIR);
    return g_db;
}

static AlzValue *stdlib_db_dispatch(VM *vm, const char *method,
                                    AlzValue **args, int argc) {
    AlzDB *db = get_db(vm);

    /* db.save("table", {obj}) */
    if (strcmp(method, "save") == 0 && argc >= 2) {
        char *t = alz_to_string(args[0]);
        AlzValue *v = db_save(db, t, args[1]);
        free(t); return v;
    }
    /* db.find("table", {filter}) */
    if (strcmp(method, "find") == 0 && argc >= 2) {
        char *t = alz_to_string(args[0]);
        AlzValue *v = db_find(db, t, args[1]);
        free(t); return v;
    }
    /* db.findById("table", "id") */
    if (strcmp(method, "findById") == 0 && argc >= 2) {
        char *t = alz_to_string(args[0]);
        char *id = alz_to_string(args[1]);
        AlzValue *v = db_find_by_id(db, t, id);
        free(t); free(id); return v;
    }
    /* db.all("table") */
    if (strcmp(method, "all") == 0 && argc >= 1) {
        char *t = alz_to_string(args[0]);
        AlzValue *v = db_all(db, t);
        free(t); return v;
    }
    /* db.where("table", {filter}) */
    if (strcmp(method, "where") == 0 && argc >= 2) {
        char *t = alz_to_string(args[0]);
        AlzValue *v = db_where(db, t, args[1]);
        free(t); return v;
    }
    /* db.update("table", {filter}, {data}) */
    if (strcmp(method, "update") == 0 && argc >= 3) {
        char *t = alz_to_string(args[0]);
        AlzValue *v = db_update(db, t, args[1], args[2]);
        free(t); return v;
    }
    /* db.updateById("table", "id", {data}) */
    if (strcmp(method, "updateById") == 0 && argc >= 3) {
        char *t  = alz_to_string(args[0]);
        char *id = alz_to_string(args[1]);
        AlzValue *v = db_update_by_id(db, t, id, args[2]);
        free(t); free(id); return v;
    }
    /* db.remove("table", {filter}) */
    if (strcmp(method, "remove") == 0 && argc >= 2) {
        char *t = alz_to_string(args[0]);
        AlzValue *v = db_remove(db, t, args[1]);
        free(t); return v;
    }
    /* db.removeById("table", "id") */
    if (strcmp(method, "removeById") == 0 && argc >= 2) {
        char *t  = alz_to_string(args[0]);
        char *id = alz_to_string(args[1]);
        AlzValue *v = db_remove_by_id(db, t, id);
        free(t); free(id); return v;
    }
    /* db.count("table") */
    if (strcmp(method, "count") == 0 && argc >= 1) {
        char *t = alz_to_string(args[0]);
        AlzValue *v = db_count(db, t);
        free(t); return v;
    }
    /* db.first("table") */
    if (strcmp(method, "first") == 0 && argc >= 1) {
        char *t = alz_to_string(args[0]);
        AlzValue *v = db_first(db, t);
        free(t); return v;
    }
    /* db.last("table") */
    if (strcmp(method, "last") == 0 && argc >= 1) {
        char *t = alz_to_string(args[0]);
        AlzValue *v = db_last(db, t);
        free(t); return v;
    }
    /* db.clear("table") */
    if (strcmp(method, "clear") == 0 && argc >= 1) {
        char *t = alz_to_string(args[0]);
        AlzValue *v = db_clear(db, t);
        free(t); return v;
    }
    /* db.drop("table") */
    if (strcmp(method, "drop") == 0 && argc >= 1) {
        char *t = alz_to_string(args[0]);
        AlzValue *v = db_drop(db, t);
        free(t); return v;
    }
    /* db.search("table", "query") */
    if (strcmp(method, "search") == 0 && argc >= 2) {
        char *t = alz_to_string(args[0]);
        char *q = alz_to_string(args[1]);
        AlzValue *v = db_search(db, t, q);
        free(t); free(q); return v;
    }
    /* db.sort("table", "field", "asc"|"desc") */
    if (strcmp(method, "sort") == 0 && argc >= 3) {
        char *t = alz_to_string(args[0]);
        char *f = alz_to_string(args[1]);
        char *d = alz_to_string(args[2]);
        int asc = strcmp(d, "desc") != 0;
        AlzValue *v = db_sort(db, t, f, asc);
        free(t); free(f); free(d); return v;
    }
    /* db.page("table", page, limit) */
    if (strcmp(method, "page") == 0 && argc >= 3) {
        char *t  = alz_to_string(args[0]);
        int pg   = (args[1]->type == VAL_NUMBER) ? (int)args[1]->as.number : 1;
        int lim  = (args[2]->type == VAL_NUMBER) ? (int)args[2]->as.number : 10;
        AlzValue *filter = (argc >= 4) ? args[3] : NULL;
        AlzValue *v = db_page(db, t, pg, lim, filter);
        free(t); return v;
    }
    /* db.exists("table", {filter}) */
    if (strcmp(method, "exists") == 0 && argc >= 2) {
        char *t = alz_to_string(args[0]);
        AlzValue *v = db_exists(db, t, args[1]);
        free(t); return v;
    }
    /* db.tables() */
    if (strcmp(method, "tables") == 0) {
        return db_tables(db);
    }
    return alz_null();
}
