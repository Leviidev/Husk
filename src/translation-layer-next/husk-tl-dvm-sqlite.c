/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * SQLite for Java apps (husk.Sqlite's natives): the system's sqlite3 (iOS and macOS carry it), which android.database.sqlite drives
 * as Android's own SQLiteConnection drives the copy it carries. The LOCALIZED and UNICODE collations Android registers on every
 * connection are registered here too (apps name them in their schemas).
 */
#include <sqlite3.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "husk-tl-dvm-internal.h"

#define NAT(fn) static bool fn(jobj *self, const jvalue *a, jvalue *ret)
static jvalue I(int32_t v) { jvalue r; r.j = (uint32_t)v; return r; }
static jvalue J(int64_t v) { jvalue r; r.j = v; return r; }
static jvalue D(double d) { jvalue r; r.d = d; return r; }
static jvalue L(void *p) { jvalue r; r.j = 0; r.l = p; return r; }
#define DB ((sqlite3 *)(uintptr_t)a[0].j)
#define ST ((sqlite3_stmt *)(uintptr_t)a[0].j)

static int coll_nocase(void *u, int la, const void *a, int lb, const void *b)
{
    (void)u;
    int n = la < lb ? la : lb, r = strncasecmp(a, b, (size_t)n);
    return r ? r : la - lb;
}
static int coll_binary(void *u, int la, const void *a, int lb, const void *b)
{
    (void)u;
    int n = la < lb ? la : lb, r = memcmp(a, b, (size_t)n);
    return r ? r : la - lb;
}

/* open(path, flags): Android's OPEN_READONLY (1) / CREATE_IF_NECESSARY (0x10000000); the handle, or 0 (errorOf(0) says why) */
static char g_open_error[512];
NAT(S_open)
{
    (void)self;
    const char *path = tl_jni_string(a[0].l);
    int fl = a[1].i;
    int flags = (fl & 1) ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE;
    if (fl & 0x10000000) flags |= SQLITE_OPEN_CREATE;
    flags |= SQLITE_OPEN_FULLMUTEX;
    char resolved[1024];
    extern const char *tl_path_resolve(const char *path, char *buf, size_t n);
    if (path && strcmp(path, ":memory:")) path = tl_path_resolve(path, resolved, sizeof(resolved));
    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(path ? path : ":memory:", &db, flags, NULL);
    if (rc != SQLITE_OK) {
        snprintf(g_open_error, sizeof(g_open_error), "%s (code %d)", db ? sqlite3_errmsg(db) : sqlite3_errstr(rc), rc);
        if (db) sqlite3_close(db);
        *ret = J(0);
        return true;
    }
    sqlite3_extended_result_codes(db, 1);
    sqlite3_busy_timeout(db, 2500);
    sqlite3_create_collation(db, "LOCALIZED", SQLITE_UTF8, NULL, coll_nocase);
    sqlite3_create_collation(db, "UNICODE", SQLITE_UTF8, NULL, coll_binary);
    *ret = J((int64_t)(uintptr_t)db);
    return true;
}
NAT(S_close) { (void)self; (void)ret; if (DB) sqlite3_close_v2(DB); return true; }
NAT(S_errmsg) { (void)self; *ret = L(dvm_new_string_utf8(DB ? sqlite3_errmsg(DB) : g_open_error)); return true; }
NAT(S_errcode) { (void)self; *ret = I(DB ? sqlite3_extended_errcode(DB) : SQLITE_CANTOPEN); return true; }
NAT(S_exec)
{
    (void)self;
    const char *sql = tl_jni_string(a[1].l);
    *ret = I(sql ? sqlite3_exec(DB, sql, NULL, NULL, NULL) : SQLITE_MISUSE);
    return true;
}
NAT(S_changes) { (void)self; *ret = I(sqlite3_changes(DB)); return true; }
NAT(S_lastInsertRowid) { (void)self; *ret = J(sqlite3_last_insert_rowid(DB)); return true; }
NAT(S_autocommit) { (void)self; *ret = I(sqlite3_get_autocommit(DB)); return true; }
NAT(S_busyTimeout) { (void)self; (void)ret; sqlite3_busy_timeout(DB, a[1].i); return true; }
NAT(S_prepare)
{
    (void)self;
    const char *sql = tl_jni_string(a[1].l);
    sqlite3_stmt *st = NULL;
    if (!sql || sqlite3_prepare_v2(DB, sql, -1, &st, NULL) != SQLITE_OK) st = NULL;
    *ret = J((int64_t)(uintptr_t)st);
    return true;
}
NAT(S_finalize) { (void)self; (void)ret; if (ST) sqlite3_finalize(ST); return true; }
NAT(S_reset) { (void)self; *ret = I(sqlite3_reset(ST)); return true; }
NAT(S_clearBindings) { (void)self; (void)ret; sqlite3_clear_bindings(ST); return true; }
NAT(S_paramCount) { (void)self; *ret = I(sqlite3_bind_parameter_count(ST)); return true; }
NAT(S_readOnly) { (void)self; *ret = I(sqlite3_stmt_readonly(ST)); return true; }
NAT(S_bindNull) { (void)self; *ret = I(sqlite3_bind_null(ST, a[1].i)); return true; }
NAT(S_bindLong) { (void)self; *ret = I(sqlite3_bind_int64(ST, a[1].i, a[2].j)); return true; }
NAT(S_bindDouble) { (void)self; *ret = I(sqlite3_bind_double(ST, a[1].i, a[2].d)); return true; }
NAT(S_bindString)
{
    (void)self;
    int32_t n; const uint16_t *s = tl_dvm_string_chars(a[2].l, &n);
    *ret = I(s ? sqlite3_bind_text16(ST, a[1].i, s, n * 2, SQLITE_TRANSIENT) : sqlite3_bind_null(ST, a[1].i));
    return true;
}
NAT(S_bindBlob)
{
    (void)self;
    jobj *b = a[2].l;
    *ret = I(b ? sqlite3_bind_blob(ST, a[1].i, b->arr.data, (int)b->arr.len, SQLITE_TRANSIENT) : sqlite3_bind_null(ST, a[1].i));
    return true;
}
NAT(S_step) { (void)self; *ret = I(sqlite3_step(ST)); return true; }
NAT(S_columnCount) { (void)self; *ret = I(sqlite3_column_count(ST)); return true; }
NAT(S_columnName) { (void)self; const char *n = sqlite3_column_name(ST, a[1].i); *ret = L(dvm_new_string_utf8(n ? n : "")); return true; }
/* Android's Cursor field types: NULL 0, INTEGER 1, FLOAT 2, STRING 3, BLOB 4 */
NAT(S_columnType)
{
    (void)self;
    int t = sqlite3_column_type(ST, a[1].i);
    *ret = I(t == SQLITE_INTEGER ? 1 : t == SQLITE_FLOAT ? 2 : t == SQLITE_TEXT ? 3 : t == SQLITE_BLOB ? 4 : 0);
    return true;
}
NAT(S_columnLong) { (void)self; *ret = J(sqlite3_column_int64(ST, a[1].i)); return true; }
NAT(S_columnDouble) { (void)self; *ret = D(sqlite3_column_double(ST, a[1].i)); return true; }
NAT(S_columnText)
{
    (void)self;
    const void *s = sqlite3_column_text16(ST, a[1].i);
    int n = sqlite3_column_bytes16(ST, a[1].i);
    *ret = L(s ? tl_dvm_string_u16(s, n / 2) : NULL);
    return true;
}
NAT(S_columnBlob)
{
    (void)self;
    const void *b = sqlite3_column_blob(ST, a[1].i);
    int n = sqlite3_column_bytes(ST, a[1].i);
    if (!b && sqlite3_column_type(ST, a[1].i) == SQLITE_NULL) { *ret = L(NULL); return true; }
    jobj *arr = tl_jni_new_prim_array('B', (uint32_t)n);
    arr->refs = 1u << 30;
    if (n) memcpy(arr->arr.data, b, (size_t)n);
    *ret = L(arr);
    return true;
}
NAT(S_version) { (void)self; (void)a; *ret = L(dvm_new_string_utf8(sqlite3_libversion())); return true; }

static const struct { const char *name, *sig; dvm_native_fn fn; } k_sqlite[] = {
    { "open", "(Ljava/lang/String;I)J", S_open },
    { "close", "(J)V", S_close },
    { "errmsg", "(J)Ljava/lang/String;", S_errmsg },
    { "errcode", "(J)I", S_errcode },
    { "exec", "(JLjava/lang/String;)I", S_exec },
    { "changes", "(J)I", S_changes },
    { "lastInsertRowid", "(J)J", S_lastInsertRowid },
    { "autocommit", "(J)I", S_autocommit },
    { "busyTimeout", "(JI)V", S_busyTimeout },
    { "prepare", "(JLjava/lang/String;)J", S_prepare },
    { "finalizeStatement", "(J)V", S_finalize },
    { "reset", "(J)I", S_reset },
    { "clearBindings", "(J)V", S_clearBindings },
    { "paramCount", "(J)I", S_paramCount },
    { "readOnly", "(J)I", S_readOnly },
    { "bindNull", "(JI)I", S_bindNull },
    { "bindLong", "(JIJ)I", S_bindLong },
    { "bindDouble", "(JID)I", S_bindDouble },
    { "bindString", "(JILjava/lang/String;)I", S_bindString },
    { "bindBlob", "(JI[B)I", S_bindBlob },
    { "step", "(J)I", S_step },
    { "columnCount", "(J)I", S_columnCount },
    { "columnName", "(JI)Ljava/lang/String;", S_columnName },
    { "columnType", "(JI)I", S_columnType },
    { "columnLong", "(JI)J", S_columnLong },
    { "columnDouble", "(JI)D", S_columnDouble },
    { "columnText", "(JI)Ljava/lang/String;", S_columnText },
    { "columnBlob", "(JI)[B", S_columnBlob },
    { "version", "()Ljava/lang/String;", S_version },
    { NULL, NULL, NULL },
};
dvm_native_fn tl_sqlite_native(const char *name, const char *sig);
dvm_native_fn tl_sqlite_native(const char *name, const char *sig)
{
    for (int i = 0; k_sqlite[i].name; i++) if (!strcmp(k_sqlite[i].name, name) && !strcmp(k_sqlite[i].sig, sig)) return k_sqlite[i].fn;
    return NULL;
}
