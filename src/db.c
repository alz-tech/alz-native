#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#endif
#ifdef _WIN32
  #include <windows.h>
#else
  #include <dirent.h>
#endif
#include <time.h>
#include "../include/db.h"
#include "../include/http.h"

/* ════════════════════════════════════════════════════════════════════
   INTERNAL HELPERS
═════════════════════════════════════════════════════════════════════ */

/* Generate a unique ID: timestamp + random suffix */
static void gen_id(char *out, size_t max) {
    snprintf(out, max, "%ld%04d",
             (long)time(NULL), rand() % 9999);
}

/* Build path to table file */
static void table_path(AlzDB *db, const char *table, char *out, size_t max) {
    snprintf(out, max, "%s/%s.json", db->data_dir, table);
}

/* Ensure data dir exists - cross-platform compatible */
static void ensure_dir(const char *path) {
#ifdef _WIN32

#else
    mkdir(path, 0755);  /* Unix: mkdir takes path and mode */
#endif
}

/* ── Load entire table from disk → AlzValue list ──────────────────────── */
static AlzValue *table_load(AlzDB *db, const char *table) {
    char path[ALZ_DB_MAX_PATH];
    table_path(db, table, path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) return alz_list(); /* empty table */

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size <= 0) { fclose(f); return alz_list(); }

    char *buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    AlzValue *list = alz_from_json(buf);
    free(buf);

    if (!list || list->type != VAL_LIST) {
        if (list) alz_free(list);
        return alz_list();
    }
    return list;
}

/* ── Save entire table to disk ────────────────────────────────────────── */
static void table_save(AlzDB *db, const char *table, AlzValue *list) {
    char path[ALZ_DB_MAX_PATH];
    table_path(db, table, path, sizeof(path));
    ensure_dir(db->data_dir);

    char *json = alz_to_json(list);
    FILE *f    = fopen(path, "w");
    if (f) {
        fputs(json, f);
        fclose(f);
    }
    free(json);
}

/* ── Check if record matches filter ────────────────────────────────────── */
static int record_matches(AlzValue *record, AlzValue *filter) {
    if (!filter || filter->type == VAL_NULL) return 1;
    if (filter->type != VAL_OBJECT) return 0;
    if (record->type != VAL_OBJECT) return 0;

    for (size_t i = 0; i < filter->as.object->count; i++) {
        const char *key = filter->as.object->entries[i].key;
        AlzValue   *fv  = filter->as.object->entries[i].val;
        AlzValue   *rv  = alz_obj_get(record->as.object, key);

        if (!rv || rv->type == VAL_NULL) return 0;

        /* Compare based on type */
        if (fv->type == VAL_STRING && rv->type == VAL_STRING) {
            if (strcmp(fv->as.string, rv->as.string) != 0) return 0;
        } else if (fv->type == VAL_NUMBER && rv->type == VAL_NUMBER) {
            if (fv->as.number != rv->as.number) return 0;
        } else if (fv->type == VAL_BOOL && rv->type == VAL_BOOL) {
            if (fv->as.boolean != rv->as.boolean) return 0;
        } else {
            /* Type mismatch — compare as strings */
            char *fs = alz_to_string(fv), *rs = alz_to_string(rv);
            int eq = strcmp(fs, rs) == 0;
            free(fs); free(rs);
            if (!eq) return 0;
        }
    }
    return 1;
}

/* ── Merge fields from src into dst (update) ────────────────────────── */
static void record_merge(AlzValue *dst, AlzValue *src) {
    if (!src || src->type != VAL_OBJECT) return;
    if (!dst || dst->type != VAL_OBJECT) return;
    for (size_t i = 0; i < src->as.object->count; i++) {
        const char *key = src->as.object->entries[i].key;
        if (strcmp(key, "id") == 0) continue; /* never overwrite id */
        alz_obj_set(dst->as.object, key, alz_copy(src->as.object->entries[i].val));
    }
    /* Update timestamp */
    char ts[32];
    snprintf(ts, sizeof(ts), "%ld", (long)time(NULL));
    alz_obj_set(dst->as.object, "updatedAt", alz_string(ts));
}

/* ════════════════════════════════════════════════════════════════════
   LIFECYCLE
═════════════════════════════════════════════════════════════════════ */

AlzDB *db_new(VM *vm, const char *dir) {
    AlzDB *db = calloc(1, sizeof(AlzDB));
    db->vm = vm;
    strncpy(db->data_dir, dir ? dir : ALZ_DB_DIR, sizeof(db->data_dir)-1);
    ensure_dir(db->data_dir);
    return db;
}

void db_free(AlzDB *db) {
    if (db) free(db);
}

/* ════════════════════════════════════════════════════════════════════
   CORE OPS
═════════════════════════════════════════════════════════════════════ */

AlzValue *db_save(AlzDB *db, const char *table, AlzValue *record) {
    if (!record || record->type != VAL_OBJECT) return alz_null();

    AlzValue *list = table_load(db, table);
    AlzValue *copy = alz_copy(record);

    /* Generate ID if missing */
    AlzValue *existing_id = alz_obj_get(copy->as.object, "id");
    if (!existing_id || existing_id->type == VAL_NULL ||
        (existing_id->type == VAL_STRING && existing_id->as.string[0] == '\0')) {
        char id[32];
        gen_id(id, sizeof(id));
        alz_obj_set(copy->as.object, "id", alz_string(id));
    }

    /* Add timestamps */
    char ts[32];
    snprintf(ts, sizeof(ts), "%ld", (long)time(NULL));
    AlzValue *has_created = alz_obj_get(copy->as.object, "createdAt");
    if (!has_created || has_created->type == VAL_NULL)
        alz_obj_set(copy->as.object, "createdAt", alz_string(ts));
    alz_obj_set(copy->as.object, "updatedAt", alz_string(ts));

    alz_list_push(list->as.list, copy);
    table_save(db, table, list);

    AlzValue *result = alz_copy(copy);
    alz_free(list);
    return result;
}

AlzValue *db_find(AlzDB *db, const char *table, AlzValue *filter) {
    AlzValue *list = table_load(db, table);
    AlzValue *result = alz_null();

    for (size_t i = 0; i < list->as.list->count; i++) {
        AlzValue *rec = list->as.list->items[i];
        if (record_matches(rec, filter)) {
            result = alz_copy(rec);
            break;
        }
    }
    alz_free(list);
    return result;
}

AlzValue *db_find_by_id(AlzDB *db, const char *table, const char *id) {
    AlzValue *filter = alz_object();
    alz_obj_set(filter->as.object, "id", alz_string(id));
    AlzValue *result = db_find(db, table, filter);
    alz_free(filter);
    return result;
}

AlzValue *db_all(AlzDB *db, const char *table) {
    return table_load(db, table);
}

AlzValue *db_where(AlzDB *db, const char *table, AlzValue *filter) {
    AlzValue *list   = table_load(db, table);
    AlzValue *result = alz_list();

    for (size_t i = 0; i < list->as.list->count; i++) {
        AlzValue *rec = list->as.list->items[i];
        if (record_matches(rec, filter))
            alz_list_push(result->as.list, alz_copy(rec));
    }
    alz_free(list);
    return result;
}

AlzValue *db_update(AlzDB *db, const char *table, AlzValue *filter, AlzValue *data) {
    AlzValue *list    = table_load(db, table);
    AlzValue *updated = alz_null();

    for (size_t i = 0; i < list->as.list->count; i++) {
        AlzValue *rec = list->as.list->items[i];
        if (record_matches(rec, filter)) {
            record_merge(rec, data);
            updated = alz_copy(rec);
            break;
        }
    }
    table_save(db, table, list);
    alz_free(list);
    return updated;
}

AlzValue *db_update_by_id(AlzDB *db, const char *table,
                          const char *id, AlzValue *data) {
    AlzValue *filter = alz_object();
    alz_obj_set(filter->as.object, "id", alz_string(id));
    AlzValue *result = db_update(db, table, filter, data);
    alz_free(filter);
    return result;
}

AlzValue *db_remove(AlzDB *db, const char *table, AlzValue *filter) {
    AlzValue *list    = table_load(db, table);
    AlzValue *newlist = alz_list();
    int       removed = 0;

    for (size_t i = 0; i < list->as.list->count; i++) {
        AlzValue *rec = list->as.list->items[i];
        if (!removed && record_matches(rec, filter)) {
            removed = 1;
            alz_free(alz_copy(rec)); /* just skip it */
        } else {
            alz_list_push(newlist->as.list, alz_copy(rec));
        }
    }
    table_save(db, table, newlist);
    alz_free(list);
    alz_free(newlist);
    return alz_bool(removed);
}

AlzValue *db_remove_by_id(AlzDB *db, const char *table, const char *id) {
    AlzValue *filter = alz_object();
    alz_obj_set(filter->as.object, "id", alz_string(id));
    AlzValue *result = db_remove(db, table, filter);
    alz_free(filter);
    return result;
}

AlzValue *db_count(AlzDB *db, const char *table) {
    AlzValue *list  = table_load(db, table);
    double    count = (double)list->as.list->count;
    alz_free(list);
    return alz_number(count);
}

AlzValue *db_first(AlzDB *db, const char *table) {
    AlzValue *list = table_load(db, table);
    AlzValue *result = (list->as.list->count > 0)
        ? alz_copy(list->as.list->items[0])
        : alz_null();
    alz_free(list);
    return result;
}

AlzValue *db_last(AlzDB *db, const char *table) {
    AlzValue *list = table_load(db, table);
    size_t    n    = list->as.list->count;
    AlzValue *result = (n > 0)
        ? alz_copy(list->as.list->items[n-1])
        : alz_null();
    alz_free(list);
    return result;
}

AlzValue *db_clear(AlzDB *db, const char *table) {
    AlzValue *empty = alz_list();
    table_save(db, table, empty);
    alz_free(empty);
    return alz_bool(1);
}

AlzValue *db_drop(AlzDB *db, const char *table) {
    char path[ALZ_DB_MAX_PATH];
    table_path(db, table, path, sizeof(path));
    return alz_bool(remove(path) == 0);
}

/* ════════════════════════════════════════════════════════════════════
   QUERY OPS
═════════════════════════════════════════════════════════════════════ */

AlzValue *db_search(AlzDB *db, const char *table, const char *query) {
    AlzValue *list   = table_load(db, table);
    AlzValue *result = alz_list();
    char      qlower[512];
    size_t    i = 0;

    /* Lowercase query for case-insensitive search */
    for (; query[i] && i < sizeof(qlower)-1; i++)
        qlower[i] = tolower((unsigned char)query[i]);
    qlower[i] = '\0';

    for (size_t j = 0; j < list->as.list->count; j++) {
        AlzValue *rec = list->as.list->items[j];
        if (rec->type != VAL_OBJECT) continue;

        /* Search every field value */
        int found = 0;
        for (size_t k = 0; k < rec->as.object->count; k++) {
            char *val = alz_to_string(rec->as.object->entries[k].val);
            char  vlower[1024];
            size_t m = 0;
            for (; val[m] && m < sizeof(vlower)-1; m++)
                vlower[m] = tolower((unsigned char)val[m]);
            vlower[m] = '\0';
            free(val);
            if (strstr(vlower, qlower)) { found = 1; break; }
        }
        if (found) alz_list_push(result->as.list, alz_copy(rec));
    }
    alz_free(list);
    return result;
}

AlzValue *db_sort(AlzDB *db, const char *table, const char *field, int asc) {
    AlzValue *list   = table_load(db, table);
    size_t    n      = list->as.list->count;
    AlzValue *result = alz_list();

    /* Copy items into result */
    for (size_t i = 0; i < n; i++)
        alz_list_push(result->as.list, alz_copy(list->as.list->items[i]));

    /* Bubble sort (simple — works for reasonable data sizes) */
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j + 1 < n - i; j++) {
            AlzValue *a = result->as.list->items[j];
            AlzValue *b = result->as.list->items[j+1];
            AlzValue *av = (a->type == VAL_OBJECT)
                           ? alz_obj_get(a->as.object, field) : alz_null();
            AlzValue *bv = (b->type == VAL_OBJECT)
                           ? alz_obj_get(b->as.object, field) : alz_null();

            int swap = 0;
            if (av && bv) {
                if (av->type == VAL_NUMBER && bv->type == VAL_NUMBER) {
                    swap = asc ? (av->as.number > bv->as.number)
                               : (av->as.number < bv->as.number);
                } else {
                    char *as = alz_to_string(av), *bs = alz_to_string(bv);
                    int cmp = strcmp(as, bs);
                    free(as); free(bs);
                    swap = asc ? (cmp > 0) : (cmp < 0);
                }
            }
            if (swap) {
                AlzValue *tmp = result->as.list->items[j];
                result->as.list->items[j]   = result->as.list->items[j+1];
                result->as.list->items[j+1] = tmp;
            }
        }
    }
    alz_free(list);
    return result;
}

AlzValue *db_page(AlzDB *db, const char *table,
                  int page, int limit, AlzValue *filter) {
    if (page < 1)  page  = 1;
    if (limit < 1) limit = 10;

    AlzValue *all    = filter ? db_where(db, table, filter)
                              : table_load(db, table);
    size_t    total  = all->as.list->count;
    size_t    start  = (size_t)((page - 1) * limit);
    size_t    end    = start + (size_t)limit;
    if (end > total) end = total;

    AlzValue *items = alz_list();
    for (size_t i = start; i < end; i++)
        alz_list_push(items->as.list, alz_copy(all->as.list->items[i]));

    /* Return { items, total, page, pages } */
    AlzValue *result = alz_object();
    alz_obj_set(result->as.object, "items",  items);
    alz_obj_set(result->as.object, "total",  alz_number((double)total));
    alz_obj_set(result->as.object, "page",   alz_number((double)page));
    alz_obj_set(result->as.object, "pages",
        alz_number((double)((total + limit - 1) / limit)));
    alz_obj_set(result->as.object, "limit",  alz_number((double)limit));

    alz_free(all);
    return result;
}

AlzValue *db_exists(AlzDB *db, const char *table, AlzValue *filter) {
    AlzValue *rec = db_find(db, table, filter);
    int found = (rec && rec->type != VAL_NULL);
    alz_free(rec);
    return alz_bool(found);
}

AlzValue *db_tables(AlzDB *db) {
    AlzValue *list = alz_list();
#ifdef _WIN32
    WIN32_FIND_DATA ffd;
    char pattern[ALZ_DB_MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*.json", db->data_dir);
    HANDLE hFind = FindFirstFile(pattern, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return list;
    do {
        const char *name = ffd.cFileName;
        size_t len = strlen(name);
        if (len > 5) {
            char table[256];
            strncpy(table, name, len - 5);
            table[len - 5] = '\0';
            alz_list_push(list->as.list, alz_string(table));
        }
    } while (FindNextFile(hFind, &ffd));
    FindClose(hFind);
#else
    DIR *dir = opendir(db->data_dir);
    if (!dir) return list;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len > 5 && strcmp(name + len - 5, ".json") == 0) {
            char table[256];
            strncpy(table, name, len - 5);
            table[len - 5] = '\0';
            alz_list_push(list->as.list, alz_string(table));
        }
    }
    closedir(dir);
#endif
    return list;
}
