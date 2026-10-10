package android.database.sqlite;
public interface SQLiteCursorDriver {
    android.database.Cursor query(SQLiteDatabase.CursorFactory factory, String[] bindArgs);
    void cursorDeactivated(); void cursorRequeried(android.database.Cursor cursor); void cursorClosed(); void setBindArguments(String[] bindArgs);
}
