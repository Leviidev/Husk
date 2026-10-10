package android.database.sqlite;
public final class SQLiteQuery extends SQLiteProgram {
    SQLiteQuery(SQLiteDatabase db, String query, android.os.CancellationSignal cs) { super(db, query, null, cs); }
    int fillWindow(android.database.CursorWindow window, int startPos, int requiredPos, boolean countAllRows) { return huskFill(window); }
    @Override public String toString() { return "SQLiteQuery: " + getSql(); }
}
