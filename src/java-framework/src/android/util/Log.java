package android.util;

public final class Log {
    public static final int VERBOSE = 2, DEBUG = 3, INFO = 4, WARN = 5, ERROR = 6, ASSERT = 7;
    private Log() {}
    private static int out(int p, String tag, String msg, Throwable t) {
        if (t != null) msg = msg + "\n" + getStackTraceString(t);
        husk.Native.log(p, tag == null ? "" : tag, msg == null ? "null" : msg);
        return 0;
    }
    public static int v(String tag, String msg) { return out(VERBOSE, tag, msg, null); }
    public static int v(String tag, String msg, Throwable t) { return out(VERBOSE, tag, msg, t); }
    public static int d(String tag, String msg) { return out(DEBUG, tag, msg, null); }
    public static int d(String tag, String msg, Throwable t) { return out(DEBUG, tag, msg, t); }
    public static int i(String tag, String msg) { return out(INFO, tag, msg, null); }
    public static int i(String tag, String msg, Throwable t) { return out(INFO, tag, msg, t); }
    public static int w(String tag, String msg) { return out(WARN, tag, msg, null); }
    public static int w(String tag, String msg, Throwable t) { return out(WARN, tag, msg, t); }
    public static int w(String tag, Throwable t) { return out(WARN, tag, "", t); }
    public static int e(String tag, String msg) { return out(ERROR, tag, msg, null); }
    public static int e(String tag, String msg, Throwable t) { return out(ERROR, tag, msg, t); }
    public static int wtf(String tag, String msg) { return out(ASSERT, tag, msg, null); }
    public static int println(int p, String tag, String msg) { return out(p, tag, msg, null); }
    public static boolean isLoggable(String tag, int level) { return level >= INFO; }
    public static String getStackTraceString(Throwable t) {
        if (t == null) return "";
        java.io.StringWriter sw = new java.io.StringWriter();
        t.printStackTrace(new java.io.PrintWriter(sw));
        return sw.toString();
    }
}
