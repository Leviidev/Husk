package android.content;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.AttributeSet;
import java.io.File;

/**
 * A context. Husk's base context is husk.ContextImpl (the app's resources, files and services); wrappers delegate to it as on Android.
 * The methods here that are not abstract on Android are written in terms of the ones a wrapper overrides.
 */
public abstract class Context {
    public static final int MODE_PRIVATE = 0, MODE_APPEND = 0x8000, MODE_WORLD_READABLE = 1, MODE_WORLD_WRITEABLE = 2, MODE_MULTI_PROCESS = 4,
        MODE_ENABLE_WRITE_AHEAD_LOGGING = 8, MODE_NO_LOCALIZED_COLLATORS = 16;
    public static final int BIND_AUTO_CREATE = 1, BIND_DEBUG_UNBIND = 2, BIND_NOT_FOREGROUND = 4, BIND_ABOVE_CLIENT = 8, BIND_IMPORTANT = 64,
        BIND_WAIVE_PRIORITY = 32, BIND_ADJUST_WITH_ACTIVITY = 128, BIND_INCLUDE_CAPABILITIES = 4096;
    public static final int CONTEXT_INCLUDE_CODE = 1, CONTEXT_IGNORE_SECURITY = 2, CONTEXT_RESTRICTED = 4, RECEIVER_EXPORTED = 2, RECEIVER_NOT_EXPORTED = 4,
        RECEIVER_VISIBLE_TO_INSTANT_APPS = 1;
    public static final String AUDIO_SERVICE = "audio", VIBRATOR_SERVICE = "vibrator", WINDOW_SERVICE = "window", CLIPBOARD_SERVICE = "clipboard",
        INPUT_METHOD_SERVICE = "input_method", SENSOR_SERVICE = "sensor", POWER_SERVICE = "power", ACTIVITY_SERVICE = "activity",
        CONNECTIVITY_SERVICE = "connectivity", WIFI_SERVICE = "wifi", LOCATION_SERVICE = "location", NOTIFICATION_SERVICE = "notification",
        LAYOUT_INFLATER_SERVICE = "layout_inflater", DISPLAY_SERVICE = "display", UI_MODE_SERVICE = "uimode", KEYGUARD_SERVICE = "keyguard",
        TELEPHONY_SERVICE = "phone", STORAGE_SERVICE = "storage", INPUT_SERVICE = "input", VIBRATOR_MANAGER_SERVICE = "vibrator_manager",
        ALARM_SERVICE = "alarm", ACCESSIBILITY_SERVICE = "accessibility", ACCOUNT_SERVICE = "account", DOWNLOAD_SERVICE = "download",
        JOB_SCHEDULER_SERVICE = "jobscheduler", USER_SERVICE = "user", APP_OPS_SERVICE = "appops", BATTERY_SERVICE = "batterymanager",
        CAMERA_SERVICE = "camera", SEARCH_SERVICE = "search", TEXT_SERVICES_MANAGER_SERVICE = "textservices", WALLPAPER_SERVICE = "wallpaper",
        AUTOFILL_MANAGER_SERVICE = "autofill", CAPTIONING_SERVICE = "captioning", MEDIA_SESSION_SERVICE = "media_session",
        TEXT_CLASSIFICATION_SERVICE = "textclassification", LOCALE_SERVICE = "locale", USAGE_STATS_SERVICE = "usagestats", BLUETOOTH_SERVICE = "bluetooth",
        SHORTCUT_SERVICE = "shortcut", APPWIDGET_SERVICE = "appwidget", NSD_SERVICE = "servicediscovery", DEVICE_POLICY_SERVICE = "device_policy",
        CONTENT_CAPTURE_MANAGER_SERVICE = "content_capture", MEDIA_ROUTER_SERVICE = "media_router", FINGERPRINT_SERVICE = "fingerprint",
        BIOMETRIC_SERVICE = "biometric", HARDWARE_PROPERTIES_SERVICE = "hardware_properties", NETWORK_STATS_SERVICE = "netstats",
        CLIPBOARD_SERVICE_ = "clipboard", GAME_SERVICE = "game", CROSS_PROFILE_APPS_SERVICE = "crossprofileapps", DISPLAY_HASH_SERVICE = "display_hash";

    public abstract AssetManager getAssets();
    public abstract Resources getResources();
    public abstract PackageManager getPackageManager();
    public abstract ContentResolver getContentResolver();
    public abstract Looper getMainLooper();
    public abstract Context getApplicationContext();
    public abstract void setTheme(int resid);
    public abstract Resources.Theme getTheme();
    public abstract ClassLoader getClassLoader();
    public abstract String getPackageName();
    public abstract ApplicationInfo getApplicationInfo();
    public abstract String getPackageResourcePath();
    public abstract String getPackageCodePath();
    public abstract SharedPreferences getSharedPreferences(String name, int mode);
    public abstract Object getSystemService(String name);
    public abstract File getFilesDir();
    public abstract File getCacheDir();
    public abstract File getDataDir();
    public abstract File getDir(String name, int mode);
    public abstract File getExternalFilesDir(String type);
    public abstract void startActivity(Intent intent);
    public abstract void sendBroadcast(Intent intent);
    public abstract Intent registerReceiver(BroadcastReceiver r, IntentFilter f);
    public abstract void unregisterReceiver(BroadcastReceiver r);
    public abstract ComponentName startService(Intent service);
    public abstract boolean stopService(Intent service);
    public abstract boolean bindService(Intent service, ServiceConnection conn, int flags);
    public abstract void unbindService(ServiceConnection conn);
    public abstract int checkPermission(String perm, int pid, int uid);
    public abstract Context createPackageContext(String pkg, int flags) throws PackageManager.NameNotFoundException;
    public abstract Context createConfigurationContext(Configuration c);

    // ---- in terms of the above
    public final CharSequence getText(int id) { return getResources().getText(id); }
    public final String getString(int id) { return getResources().getString(id); }
    public final String getString(int id, Object... args) { return getResources().getString(id, args); }
    public final int getColor(int id) { return getResources().getColor(id, getTheme()); }
    public final Drawable getDrawable(int id) { return getResources().getDrawable(id, getTheme()); }
    public final ColorStateList getColorStateList(int id) { return getResources().getColorStateList(id, getTheme()); }
    public final TypedArray obtainStyledAttributes(int[] attrs) { return getTheme().obtainStyledAttributes(attrs); }
    public final TypedArray obtainStyledAttributes(int resid, int[] attrs) { return getTheme().obtainStyledAttributes(resid, attrs); }
    public final TypedArray obtainStyledAttributes(AttributeSet set, int[] attrs) { return getTheme().obtainStyledAttributes(set, attrs, 0, 0); }
    public final TypedArray obtainStyledAttributes(AttributeSet set, int[] attrs, int defStyleAttr, int defStyleRes) { return getTheme().obtainStyledAttributes(set, attrs, defStyleAttr, defStyleRes); }
    @SuppressWarnings("unchecked")
    public final <T> T getSystemService(Class<T> c) { String n = getSystemServiceName(c); return n == null ? null : (T) getSystemService(n); }
    public String getSystemServiceName(Class<?> c) { return husk.ContextImpl.serviceName(c); }
    public String getOpPackageName() { return getPackageName(); }
    public String getAttributionTag() { return null; }
    public String getBasePackageName() { return getPackageName(); }
    public java.util.concurrent.Executor getMainExecutor() { final Handler h = new Handler(getMainLooper()); return h::post; }
    public File getCodeCacheDir() { return sub("code_cache"); }
    public File getNoBackupFilesDir() { return sub("no_backup"); }
    public File getObbDir() { return getExternalFilesDir(null).getParentFile(); }
    public File[] getObbDirs() { return new File[] { getObbDir() }; }
    public File getExternalCacheDir() { File f = new File(getExternalFilesDir(null).getParentFile(), "cache"); f.mkdirs(); return f; }
    public File[] getExternalCacheDirs() { return new File[] { getExternalCacheDir() }; }
    public File[] getExternalFilesDirs(String type) { return new File[] { getExternalFilesDir(type) }; }
    public File[] getExternalMediaDirs() { return new File[] { getExternalFilesDir(null) }; }
    private File sub(String n) { File f = new File(getDataDir(), n); f.mkdirs(); return f; }
    public File getDatabasePath(String name) { if (name.startsWith("/")) return new File(name); File d = sub("databases"); return new File(d, name); }
    public String[] databaseList() { String[] l = sub("databases").list(); return l == null ? new String[0] : l; }
    public boolean deleteDatabase(String name) { return android.database.sqlite.SQLiteDatabase.deleteDatabase(getDatabasePath(name)); }
    public android.database.sqlite.SQLiteDatabase openOrCreateDatabase(String name, int mode, android.database.sqlite.SQLiteDatabase.CursorFactory factory) { return openOrCreateDatabase(name, mode, factory, null); }
    public android.database.sqlite.SQLiteDatabase openOrCreateDatabase(String name, int mode, android.database.sqlite.SQLiteDatabase.CursorFactory factory, android.database.DatabaseErrorHandler errorHandler) {
        File f = getDatabasePath(name);
        if (f.getParentFile() != null) f.getParentFile().mkdirs();
        int flags = android.database.sqlite.SQLiteDatabase.CREATE_IF_NECESSARY | ((mode & MODE_ENABLE_WRITE_AHEAD_LOGGING) != 0 ? android.database.sqlite.SQLiteDatabase.ENABLE_WRITE_AHEAD_LOGGING : 0);
        return android.database.sqlite.SQLiteDatabase.openDatabase(f.getPath(), factory, flags, errorHandler);
    }
    public File getFileStreamPath(String name) { return new File(getFilesDir(), name); }
    public java.io.FileInputStream openFileInput(String name) throws java.io.FileNotFoundException { return new java.io.FileInputStream(getFileStreamPath(name)); }
    public java.io.FileOutputStream openFileOutput(String name, int mode) throws java.io.FileNotFoundException { return new java.io.FileOutputStream(getFileStreamPath(name), (mode & MODE_APPEND) != 0); }
    public boolean deleteFile(String name) { return getFileStreamPath(name).delete(); }
    public String[] fileList() { String[] l = getFilesDir().list(); return l == null ? new String[0] : l; }
    public boolean deleteSharedPreferences(String name) { return new File(sub("shared_prefs"), name + ".xml").delete(); }
    public boolean moveSharedPreferencesFrom(Context src, String name) { return true; }
    public boolean moveDatabaseFrom(Context src, String name) { return true; }
    public void startActivity(Intent intent, Bundle options) { startActivity(intent); }
    public void startActivities(Intent[] intents) { for (Intent i : intents) startActivity(i); }
    public void startActivities(Intent[] intents, Bundle options) { startActivities(intents); }
    public void sendBroadcast(Intent i, String perm) { sendBroadcast(i); }
    public void sendOrderedBroadcast(Intent i, String perm) { sendBroadcast(i); }
    public void sendStickyBroadcast(Intent i) { sendBroadcast(i); }
    public Intent registerReceiver(BroadcastReceiver r, IntentFilter f, int flags) { return registerReceiver(r, f); }
    public Intent registerReceiver(BroadcastReceiver r, IntentFilter f, String perm, Handler h) { return registerReceiver(r, f); }
    public Intent registerReceiver(BroadcastReceiver r, IntentFilter f, String perm, Handler h, int flags) { return registerReceiver(r, f); }
    public ComponentName startForegroundService(Intent service) { return startService(service); }
    public boolean bindService(Intent service, int flags, java.util.concurrent.Executor e, ServiceConnection c) { return bindService(service, c, flags); }
    public int checkCallingOrSelfPermission(String p) { return checkPermission(p, android.os.Process.myPid(), android.os.Process.myUid()); }
    public int checkSelfPermission(String p) { return checkPermission(p, android.os.Process.myPid(), android.os.Process.myUid()); }
    public int checkCallingPermission(String p) { return checkSelfPermission(p); }
    public int checkUriPermission(android.net.Uri u, int pid, int uid, int flags) { return PackageManager.PERMISSION_GRANTED; }
    public int checkCallingUriPermission(android.net.Uri u, int flags) { return PackageManager.PERMISSION_GRANTED; }
    public void enforceCallingOrSelfPermission(String p, String m) {}
    public void enforcePermission(String p, int pid, int uid, String m) {}
    public void grantUriPermission(String pkg, android.net.Uri u, int flags) {}
    public void revokeUriPermission(android.net.Uri u, int flags) {}
    public void registerComponentCallbacks(ComponentCallbacks c) { getApplicationContext().registerComponentCallbacks(c); }
    public void unregisterComponentCallbacks(ComponentCallbacks c) { getApplicationContext().unregisterComponentCallbacks(c); }
    public boolean isRestricted() { return false; }
    public boolean isDeviceProtectedStorage() { return false; }
    public boolean isUiContext() { return true; }
    public Context createDeviceProtectedStorageContext() { return this; }
    public Context createDisplayContext(android.view.Display d) { return this; }
    public Context createWindowContext(int type, Bundle options) { return this; }
    public Context createAttributionContext(String tag) { return this; }
    public Context createContextForSplit(String split) { return this; }
    public android.view.Display getDisplay() { return ((android.view.WindowManager) getSystemService(WINDOW_SERVICE)).getDefaultDisplay(); }
    public int getDisplayId() { return 0; }
    public void updateDisplay(int id) {}
    public boolean isDeviceProtectedStorageHusk() { return false; }
    public Context getBaseContextHusk() { return this; }
    public int getUserId() { return 0; }
    public void revokeSelfPermissionOnKill(String p) {}
}
