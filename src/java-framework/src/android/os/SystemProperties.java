package android.os;

/** The hidden SystemProperties apps read by reflection (ro.build.*, ro.product.*, vendor checks): the device's build values as
 *  Build reports them, anything else unset. set() keeps values for this process. */
public class SystemProperties {
    private static final java.util.HashMap<String, String> sSet = new java.util.HashMap<>();
    public static final int PROP_VALUE_MAX = 91;

    private static String lookup(String key) {
        synchronized (sSet) { if (sSet.containsKey(key)) return sSet.get(key); }
        switch (key) {
            case "ro.build.version.sdk": return String.valueOf(Build.VERSION.SDK_INT);
            case "ro.build.version.release": return Build.VERSION.RELEASE;
            case "ro.build.version.incremental": return Build.VERSION.INCREMENTAL;
            case "ro.build.version.codename": return Build.VERSION.CODENAME;
            case "ro.build.version.security_patch": return Build.VERSION.SECURITY_PATCH;
            case "ro.build.id": return Build.ID;
            case "ro.build.display.id": return Build.DISPLAY;
            case "ro.build.type": return Build.TYPE;
            case "ro.build.tags": return Build.TAGS;
            case "ro.build.fingerprint": return Build.FINGERPRINT;
            case "ro.product.model": return Build.MODEL;
            case "ro.product.brand": return Build.BRAND;
            case "ro.product.name": return Build.PRODUCT;
            case "ro.product.device": return Build.DEVICE;
            case "ro.product.manufacturer": return Build.MANUFACTURER;
            case "ro.product.cpu.abi": return "arm64-v8a";
            case "ro.product.cpu.abilist": case "ro.product.cpu.abilist64": return "arm64-v8a";
            case "ro.hardware": return Build.HARDWARE;
            case "ro.debuggable": case "ro.secure": return key.equals("ro.secure") ? "1" : "0";
            case "persist.sys.locale": return java.util.Locale.getDefault().toLanguageTag();
        }
        return null;
    }
    public static String get(String key) { String v = lookup(key); return v != null ? v : ""; }
    public static String get(String key, String def) { String v = lookup(key); return v != null && !v.isEmpty() ? v : def; }
    public static int getInt(String key, int def) { try { return Integer.parseInt(get(key)); } catch (NumberFormatException e) { return def; } }
    public static long getLong(String key, long def) { try { return Long.parseLong(get(key)); } catch (NumberFormatException e) { return def; } }
    public static boolean getBoolean(String key, boolean def) {
        String v = get(key);
        if (v.equals("1") || v.equals("y") || v.equals("yes") || v.equals("on") || v.equals("true")) return true;
        if (v.equals("0") || v.equals("n") || v.equals("no") || v.equals("off") || v.equals("false")) return false;
        return def;
    }
    public static void set(String key, String val) { synchronized (sSet) { sSet.put(key, val); } }
    public static void addChangeCallback(Runnable r) {}
    public static void removeChangeCallback(Runnable r) {}
}
