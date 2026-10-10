package husk;
/** The system's sqlite3 (husk-tl-dvm-sqlite.c): handles are its sqlite3 * and sqlite3_stmt * pointers. */
public final class Sqlite {
    private Sqlite() {}
    public static final int OK = 0, ROW = 100, DONE = 101;
    public static native long open(String path, int flags);
    public static native void close(long db);
    public static native String errmsg(long db);
    public static native int errcode(long db);
    public static native int exec(long db, String sql);
    public static native int changes(long db);
    public static native long lastInsertRowid(long db);
    public static native int autocommit(long db);
    public static native void busyTimeout(long db, int ms);
    public static native long prepare(long db, String sql);
    public static native void finalizeStatement(long st);
    public static native int reset(long st);
    public static native void clearBindings(long st);
    public static native int paramCount(long st);
    public static native int readOnly(long st);
    public static native int bindNull(long st, int i);
    public static native int bindLong(long st, int i, long v);
    public static native int bindDouble(long st, int i, double v);
    public static native int bindString(long st, int i, String v);
    public static native int bindBlob(long st, int i, byte[] v);
    public static native int step(long st);
    public static native int columnCount(long st);
    public static native String columnName(long st, int i);
    public static native int columnType(long st, int i);
    public static native long columnLong(long st, int i);
    public static native double columnDouble(long st, int i);
    public static native String columnText(long st, int i);
    public static native byte[] columnBlob(long st, int i);
    public static native String version();
}
