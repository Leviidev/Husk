package android.database.sqlite;

import android.content.ContentValues;
import android.database.Cursor;
import android.database.DatabaseErrorHandler;
import android.database.DatabaseUtils;
import android.database.DefaultDatabaseErrorHandler;
import android.database.SQLException;
import android.os.CancellationSignal;
import android.util.Pair;
import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.locks.ReentrantLock;

/**
 * A database: one connection to the system's sqlite3 (husk.Sqlite), used by one thread at a time (a transaction keeps it for its
 * thread until it ends, as Android's primary connection does). Nested transactions behave as Android's: an inner one that does not
 * succeed makes the outer one roll back.
 */
public final class SQLiteDatabase extends SQLiteClosable {
    public static final int CONFLICT_NONE = 0, CONFLICT_ROLLBACK = 1, CONFLICT_ABORT = 2, CONFLICT_FAIL = 3, CONFLICT_IGNORE = 4, CONFLICT_REPLACE = 5;
    private static final String[] CONFLICT_VALUES = { "", " OR ROLLBACK ", " OR ABORT ", " OR FAIL ", " OR IGNORE ", " OR REPLACE " };
    public static final int SQLITE_MAX_LIKE_PATTERN_LENGTH = 50000;
    public static final int OPEN_READWRITE = 0x00000000, OPEN_READONLY = 0x00000001, NO_LOCALIZED_COLLATORS = 0x00000010, CREATE_IF_NECESSARY = 0x10000000, ENABLE_WRITE_AHEAD_LOGGING = 0x20000000;
    public static final int MAX_SQL_CACHE_SIZE = 100;
    public static final String JOURNAL_MODE_WAL = "WAL", JOURNAL_MODE_PERSIST = "PERSIST", JOURNAL_MODE_TRUNCATE = "TRUNCATE", JOURNAL_MODE_MEMORY = "MEMORY", JOURNAL_MODE_DELETE = "DELETE", JOURNAL_MODE_OFF = "OFF";
    public static final String SYNC_MODE_EXTRA = "EXTRA", SYNC_MODE_FULL = "FULL", SYNC_MODE_NORMAL = "NORMAL", SYNC_MODE_OFF = "OFF";
    public interface CursorFactory { Cursor newCursor(SQLiteDatabase db, SQLiteCursorDriver masterQuery, String editTable, SQLiteQuery query); }
    public interface CustomFunction { void callback(String[] args); }

    private final String mPath;
    private final int mFlags;
    private final CursorFactory mFactory;
    private final DatabaseErrorHandler mErrorHandler;
    private long mHandle;
    private boolean mWal;
    final ReentrantLock mLock = new ReentrantLock(true);
    private static final class Txn { boolean successful, childFailed; SQLiteTransactionListener listener; Txn parent; }
    private Txn mTxn;

    private SQLiteDatabase(String path, int flags, CursorFactory factory, DatabaseErrorHandler errorHandler) {
        mPath = path; mFlags = flags; mFactory = factory;
        mErrorHandler = errorHandler != null ? errorHandler : new DefaultDatabaseErrorHandler();
    }
    private void open() {
        mHandle = husk.Sqlite.open(mPath, mFlags);
        if (mHandle == 0) throw new SQLiteCantOpenDatabaseException("Cannot open database '" + mPath + "': " + husk.Sqlite.errmsg(0));
        if ((mFlags & ENABLE_WRITE_AHEAD_LOGGING) != 0) enableWriteAheadLogging();
    }
    // ---- opening
    public static SQLiteDatabase openDatabase(String path, CursorFactory factory, int flags) { return openDatabase(path, factory, flags, null); }
    public static SQLiteDatabase openDatabase(String path, CursorFactory factory, int flags, DatabaseErrorHandler errorHandler) {
        SQLiteDatabase db = new SQLiteDatabase(path, flags, factory, errorHandler);
        db.open();
        return db;
    }
    public static SQLiteDatabase openDatabase(File path, OpenParams p) {
        SQLiteDatabase db = new SQLiteDatabase(path.getPath(), p.mOpenFlags, p.mCursorFactory, p.mErrorHandler);
        db.open();
        return db;
    }
    public static SQLiteDatabase openOrCreateDatabase(File file, CursorFactory factory) { return openOrCreateDatabase(file.getPath(), factory); }
    public static SQLiteDatabase openOrCreateDatabase(String path, CursorFactory factory) { return openDatabase(path, factory, CREATE_IF_NECESSARY, null); }
    public static SQLiteDatabase openOrCreateDatabase(String path, CursorFactory factory, DatabaseErrorHandler errorHandler) { return openDatabase(path, factory, CREATE_IF_NECESSARY, errorHandler); }
    public static SQLiteDatabase create(CursorFactory factory) { return openDatabase(":memory:", factory, CREATE_IF_NECESSARY); }
    public static SQLiteDatabase createInMemory(OpenParams p) { SQLiteDatabase db = new SQLiteDatabase(":memory:", p.mOpenFlags | CREATE_IF_NECESSARY, p.mCursorFactory, p.mErrorHandler); db.open(); return db; }
    public static boolean deleteDatabase(File file) {
        boolean deleted = file.delete();
        deleted |= new File(file.getPath() + "-journal").delete();
        deleted |= new File(file.getPath() + "-shm").delete();
        deleted |= new File(file.getPath() + "-wal").delete();
        return deleted;
    }
    public static int releaseMemory() { return 0; }
    public static String findEditTable(String tables) {
        if (tables == null || tables.isEmpty()) throw new IllegalStateException("Invalid tables");
        int spacepos = tables.indexOf(' '), commapos = tables.indexOf(',');
        if (spacepos > 0 && (spacepos < commapos || commapos < 0)) return tables.substring(0, spacepos);
        if (commapos > 0 && (commapos < spacepos || spacepos < 0)) return tables.substring(0, commapos);
        return tables;
    }
    // ---- the connection
    long handle() { if (mHandle == 0) throw new IllegalStateException("attempt to re-open an already-closed object: SQLiteDatabase: " + mPath); return mHandle; }
    void acquire() { mLock.lock(); }
    void release() { mLock.unlock(); }
    /** Throw the exception Android throws for sqlite3's error code. */
    SQLiteException error(String what) {
        int code = mHandle != 0 ? husk.Sqlite.errcode(mHandle) : 14;
        String msg = (mHandle != 0 ? husk.Sqlite.errmsg(mHandle) : "unknown error") + " (code " + code + ")" + (what != null ? (": , while compiling: " + what) : "");
        return exceptionFor(code, msg);
    }
    SQLiteException exceptionFor(int code, String msg) {
        switch (code & 0xff) {
        case 4: return new SQLiteAbortException(msg);
        case 5: return new SQLiteDatabaseLockedException(msg);
        case 6: return new SQLiteTableLockedException(msg);
        case 7: return new SQLiteOutOfMemoryException(msg);
        case 8: return new SQLiteReadOnlyDatabaseException(msg);
        case 10: return new SQLiteDiskIOException(msg);
        case 11: case 26: mErrorHandler.onCorruption(this); return new SQLiteDatabaseCorruptException(msg);
        case 13: return new SQLiteFullException(msg);
        case 14: return new SQLiteCantOpenDatabaseException(msg);
        case 18: return new SQLiteBlobTooBigException(msg);
        case 19: return new SQLiteConstraintException(msg);
        case 20: return new SQLiteDatatypeMismatchException(msg);
        case 21: return new SQLiteMisuseException(msg);
        case 23: return new SQLiteAccessPermException(msg);
        case 25: return new SQLiteBindOrColumnIndexOutOfRangeException(msg);
        default: return new SQLiteException(msg);
        }
    }
    private void execRaw(String sql) {
        acquire();
        try { int rc = husk.Sqlite.exec(handle(), sql); if (rc != 0) throw error(sql); }
        finally { release(); }
    }
    @Override protected void onAllReferencesReleased() { if (mHandle != 0) { husk.Sqlite.close(mHandle); mHandle = 0; } }
    @Override public void close() { acquire(); try { super.close(); } finally { release(); } }
    @Override protected void finalize() throws Throwable { try { if (mHandle != 0) { husk.Sqlite.close(mHandle); mHandle = 0; } } finally { super.finalize(); } }
    // ---- transactions
    public void beginTransaction() { beginTransaction(null, true); }
    public void beginTransactionNonExclusive() { beginTransaction(null, false); }
    public void beginTransactionReadOnly() { beginTransaction(null, false); }
    public void beginTransactionWithListener(SQLiteTransactionListener l) { beginTransaction(l, true); }
    public void beginTransactionWithListenerNonExclusive(SQLiteTransactionListener l) { beginTransaction(l, false); }
    public void beginTransactionWithListenerReadOnly(SQLiteTransactionListener l) { beginTransaction(l, false); }
    private void beginTransaction(SQLiteTransactionListener listener, boolean exclusive) {
        acquire();
        boolean ok = false;
        try {
            if (mTxn == null) { int rc = husk.Sqlite.exec(handle(), exclusive ? "BEGIN EXCLUSIVE;" : "BEGIN IMMEDIATE;"); if (rc != 0) throw error("BEGIN"); }
            Txn t = new Txn(); t.listener = listener; t.parent = mTxn; mTxn = t;
            if (listener != null) {
                try { listener.onBegin(); }
                catch (RuntimeException e) { if (t.parent == null) husk.Sqlite.exec(handle(), "ROLLBACK;"); mTxn = t.parent; throw e; }
            }
            ok = true;
        } finally { if (!ok) release(); }
    }
    public void setTransactionSuccessful() {
        if (mTxn == null || !mLock.isHeldByCurrentThread()) throw new IllegalStateException("Cannot perform this operation because there is no current transaction.");
        if (mTxn.successful) throw new IllegalStateException("Cannot perform this operation because the transaction has already been marked successful.  The only thing you can do now is call endTransaction().");
        mTxn.successful = true;
    }
    public void endTransaction() {
        if (mTxn == null || !mLock.isHeldByCurrentThread()) throw new IllegalStateException("Cannot perform this operation because there is no current transaction.");
        Txn t = mTxn;
        mTxn = t.parent;
        boolean success = t.successful && !t.childFailed;
        RuntimeException listenerError = null;
        try {
            if (t.listener != null) {
                try { if (success) t.listener.onCommit(); else t.listener.onRollback(); }
                catch (RuntimeException e) { listenerError = e; success = false; }
            }
            if (mTxn != null) { if (!success) mTxn.childFailed = true; }
            else {
                int rc = husk.Sqlite.exec(handle(), success ? "COMMIT;" : "ROLLBACK;");
                if (rc != 0) { husk.Sqlite.exec(handle(), "ROLLBACK;"); if (listenerError == null) throw error(success ? "COMMIT" : "ROLLBACK"); }
            }
        } finally { release(); }
        if (listenerError != null) throw listenerError;
    }
    public boolean inTransaction() { return mTxn != null && mLock.isHeldByCurrentThread(); }
    public boolean isDbLockedByCurrentThread() { return mLock.isHeldByCurrentThread(); }
    @Deprecated public boolean isDbLockedByOtherThreads() { return mLock.isLocked() && !mLock.isHeldByCurrentThread(); }
    @Deprecated public boolean yieldIfContended() { return yieldIfContendedSafely(); }
    public boolean yieldIfContendedSafely() { return yieldIfContendedSafely(0); }
    public boolean yieldIfContendedSafely(long sleepAfterYieldDelay) {
        if (mTxn == null || mTxn.parent != null || !mLock.hasQueuedThreads()) return false;
        SQLiteTransactionListener l = mTxn.listener;
        setTransactionSuccessful();
        endTransaction();
        if (sleepAfterYieldDelay > 0) { try { Thread.sleep(sleepAfterYieldDelay); } catch (InterruptedException e) {} }
        beginTransaction(l, true);
        return true;
    }
    // ---- settings
    public int getVersion() { return (int) DatabaseUtils.longForQuery(this, "PRAGMA user_version;", null); }
    public void setVersion(int version) { execSQL("PRAGMA user_version = " + version); }
    public long getMaximumSize() { long pageCount = DatabaseUtils.longForQuery(this, "PRAGMA max_page_count;", null); return pageCount * getPageSize(); }
    public long setMaximumSize(long numBytes) { long pageSize = getPageSize(); long numPages = numBytes / pageSize; if ((numBytes % pageSize) != 0) numPages++; long newPageCount = DatabaseUtils.longForQuery(this, "PRAGMA max_page_count = " + numPages, null); return newPageCount * pageSize; }
    public long getPageSize() { return DatabaseUtils.longForQuery(this, "PRAGMA page_size;", null); }
    public void setPageSize(long numBytes) { execSQL("PRAGMA page_size = " + numBytes); }
    @Deprecated public void markTableSyncable(String table, String deletedTable) {}
    @Deprecated public void markTableSyncable(String table, String foreignKey, String updateTable) {}
    @Deprecated public java.util.Map<String, String> getSyncedTables() { return new java.util.HashMap<>(); }
    @Deprecated public void setLockingEnabled(boolean lockingEnabled) {}
    public boolean isReadOnly() { return (mFlags & OPEN_READONLY) != 0; }
    public boolean isInMemoryDatabase() { return ":memory:".equals(mPath); }
    public boolean isOpen() { return mHandle != 0; }
    public boolean needUpgrade(int newVersion) { return newVersion > getVersion(); }
    public final String getPath() { return mPath; }
    public void setLocale(Locale locale) {}
    public void setMaxSqlCacheSize(int cacheSize) { if (cacheSize > MAX_SQL_CACHE_SIZE || cacheSize < 0) throw new IllegalStateException("expected value between 0 and " + MAX_SQL_CACHE_SIZE); }
    public void setForeignKeyConstraintsEnabled(boolean enable) { execRaw("PRAGMA foreign_keys = " + (enable ? "ON" : "OFF") + ";"); }
    public boolean enableWriteAheadLogging() {
        if (isInMemoryDatabase() || isReadOnly()) return false;
        if (mTxn != null) throw new IllegalStateException("Write Ahead Logging (WAL) mode cannot be enabled or disabled while there are transactions in progress.  Finish all transactions and release all active database connections first.");
        String mode = DatabaseUtils.stringForQuery(this, "PRAGMA journal_mode=WAL;", null);
        mWal = "wal".equalsIgnoreCase(mode);
        return true;
    }
    public void disableWriteAheadLogging() { if (mWal) { DatabaseUtils.stringForQuery(this, "PRAGMA journal_mode=DELETE;", null); mWal = false; } }
    public boolean isWriteAheadLoggingEnabled() { return mWal; }
    public List<Pair<String, String>> getAttachedDbs() {
        ArrayList<Pair<String, String>> l = new ArrayList<>();
        if (!isOpen()) return null;
        try (Cursor c = rawQuery("pragma database_list;", null)) { while (c.moveToNext()) l.add(new Pair<>(c.getString(1), c.getString(2))); }
        return l;
    }
    public boolean isDatabaseIntegrityOk() {
        try (Cursor c = rawQuery("PRAGMA integrity_check;", null)) { return c.moveToFirst() && "ok".equalsIgnoreCase(c.getString(0)); }
    }
    // ---- statements and queries
    public SQLiteStatement compileStatement(String sql) throws SQLException { return new SQLiteStatement(this, sql, null); }
    public Cursor query(boolean distinct, String table, String[] columns, String selection, String[] selectionArgs, String groupBy, String having, String orderBy, String limit) { return queryWithFactory(null, distinct, table, columns, selection, selectionArgs, groupBy, having, orderBy, limit, null); }
    public Cursor query(boolean distinct, String table, String[] columns, String selection, String[] selectionArgs, String groupBy, String having, String orderBy, String limit, CancellationSignal cs) { return queryWithFactory(null, distinct, table, columns, selection, selectionArgs, groupBy, having, orderBy, limit, cs); }
    public Cursor queryWithFactory(CursorFactory f, boolean distinct, String table, String[] columns, String selection, String[] selectionArgs, String groupBy, String having, String orderBy, String limit) { return queryWithFactory(f, distinct, table, columns, selection, selectionArgs, groupBy, having, orderBy, limit, null); }
    public Cursor queryWithFactory(CursorFactory f, boolean distinct, String table, String[] columns, String selection, String[] selectionArgs, String groupBy, String having, String orderBy, String limit, CancellationSignal cs) {
        String sql = SQLiteQueryBuilder.buildQueryString(distinct, table, columns, selection, groupBy, having, orderBy, limit);
        return rawQueryWithFactory(f, sql, selectionArgs, findEditTable(table), cs);
    }
    public Cursor query(String table, String[] columns, String selection, String[] selectionArgs, String groupBy, String having, String orderBy) { return query(false, table, columns, selection, selectionArgs, groupBy, having, orderBy, null); }
    public Cursor query(String table, String[] columns, String selection, String[] selectionArgs, String groupBy, String having, String orderBy, String limit) { return query(false, table, columns, selection, selectionArgs, groupBy, having, orderBy, limit); }
    public Cursor rawQuery(String sql, String[] selectionArgs) { return rawQueryWithFactory(null, sql, selectionArgs, null, null); }
    public Cursor rawQuery(String sql, String[] selectionArgs, CancellationSignal cs) { return rawQueryWithFactory(null, sql, selectionArgs, null, cs); }
    public Cursor rawQueryWithFactory(CursorFactory f, String sql, String[] selectionArgs, String editTable) { return rawQueryWithFactory(f, sql, selectionArgs, editTable, null); }
    public Cursor rawQueryWithFactory(CursorFactory f, String sql, String[] selectionArgs, String editTable, CancellationSignal cs) {
        SQLiteCursorDriver driver = new SQLiteDirectCursorDriver(this, sql, editTable, cs);
        return driver.query(f != null ? f : mFactory, selectionArgs);
    }
    public long insert(String table, String nullColumnHack, ContentValues values) { try { return insertWithOnConflict(table, nullColumnHack, values, CONFLICT_NONE); } catch (SQLException e) { android.util.Log.e("SQLiteDatabase", "Error inserting " + values, e); return -1; } }
    public long insertOrThrow(String table, String nullColumnHack, ContentValues values) throws SQLException { return insertWithOnConflict(table, nullColumnHack, values, CONFLICT_NONE); }
    public long replace(String table, String nullColumnHack, ContentValues initialValues) { try { return insertWithOnConflict(table, nullColumnHack, initialValues, CONFLICT_REPLACE); } catch (SQLException e) { android.util.Log.e("SQLiteDatabase", "Error inserting " + initialValues, e); return -1; } }
    public long replaceOrThrow(String table, String nullColumnHack, ContentValues initialValues) throws SQLException { return insertWithOnConflict(table, nullColumnHack, initialValues, CONFLICT_REPLACE); }
    public long insertWithOnConflict(String table, String nullColumnHack, ContentValues initialValues, int conflictAlgorithm) {
        StringBuilder sql = new StringBuilder();
        sql.append("INSERT").append(CONFLICT_VALUES[conflictAlgorithm]).append(" INTO ").append(table).append('(');
        Object[] bindArgs = null;
        int size = (initialValues != null && !initialValues.isEmpty()) ? initialValues.size() : 0;
        if (size > 0) {
            bindArgs = new Object[size];
            int i = 0;
            for (String colName : initialValues.keySet()) { sql.append((i > 0) ? "," : ""); sql.append(colName); bindArgs[i++] = initialValues.get(colName); }
            sql.append(')').append(" VALUES (");
            for (i = 0; i < size; i++) sql.append((i > 0) ? ",?" : "?");
        } else sql.append(nullColumnHack).append(") VALUES (NULL");
        sql.append(')');
        try (SQLiteStatement st = new SQLiteStatement(this, sql.toString(), bindArgs)) { return st.executeInsert(); }
    }
    public int delete(String table, String whereClause, String[] whereArgs) {
        try (SQLiteStatement st = new SQLiteStatement(this, "DELETE FROM " + table + (whereClause != null && !whereClause.isEmpty() ? " WHERE " + whereClause : ""), whereArgs)) { return st.executeUpdateDelete(); }
    }
    public int update(String table, ContentValues values, String whereClause, String[] whereArgs) { return updateWithOnConflict(table, values, whereClause, whereArgs, CONFLICT_NONE); }
    public int updateWithOnConflict(String table, ContentValues values, String whereClause, String[] whereArgs, int conflictAlgorithm) {
        if (values == null || values.isEmpty()) throw new IllegalArgumentException("Empty values");
        StringBuilder sql = new StringBuilder(120);
        sql.append("UPDATE ").append(CONFLICT_VALUES[conflictAlgorithm]).append(table).append(" SET ");
        int setValuesSize = values.size(), bindArgsSize = (whereArgs == null) ? setValuesSize : (setValuesSize + whereArgs.length);
        Object[] bindArgs = new Object[bindArgsSize];
        int i = 0;
        for (String colName : values.keySet()) { sql.append((i > 0) ? "," : ""); sql.append(colName); bindArgs[i++] = values.get(colName); sql.append("=?"); }
        if (whereArgs != null) for (i = setValuesSize; i < bindArgsSize; i++) bindArgs[i] = whereArgs[i - setValuesSize];
        if (whereClause != null && !whereClause.isEmpty()) sql.append(" WHERE ").append(whereClause);
        try (SQLiteStatement st = new SQLiteStatement(this, sql.toString(), bindArgs)) { return st.executeUpdateDelete(); }
    }
    public void execSQL(String sql) throws SQLException { executeSql(sql, null); }
    public void execSQL(String sql, Object[] bindArgs) throws SQLException { if (bindArgs == null) throw new IllegalArgumentException("Empty bindArgs"); executeSql(sql, bindArgs); }
    public void execPerConnectionSQL(String sql, Object[] bindArgs) throws SQLException { executeSql(sql, bindArgs); }
    private void executeSql(String sql, Object[] bindArgs) {
        if (DatabaseUtils.getSqlStatementType(sql) == DatabaseUtils.STATEMENT_ATTACH) { /* attaching disables WAL, as on Android */ disableWriteAheadLogging(); }
        try (SQLiteStatement st = new SQLiteStatement(this, sql, bindArgs)) { st.executeUpdateDelete(); }
    }
    public void validateSql(String sql, CancellationSignal cs) {
        acquire();
        try { long s = husk.Sqlite.prepare(handle(), sql); if (s == 0) throw error(sql); husk.Sqlite.finalizeStatement(s); }
        finally { release(); }
    }
    public void setCustomScalarFunction(String name, java.util.function.UnaryOperator<String> fn) { throw new UnsupportedOperationException("custom SQL functions are not available"); }
    public void setCustomAggregateFunction(String name, java.util.function.BinaryOperator<String> fn) { throw new UnsupportedOperationException("custom SQL functions are not available"); }
    @Override public String toString() { return "SQLiteDatabase: " + mPath; }

    public static final class OpenParams {
        final int mOpenFlags; final CursorFactory mCursorFactory; final DatabaseErrorHandler mErrorHandler; final int mLookasideSlotSize, mLookasideSlotCount; final long mIdleConnectionTimeout; final String mJournalMode, mSyncMode;
        private OpenParams(int f, CursorFactory c, DatabaseErrorHandler e, int ls, int lc, long it, String jm, String sm) { mOpenFlags = f; mCursorFactory = c; mErrorHandler = e; mLookasideSlotSize = ls; mLookasideSlotCount = lc; mIdleConnectionTimeout = it; mJournalMode = jm; mSyncMode = sm; }
        public int getLookasideSlotSize() { return mLookasideSlotSize; }
        public int getLookasideSlotCount() { return mLookasideSlotCount; }
        public int getOpenFlags() { return mOpenFlags; }
        public CursorFactory getCursorFactory() { return mCursorFactory; }
        public DatabaseErrorHandler getErrorHandler() { return mErrorHandler; }
        public long getIdleConnectionTimeout() { return mIdleConnectionTimeout; }
        public String getJournalMode() { return mJournalMode; }
        public String getSynchronousMode() { return mSyncMode; }
        public Builder toBuilder() { return new Builder(this); }
        public static final class Builder {
            private int mLookasideSlotSize = -1, mLookasideSlotCount = -1, mOpenFlags; private long mIdleConnectionTimeout = -1; private CursorFactory mCursorFactory; private DatabaseErrorHandler mErrorHandler; private String mJournalMode, mSyncMode;
            public Builder() {}
            public Builder(OpenParams p) { mLookasideSlotSize = p.mLookasideSlotSize; mLookasideSlotCount = p.mLookasideSlotCount; mOpenFlags = p.mOpenFlags; mCursorFactory = p.mCursorFactory; mErrorHandler = p.mErrorHandler; mJournalMode = p.mJournalMode; mSyncMode = p.mSyncMode; }
            public Builder setLookasideConfig(int slotSize, int slotCount) { mLookasideSlotSize = slotSize; mLookasideSlotCount = slotCount; return this; }
            public Builder addOpenFlags(int f) { mOpenFlags |= f; return this; }
            public Builder removeOpenFlags(int f) { mOpenFlags &= ~f; return this; }
            public Builder setOpenFlags(int f) { mOpenFlags = f; return this; }
            public Builder setCursorFactory(CursorFactory c) { mCursorFactory = c; return this; }
            public Builder setErrorHandler(DatabaseErrorHandler e) { mErrorHandler = e; return this; }
            public Builder setIdleConnectionTimeout(long ms) { mIdleConnectionTimeout = ms; return this; }
            public Builder setJournalMode(String m) { mJournalMode = m; return this; }
            public Builder setSynchronousMode(String m) { mSyncMode = m; return this; }
            public OpenParams build() { return new OpenParams(mOpenFlags, mCursorFactory, mErrorHandler, mLookasideSlotSize, mLookasideSlotCount, mIdleConnectionTimeout, mJournalMode, mSyncMode); }
        }
    }
}
