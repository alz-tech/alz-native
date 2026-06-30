#ifndef ALZ_STDLIB_H
#define ALZ_STDLIB_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "vm.h"
#include "value.h"
#include "http.h"
#include "db.h"

// ── Stdlib registration ───────────────────────────────────────────────────────
// Called once at VM startup — registers all built-in functions as globals
void stdlib_register(VM *vm);

// ── Module handlers ───────────────────────────────────────────────────────────
// Each returns an AlzValue* (result pushed onto stack by VM)

// File I/O
AlzValue *stdlib_file_read(const char *path);
AlzValue *stdlib_file_write(const char *path, const char *data);
AlzValue *stdlib_file_append(const char *path, const char *data);
AlzValue *stdlib_file_exists(const char *path);
AlzValue *stdlib_file_delete(const char *path);

// String ops
AlzValue *stdlib_str_upper(const char *s);
AlzValue *stdlib_str_lower(const char *s);
AlzValue *stdlib_str_trim(const char *s);
AlzValue *stdlib_str_split(const char *s, const char *sep);
AlzValue *stdlib_str_replace(const char *s, const char *from, const char *to);
AlzValue *stdlib_str_contains(const char *s, const char *sub);
AlzValue *stdlib_str_starts(const char *s, const char *sub);
AlzValue *stdlib_str_ends(const char *s, const char *sub);
AlzValue *stdlib_str_len(const char *s);
AlzValue *stdlib_str_slice(const char *s, int start, int end);
AlzValue *stdlib_str_reverse(const char *s);
AlzValue *stdlib_str_repeat(const char *s, int n);
AlzValue *stdlib_str_index_of(const char *s, const char *sub);

// Math
AlzValue *stdlib_math_sqrt(double n);
AlzValue *stdlib_math_floor(double n);
AlzValue *stdlib_math_ceil(double n);
AlzValue *stdlib_math_round(double n);
AlzValue *stdlib_math_abs(double n);
AlzValue *stdlib_math_pow(double base, double exp);
AlzValue *stdlib_math_min(double a, double b);
AlzValue *stdlib_math_max(double a, double b);
AlzValue *stdlib_math_random();
AlzValue *stdlib_math_pi();

// Date/Time
AlzValue *stdlib_date_now();       // unix timestamp
AlzValue *stdlib_date_string();    // "2026-06-23"
AlzValue *stdlib_date_time();      // "14:30:00"
AlzValue *stdlib_date_year();
AlzValue *stdlib_date_month();
AlzValue *stdlib_date_day();
AlzValue *stdlib_date_hour();
AlzValue *stdlib_date_minute();

// Type utils
AlzValue *stdlib_type_of(AlzValue *val);    // "string", "number", etc.
AlzValue *stdlib_to_num(AlzValue *val);
AlzValue *stdlib_to_str(AlzValue *val);
AlzValue *stdlib_to_bool(AlzValue *val);

// System
AlzValue *stdlib_env_get(const char *key);
AlzValue *stdlib_sys_exit(int code);
AlzValue *stdlib_sys_args();

// ── Dispatch ──────────────────────────────────────────────────────────────────
// Called by VM to invoke stdlib module methods
AlzValue *stdlib_dispatch(VM *vm, const char *module, const char *method,
                          AlzValue **args, int argc);

#endif
