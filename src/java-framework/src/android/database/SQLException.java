package android.database;
public class SQLException extends RuntimeException { public SQLException() {} public SQLException(String s) { super(s); } public SQLException(String s, Throwable t) { super(s, t); } }
