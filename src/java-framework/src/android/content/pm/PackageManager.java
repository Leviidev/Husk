package android.content.pm;

public class PackageManager {
    public static final int PERMISSION_GRANTED = 0, PERMISSION_DENIED = -1, GET_META_DATA = 128, GET_SIGNATURES = 64, GET_ACTIVITIES = 1;
    public static final String FEATURE_TOUCHSCREEN = "android.hardware.touchscreen", FEATURE_TOUCHSCREEN_MULTITOUCH = "android.hardware.touchscreen.multitouch";
    public static class NameNotFoundException extends Exception { public NameNotFoundException() {} public NameNotFoundException(String s) { super(s); } }
    private static PackageManager sSelf;
    public static synchronized PackageManager self() { if (sSelf == null) sSelf = new PackageManager(); return sSelf; }
    public PackageInfo getPackageInfo(String pkg, int flags) throws NameNotFoundException {
        if (!pkg.equals(husk.Native.packageName())) throw new NameNotFoundException(pkg);
        PackageInfo p = new PackageInfo(); p.packageName = pkg; return p;
    }
    public ApplicationInfo getApplicationInfo(String pkg, int flags) throws NameNotFoundException {
        if (!pkg.equals(husk.Native.packageName())) throw new NameNotFoundException(pkg);
        ApplicationInfo a = ApplicationInfo.self(); if ((flags & GET_META_DATA) != 0 && a == null) { } return a;
    }
    public ActivityInfo getActivityInfo(android.content.ComponentName c, int flags) { ActivityInfo a = new ActivityInfo(); a.name = c.getClassName(); a.packageName = c.getPackageName(); a.metaData = new android.os.Bundle(); return a; }
    public boolean hasSystemFeature(String name) { return name.startsWith("android.hardware.touchscreen") || name.equals("android.hardware.opengles.aep"); }
    public int checkPermission(String perm, String pkg) { return PERMISSION_GRANTED; }
    public CharSequence getApplicationLabel(ApplicationInfo a) { return a.packageName; }
    public String getInstallerPackageName(String pkg) { return "com.android.vending"; }
    public java.util.List<ResolveInfo> queryIntentActivities(android.content.Intent i, int flags) { return new java.util.ArrayList<>(); }
    public ResolveInfo resolveActivity(android.content.Intent i, int flags) { return null; }
    public android.content.Intent getLaunchIntentForPackage(String pkg) { return null; }
}
