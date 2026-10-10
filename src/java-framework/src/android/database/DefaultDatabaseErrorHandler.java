package android.database;
public final class DefaultDatabaseErrorHandler implements DatabaseErrorHandler {
    public void onCorruption(android.database.sqlite.SQLiteDatabase db) {
        android.util.Log.e("DefaultDatabaseErrorHandler", "Corruption reported by sqlite on database: " + db.getPath());
        String path = db.getPath();
        try { db.close(); } catch (Exception e) {}
        if (path != null && !path.equalsIgnoreCase(":memory:")) android.database.sqlite.SQLiteDatabase.deleteDatabase(new java.io.File(path));
    }
}
