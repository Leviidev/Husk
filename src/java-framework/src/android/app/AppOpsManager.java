package android.app;
public class AppOpsManager {
    public static final int MODE_ALLOWED = 0, MODE_IGNORED = 1, MODE_ERRORED = 2, MODE_DEFAULT = 3, MODE_FOREGROUND = 4;
    public static final String OPSTR_GET_USAGE_STATS = "android:get_usage_stats", OPSTR_SYSTEM_ALERT_WINDOW = "android:system_alert_window", OPSTR_PICTURE_IN_PICTURE = "android:picture_in_picture";
    public int checkOpNoThrow(String op, int uid, String pkg) { return MODE_ALLOWED; } public int unsafeCheckOpNoThrow(String op, int uid, String pkg) { return MODE_ALLOWED; }
    public int noteOpNoThrow(String op, int uid, String pkg) { return MODE_ALLOWED; } public int noteOpNoThrow(String op, int uid, String pkg, String tag, String msg) { return MODE_ALLOWED; }
    public int noteProxyOpNoThrow(String op, String pkg) { return MODE_ALLOWED; } public int checkOp(String op, int uid, String pkg) { return MODE_ALLOWED; }
    public void checkPackage(int uid, String pkg) {} public static String permissionToOp(String p) { return null; }
    public void startWatchingMode(String op, String pkg, Object cb) {} public void stopWatchingMode(Object cb) {}
}
