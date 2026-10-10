package android.database.sqlite;

import android.database.Cursor;

public final class SQLiteDirectCursorDriver implements SQLiteCursorDriver {
    private final SQLiteDatabase mDatabase;
    private final String mEditTable, mSql;
    private final android.os.CancellationSignal mCancellationSignal;
    private SQLiteQuery mQuery;
    public SQLiteDirectCursorDriver(SQLiteDatabase db, String sql, String editTable, android.os.CancellationSignal cs) { mDatabase = db; mEditTable = editTable; mSql = sql; mCancellationSignal = cs; }
    public Cursor query(SQLiteDatabase.CursorFactory factory, String[] selectionArgs) {
        final SQLiteQuery query = new SQLiteQuery(mDatabase, mSql, mCancellationSignal);
        final Cursor cursor;
        try {
            query.bindAllArgsAsStrings(selectionArgs);
            cursor = factory == null ? new SQLiteCursor(this, mEditTable, query) : factory.newCursor(mDatabase, this, mEditTable, query);
        } catch (RuntimeException ex) { query.close(); throw ex; }
        mQuery = query;
        return cursor;
    }
    public void cursorClosed() {}
    public void setBindArguments(String[] bindArgs) { mQuery.bindAllArgsAsStrings(bindArgs); }
    public void cursorDeactivated() {}
    public void cursorRequeried(Cursor cursor) {}
    @Override public String toString() { return "SQLiteDirectCursorDriver: " + mSql; }
}
