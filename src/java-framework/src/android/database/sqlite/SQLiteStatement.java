package android.database.sqlite;

public final class SQLiteStatement extends SQLiteProgram {
    SQLiteStatement(SQLiteDatabase db, String sql, Object[] bindArgs) { super(db, sql, bindArgs, null); }
    public void execute() { huskExecute(); }
    public int executeUpdateDelete() { return huskExecute(); }
    public long executeInsert() {
        SQLiteDatabase db = getDatabase();
        db.acquire();
        try { int changes = huskExecute(); return changes > 0 ? husk.Sqlite.lastInsertRowid(db.handle()) : -1; }
        finally { db.release(); }
    }
    public long simpleQueryForLong() { android.database.CursorWindow w = new android.database.CursorWindow("simpleQuery"); if (huskFill(w) == 0) throw new SQLiteDoneException(); return w.getLong(0, 0); }
    public String simpleQueryForString() { android.database.CursorWindow w = new android.database.CursorWindow("simpleQuery"); if (huskFill(w) == 0) throw new SQLiteDoneException(); return w.getString(0, 0); }
    public android.os.ParcelFileDescriptor simpleQueryForBlobFileDescriptor() { return null; }
    @Override public String toString() { return "SQLiteProgram: " + getSql(); }
}
