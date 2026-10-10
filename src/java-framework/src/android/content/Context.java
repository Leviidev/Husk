package android.content;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.os.Looper;
import java.io.File;
import java.util.HashMap;

/** One context for the whole app: Husk runs one app at a time, and every Context answers the same. */
public abstract class Context {
    public static final int MODE_PRIVATE = 0, MODE_APPEND = 0x8000, MODE_WORLD_READABLE = 1, MODE_WORLD_WRITEABLE = 2, MODE_MULTI_PROCESS = 4;
    public static final int BIND_AUTO_CREATE = 1;
    public static final String AUDIO_SERVICE = "audio", VIBRATOR_SERVICE = "vibrator", WINDOW_SERVICE = "window", CLIPBOARD_SERVICE = "clipboard",
        INPUT_METHOD_SERVICE = "input_method", SENSOR_SERVICE = "sensor", POWER_SERVICE = "power", ACTIVITY_SERVICE = "activity",
        CONNECTIVITY_SERVICE = "connectivity", WIFI_SERVICE = "wifi", LOCATION_SERVICE = "location", NOTIFICATION_SERVICE = "notification",
        LAYOUT_INFLATER_SERVICE = "layout_inflater", DISPLAY_SERVICE = "display", UI_MODE_SERVICE = "uimode", KEYGUARD_SERVICE = "keyguard",
        TELEPHONY_SERVICE = "phone", STORAGE_SERVICE = "storage", INPUT_SERVICE = "input", VIBRATOR_MANAGER_SERVICE = "vibrator_manager";

    private static final HashMap<String, Object> sServices = new HashMap<>();
    private static final HashMap<String, SharedPreferences> sPrefs = new HashMap<>();
    private static AssetManager sAssets;
    private static Resources sResources;

    public AssetManager getAssets() { synchronized (Context.class) { if (sAssets == null) sAssets = new AssetManager(); return sAssets; } }
    public Resources getResources() { synchronized (Context.class) { if (sResources == null) sResources = new Resources(getAssets()); return sResources; } }
    public String getPackageName() { return husk.Native.packageName(); }
    public String getPackageCodePath() { return getApplicationInfo().sourceDir; }
    public Context getApplicationContext() { return this; }
    public Looper getMainLooper() { return Looper.getMainLooper(); }
    public ClassLoader getClassLoader() { return getClass().getClassLoader(); }
    public ApplicationInfo getApplicationInfo() { return ApplicationInfo.self(); }
    public PackageManager getPackageManager() { return PackageManager.self(); }
    public ContentResolver getContentResolver() { return new ContentResolver(); }
    public Resources.Theme getTheme() { return null; }
    public void setTheme(int resid) {}
    public CharSequence getText(int id) { return getResources().getText(id); }
    public String getString(int id) { return getResources().getString(id); }
    public String getString(int id, Object... args) { return String.format(getString(id), args); }
    public int getColor(int id) { return 0; }

    private static File dir(String sub) { File f = new File(husk.Native.dataDir(), sub); f.mkdirs(); return f; }
    public File getFilesDir() { return dir("files"); }
    public File getCacheDir() { return dir("cache"); }
    public File getCodeCacheDir() { return dir("code_cache"); }
    public File getNoBackupFilesDir() { return dir("no_backup"); }
    public File getDataDir() { return new File(husk.Native.dataDir()); }
    public File getDir(String name, int mode) { return dir("app_" + name); }
    public File getDatabasePath(String name) { return new File(dir("databases"), name); }
    public File getFileStreamPath(String name) { return new File(getFilesDir(), name); }
    public File getExternalFilesDir(String type) {
        File f = new File(husk.Native.externalDir(), "Android/data/" + getPackageName() + "/files" + (type == null ? "" : "/" + type));
        f.mkdirs();
        return f;
    }
    public File[] getExternalFilesDirs(String type) { return new File[] { getExternalFilesDir(type) }; }
    public File getExternalCacheDir() { File f = new File(husk.Native.externalDir(), "Android/data/" + getPackageName() + "/cache"); f.mkdirs(); return f; }
    public File getObbDir() { File f = new File(husk.Native.externalDir(), "Android/obb/" + getPackageName()); f.mkdirs(); return f; }
    public java.io.FileInputStream openFileInput(String name) throws java.io.FileNotFoundException { return new java.io.FileInputStream(new File(getFilesDir(), name)); }
    public java.io.FileOutputStream openFileOutput(String name, int mode) throws java.io.FileNotFoundException {
        return new java.io.FileOutputStream(new File(getFilesDir(), name), (mode & MODE_APPEND) != 0);
    }
    public boolean deleteFile(String name) { return new File(getFilesDir(), name).delete(); }
    public String[] fileList() { String[] l = getFilesDir().list(); return l == null ? new String[0] : l; }

    public SharedPreferences getSharedPreferences(String name, int mode) {
        synchronized (sPrefs) {
            SharedPreferences p = sPrefs.get(name);
            if (p == null) { p = new HuskSharedPreferences(new File(dir("shared_prefs"), name + ".xml")); sPrefs.put(name, p); }
            return p;
        }
    }

    public Object getSystemService(String name) {
        synchronized (sServices) {
            Object s = sServices.get(name);
            if (s != null) return s;
            switch (name) {
            case AUDIO_SERVICE: s = new android.media.AudioManager(); break;
            case VIBRATOR_SERVICE: s = new android.os.Vibrator(); break;
            case WINDOW_SERVICE: s = new android.view.WindowManagerImpl(); break;
            case CLIPBOARD_SERVICE: s = new ClipboardManager(); break;
            case INPUT_METHOD_SERVICE: s = new android.view.inputmethod.InputMethodManager(); break;
            case SENSOR_SERVICE: s = new android.hardware.SensorManager(); break;
            case POWER_SERVICE: s = new android.os.PowerManager(); break;
            case ACTIVITY_SERVICE: s = new android.app.ActivityManager(); break;
            default: return null;
            }
            sServices.put(name, s);
            return s;
        }
    }
    @SuppressWarnings("unchecked")
    public final <T> T getSystemService(Class<T> c) {
        String n = c.getSimpleName();
        switch (n) {
        case "AudioManager": return (T) getSystemService(AUDIO_SERVICE);
        case "Vibrator": return (T) getSystemService(VIBRATOR_SERVICE);
        case "WindowManager": return (T) getSystemService(WINDOW_SERVICE);
        case "ClipboardManager": return (T) getSystemService(CLIPBOARD_SERVICE);
        case "InputMethodManager": return (T) getSystemService(INPUT_METHOD_SERVICE);
        case "SensorManager": return (T) getSystemService(SENSOR_SERVICE);
        case "PowerManager": return (T) getSystemService(POWER_SERVICE);
        default: return null;
        }
    }
    public void startActivity(Intent i) {
        if (i != null && i.getData() != null) husk.Native.openUrl(i.getData().toString());
    }
    public void startActivity(Intent i, android.os.Bundle opts) { startActivity(i); }
    public void sendBroadcast(Intent i) {}
    public Intent registerReceiver(BroadcastReceiver r, IntentFilter f) { return null; }
    public Intent registerReceiver(BroadcastReceiver r, IntentFilter f, int flags) { return null; }
    public void unregisterReceiver(BroadcastReceiver r) {}
    public ComponentName startService(Intent i) { return null; }
    public boolean stopService(Intent i) { return false; }
    public boolean bindService(Intent i, ServiceConnection c, int flags) { return false; }
    public void unbindService(ServiceConnection c) {}
    public int checkSelfPermission(String p) { return 0; }
    public int checkCallingOrSelfPermission(String p) { return 0; }
    public void registerComponentCallbacks(ComponentCallbacks c) {}
    public void unregisterComponentCallbacks(ComponentCallbacks c) {}
    public boolean isRestricted() { return false; }
}
