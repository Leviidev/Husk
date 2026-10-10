package android.database;
public class SQLException extends RuntimeException { public SQLException() {} public SQLException(String e) { super(e); } public SQLException(String e, Throwable c) { super(e, c); } }
