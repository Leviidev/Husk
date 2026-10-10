package android.database.sqlite;

import android.database.CursorWindow;

/** A compiled statement's SQL and its bound arguments; it runs against the database's connection each time it executes. */
public abstract class SQLiteProgram extends SQLiteClosable {
    private static final String[] EMPTY_STRING_ARRAY = new String[0];
    private final SQLiteDatabase mDatabase;
    private final String mSql;
    private final boolean mReadOnly;
    private final String[] mColumnNames;
    private final int mNumParameters;
    private final Object[] mBindArgs;
    SQLiteProgram(SQLiteDatabase db, String sql, Object[] bindArgs, android.os.CancellationSignal cs) {
        mDatabase = db;
        mSql = sql.trim();
        db.acquire();
        try {
            long st = husk.Sqlite.prepare(db.handle(), mSql);
            if (st == 0) throw db.error(mSql);
            try {
                mReadOnly = husk.Sqlite.readOnly(st) != 0;
                mNumParameters = husk.Sqlite.paramCount(st);
                int n = husk.Sqlite.columnCount(st);
                mColumnNames = n == 0 ? EMPTY_STRING_ARRAY : new String[n];
                for (int i = 0; i < n; i++) mColumnNames[i] = husk.Sqlite.columnName(st, i);
            } finally { husk.Sqlite.finalizeStatement(st); }
        } finally { db.release(); }
        if (bindArgs != null && bindArgs.length > mNumParameters) throw new IllegalArgumentException("Too many bind arguments.  " + bindArgs.length + " arguments were provided but the statement needs " + mNumParameters + " arguments.");
        mBindArgs = mNumParameters != 0 ? new Object[mNumParameters] : null;
        if (bindArgs != null && bindArgs.length != 0) System.arraycopy(bindArgs, 0, mBindArgs, 0, bindArgs.length);
    }
    final SQLiteDatabase getDatabase() { return mDatabase; }
    final String getSql() { return mSql; }
    final Object[] getBindArgs() { return mBindArgs; }
    final String[] getColumnNames() { return mColumnNames; }
    @Deprecated public final int getUniqueId() { return -1; }
    public void bindNull(int index) { bind(index, null); }
    public void bindLong(int index, long value) { bind(index, value); }
    public void bindDouble(int index, double value) { bind(index, value); }
    public void bindString(int index, String value) { if (value == null) throw new IllegalArgumentException("the bind value at index " + index + " is null"); bind(index, value); }
    public void bindBlob(int index, byte[] value) { if (value == null) throw new IllegalArgumentException("the bind value at index " + index + " is null"); bind(index, value); }
    public void clearBindings() { if (mBindArgs != null) java.util.Arrays.fill(mBindArgs, null); }
    public void bindAllArgsAsStrings(String[] bindArgs) { if (bindArgs != null) for (int i = bindArgs.length; i != 0; i--) bindString(i, bindArgs[i - 1]); }
    @Override protected void onAllReferencesReleased() { clearBindings(); }
    private void bind(int index, Object value) {
        if (index < 1 || index > mNumParameters) throw new IllegalArgumentException("Cannot bind argument at index " + index + " because the index is out of range.  The statement has " + mNumParameters + " parameters.");
        mBindArgs[index - 1] = value;
    }
    /** Prepare and bind on the connection (the caller holds the database). */
    long huskPrepare() {
        long st = husk.Sqlite.prepare(mDatabase.handle(), mSql);
        if (st == 0) throw mDatabase.error(mSql);
        if (mBindArgs != null) {
            for (int i = 0; i < mBindArgs.length; i++) {
                Object v = mBindArgs[i];
                int rc;
                if (v == null) rc = husk.Sqlite.bindNull(st, i + 1);
                else if (v instanceof byte[]) rc = husk.Sqlite.bindBlob(st, i + 1, (byte[]) v);
                else if (v instanceof Double || v instanceof Float) rc = husk.Sqlite.bindDouble(st, i + 1, ((Number) v).doubleValue());
                else if (v instanceof Long || v instanceof Integer || v instanceof Short || v instanceof Byte) rc = husk.Sqlite.bindLong(st, i + 1, ((Number) v).longValue());
                else if (v instanceof Boolean) rc = husk.Sqlite.bindLong(st, i + 1, ((Boolean) v) ? 1 : 0);
                else rc = husk.Sqlite.bindString(st, i + 1, v.toString());
                if (rc != 0) { husk.Sqlite.finalizeStatement(st); throw mDatabase.error(mSql); }
            }
        }
        return st;
    }
    /** Run to completion; sqlite3_changes afterwards. */
    int huskExecute() {
        SQLiteDatabase db = mDatabase;
        db.acquire();
        try {
            long st = huskPrepare();
            try {
                int rc;
                while ((rc = husk.Sqlite.step(st)) == husk.Sqlite.ROW) {}
                if (rc != husk.Sqlite.DONE) throw db.error(mSql);
                return husk.Sqlite.changes(db.handle());
            } finally { husk.Sqlite.finalizeStatement(st); }
        } finally { db.release(); }
    }
    /** Every row of the result into the window. */
    int huskFill(CursorWindow window) {
        SQLiteDatabase db = mDatabase;
        db.acquire();
        try {
            long st = huskPrepare();
            try {
                int cols = husk.Sqlite.columnCount(st), rows = 0, rc;
                window.clear();
                window.setNumColumns(cols);
                while ((rc = husk.Sqlite.step(st)) == husk.Sqlite.ROW) {
                    Object[] r = new Object[cols];
                    for (int c = 0; c < cols; c++) {
                        switch (husk.Sqlite.columnType(st, c)) {
                        case 1: r[c] = husk.Sqlite.columnLong(st, c); break;
                        case 2: r[c] = husk.Sqlite.columnDouble(st, c); break;
                        case 3: r[c] = husk.Sqlite.columnText(st, c); break;
                        case 4: r[c] = husk.Sqlite.columnBlob(st, c); break;
                        default: r[c] = null;
                        }
                    }
                    window.huskAddRow(r);
                    rows++;
                }
                if (rc != husk.Sqlite.DONE) throw db.error(mSql);
                return rows;
            } finally { husk.Sqlite.finalizeStatement(st); }
        } finally { db.release(); }
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    protected int getConnectionFlags() { return 0; }
    protected void onCorruption() {}
    // ---- end of generated members
}
