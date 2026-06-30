#ifndef ALZ_DB_H
#define ALZ_DB_H

#include <stddef.h>
#include "value.h"
#include "vm.h"

/* ── Database ────────────────────────────────────────────────────────────── */
#define ALZ_DB_DIR      ".alzdb"
#define ALZ_DB_MAX_PATH 512

typedef struct {
    char   data_dir[ALZ_DB_MAX_PATH];
    VM    *vm;
} AlzDB;

/* ── Lifecycle ───────────────────────────────────────────────────────────── */
AlzDB    *db_new(VM *vm, const char *dir);
void      db_free(AlzDB *db);

/* ── Core ops ────────────────────────────────────────────────────────────── */
AlzValue *db_save(AlzDB *db, const char *table, AlzValue *record);
AlzValue *db_find(AlzDB *db, const char *table, AlzValue *filter);
AlzValue *db_find_by_id(AlzDB *db, const char *table, const char *id);
AlzValue *db_all(AlzDB *db, const char *table);
AlzValue *db_where(AlzDB *db, const char *table, AlzValue *filter);
AlzValue *db_update(AlzDB *db, const char *table, AlzValue *filter, AlzValue *data);
AlzValue *db_update_by_id(AlzDB *db, const char *table, const char *id, AlzValue *data);
AlzValue *db_remove(AlzDB *db, const char *table, AlzValue *filter);
AlzValue *db_remove_by_id(AlzDB *db, const char *table, const char *id);
AlzValue *db_count(AlzDB *db, const char *table);
AlzValue *db_first(AlzDB *db, const char *table);
AlzValue *db_last(AlzDB *db, const char *table);
AlzValue *db_clear(AlzDB *db, const char *table);
AlzValue *db_drop(AlzDB *db, const char *table);

/* ── Query ops ───────────────────────────────────────────────────────────── */
AlzValue *db_search(AlzDB *db, const char *table, const char *query);
AlzValue *db_sort(AlzDB *db, const char *table, const char *field, int asc);
AlzValue *db_page(AlzDB *db, const char *table, int page, int limit, AlzValue *filter);
AlzValue *db_exists(AlzDB *db, const char *table, AlzValue *filter);

/* ── Table list ──────────────────────────────────────────────────────────── */
AlzValue *db_tables(AlzDB *db);

#endif
