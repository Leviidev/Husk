package husk;

import android.content.*;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Looper;
import java.io.File;
import java.util.HashMap;

/** The app's base context: one for the process, as ContextImpl is on Android (activities and the application wrap it). */
public final class ContextImpl extends Context {
    private static ContextImpl sApp;
    private static Context sApplication;
    private static final HashMap<String, Object> sServices = new HashMap<>();
    private static final HashMap<String, SharedPreferences> sPrefs = new HashMap<>();
    private static AssetManager sAssets;
    private static Resources sResources;
    private final Resources mResources;
    private Resources.Theme mTheme;
    private int mThemeId;

    private ContextImpl(Resources r) { mResources = r; }
    public static synchronized ContextImpl app() {
        if (sApp == null) {
            if (sAssets == null) sAssets = new AssetManager();
            if (sResources == null) sResources = new Resources(sAssets);
            sApp = new ContextImpl(sResources);
        }
        return sApp;
    }
    /** A base context of its own (an activity's: its own theme), sharing the app's resources. */
    public static ContextImpl forActivity() { app(); return new ContextImpl(sResources); }
    public static void setApplication(Context a) { sApplication = a; }
    /** The theme an activity or the app gets when the manifest names none: what the target SDK selects on Android. */
    public static int defaultTheme() {
        Manifest.read();
        if (Manifest.appTheme != 0) return Manifest.appTheme;
        int t = Manifest.targetSdk;
        if (t < 11) return IR.style.Theme;
        if (t < 14) return IR.style.Theme_Holo;
        if (t < 24) return IR.style.Theme_DeviceDefault;
        return IR.style.Theme_DeviceDefault_Light_DarkActionBar;
    }

    @Override public AssetManager getAssets() { return mResources.getAssets(); }
    @Override public Resources getResources() { return mResources; }
    @Override public PackageManager getPackageManager() { return PackageManager.self(); }
    @Override public ContentResolver getContentResolver() { return ContentResolver.self(); }
    @Override public Looper getMainLooper() { return Looper.getMainLooper(); }
    @Override public Context getApplicationContext() { return sApplication != null ? sApplication : this; }
    @Override public void setTheme(int resid) { if (mThemeId != resid) { mThemeId = resid; mTheme = null; } }
    @Override public Resources.Theme getTheme() {
        if (mTheme == null) {
            if (mThemeId == 0) mThemeId = defaultTheme();
            mTheme = mResources.newTheme();
            mTheme.applyStyle(mThemeId, true);
        }
        return mTheme;
    }
    @Override public ClassLoader getClassLoader() { return ContextImpl.class.getClassLoader(); }
    @Override public String getPackageName() { return Native.packageName(); }
    @Override public ApplicationInfo getApplicationInfo() { return ApplicationInfo.self(); }
    @Override public String getPackageResourcePath() { return getApplicationInfo().sourceDir; }
    @Override public String getPackageCodePath() { return getApplicationInfo().sourceDir; }
    private static File dir(String sub) { File f = new File(Native.dataDir(), sub); f.mkdirs(); return f; }
    @Override public File getFilesDir() { return dir("files"); }
    @Override public File getCacheDir() { return dir("cache"); }
    @Override public File getDataDir() { return new File(Native.dataDir()); }
    @Override public File getDir(String name, int mode) { return dir("app_" + name); }
    @Override public File getExternalFilesDir(String type) {
        File f = new File(Native.externalDir(), "Android/data/" + getPackageName() + "/files" + (type == null ? "" : "/" + type));
        f.mkdirs();
        return f;
    }
    @Override public SharedPreferences getSharedPreferences(String name, int mode) {
        if (name == null) name = "null";
        synchronized (sPrefs) {
            SharedPreferences p = sPrefs.get(name);
            if (p == null) { p = new android.content.HuskSharedPreferences(new File(dir("shared_prefs"), name + ".xml")); sPrefs.put(name, p); }
            return p;
        }
    }
    @Override public void startActivity(Intent i) { AppRunner.startActivity(this, i, -1, null); }
    @Override public void sendBroadcast(Intent i) { Broadcasts.send(this, i); }
    @Override public Intent registerReceiver(BroadcastReceiver r, IntentFilter f) { return Broadcasts.register(r, f); }
    @Override public void unregisterReceiver(BroadcastReceiver r) { Broadcasts.unregister(r); }
    @Override public ComponentName startService(Intent s) { return Services.start(this, s); }
    @Override public boolean stopService(Intent s) { return Services.stop(s); }
    @Override public boolean bindService(Intent s, ServiceConnection c, int flags) { return Services.bind(this, s, c); }
    @Override public void unbindService(ServiceConnection c) { Services.unbind(c); }
    @Override public int checkPermission(String p, int pid, int uid) { return PackageManager.PERMISSION_GRANTED; }
    @Override public Context createPackageContext(String pkg, int flags) throws PackageManager.NameNotFoundException {
        if (pkg.equals(getPackageName()) || "android".equals(pkg)) return this;
        throw new PackageManager.NameNotFoundException(pkg);
    }
    @Override public Context createConfigurationContext(Configuration c) {
        Resources r = new Resources(getAssets(), null, c);
        return new ContextImpl(r);
    }
    private final java.util.ArrayList<ComponentCallbacks> mCallbacks = new java.util.ArrayList<>();
    @Override public void registerComponentCallbacks(ComponentCallbacks c) { mCallbacks.add(c); }
    @Override public void unregisterComponentCallbacks(ComponentCallbacks c) { mCallbacks.remove(c); }

    @Override public Object getSystemService(String name) {
        if (name == null) return null;
        if (LAYOUT_INFLATER_SERVICE.equals(name)) {
            synchronized (sServices) { Object s = sServices.get(name); if (s == null) { s = new PhoneLayoutInflater(getApplicationContext() != null ? getApplicationContext() : this); sServices.put(name, s); } return s; }
        }
        if (WINDOW_SERVICE.equals(name)) return new android.view.WindowManagerImpl(this);
        synchronized (sServices) {
            Object s = sServices.get(name);
            if (s != null) return s;
            s = Services.system(name, this);
            if (s != null) sServices.put(name, s);
            return s;
        }
    }
    private static final HashMap<Class<?>, String> sNames = new HashMap<>();
    static {
        String[][] m = {
            { "android.media.AudioManager", AUDIO_SERVICE }, { "android.os.Vibrator", VIBRATOR_SERVICE }, { "android.view.WindowManager", WINDOW_SERVICE },
            { "android.content.ClipboardManager", CLIPBOARD_SERVICE }, { "android.view.inputmethod.InputMethodManager", INPUT_METHOD_SERVICE },
            { "android.hardware.SensorManager", SENSOR_SERVICE }, { "android.os.PowerManager", POWER_SERVICE }, { "android.app.ActivityManager", ACTIVITY_SERVICE },
            { "android.view.LayoutInflater", LAYOUT_INFLATER_SERVICE }, { "android.net.ConnectivityManager", CONNECTIVITY_SERVICE },
            { "android.app.NotificationManager", NOTIFICATION_SERVICE }, { "android.app.AlarmManager", ALARM_SERVICE },
            { "android.view.accessibility.AccessibilityManager", ACCESSIBILITY_SERVICE }, { "android.app.UiModeManager", UI_MODE_SERVICE },
            { "android.hardware.display.DisplayManager", DISPLAY_SERVICE }, { "android.app.KeyguardManager", KEYGUARD_SERVICE },
            { "android.os.UserManager", USER_SERVICE }, { "android.os.BatteryManager", BATTERY_SERVICE }, { "android.os.VibratorManager", VIBRATOR_MANAGER_SERVICE },
            { "android.telephony.TelephonyManager", TELEPHONY_SERVICE }, { "android.app.job.JobScheduler", JOB_SCHEDULER_SERVICE },
            { "android.view.autofill.AutofillManager", AUTOFILL_MANAGER_SERVICE }, { "android.app.AppOpsManager", APP_OPS_SERVICE },
            { "android.os.storage.StorageManager", STORAGE_SERVICE }, { "android.hardware.input.InputManager", INPUT_SERVICE },
            { "android.view.textclassifier.TextClassificationManager", TEXT_CLASSIFICATION_SERVICE }, { "android.app.LocaleManager", LOCALE_SERVICE },
            { "android.net.wifi.WifiManager", WIFI_SERVICE }, { "android.location.LocationManager", LOCATION_SERVICE },
        };
        for (String[] e : m) { try { sNames.put(Class.forName(e[0]), e[1]); } catch (Throwable t) {} }
    }
    public static String serviceName(Class<?> c) { String n = sNames.get(c); return n; }
}
