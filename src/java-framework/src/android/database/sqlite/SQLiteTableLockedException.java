package android.database.sqlite;
public class SQLiteTableLockedException extends SQLiteException { public SQLiteTableLockedException() {} public SQLiteTableLockedException(String e) { super(e); } public SQLiteTableLockedException(String e, Throwable c) { super(e, c); } }
