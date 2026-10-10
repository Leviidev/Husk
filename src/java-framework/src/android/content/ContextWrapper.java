package android.content;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Looper;
import java.io.File;

public class ContextWrapper extends Context {
    Context mBase;
    public ContextWrapper(Context base) { mBase = base; }
    protected void attachBaseContext(Context base) {
        if (mBase != null) throw new IllegalStateException("Base context already set");
        mBase = base;
    }
    public Context getBaseContext() { return mBase; }
    private Context b() { if (mBase == null) throw new IllegalStateException("ContextWrapper with no base context: " + getClass().getName()); return mBase; }
    @Override public AssetManager getAssets() { return b().getAssets(); }
    @Override public Resources getResources() { return b().getResources(); }
    @Override public PackageManager getPackageManager() { return b().getPackageManager(); }
    @Override public ContentResolver getContentResolver() { return b().getContentResolver(); }
    @Override public Looper getMainLooper() { return b().getMainLooper(); }
    @Override public Context getApplicationContext() { return b().getApplicationContext(); }
    @Override public void setTheme(int resid) { b().setTheme(resid); }
    @Override public Resources.Theme getTheme() { return b().getTheme(); }
    @Override public ClassLoader getClassLoader() { return b().getClassLoader(); }
    @Override public String getPackageName() { return b().getPackageName(); }
    @Override public ApplicationInfo getApplicationInfo() { return b().getApplicationInfo(); }
    @Override public String getPackageResourcePath() { return b().getPackageResourcePath(); }
    @Override public String getPackageCodePath() { return b().getPackageCodePath(); }
    @Override public SharedPreferences getSharedPreferences(String name, int mode) { return b().getSharedPreferences(name, mode); }
    @Override public Object getSystemService(String name) { return b().getSystemService(name); }
    @Override public String getSystemServiceName(Class<?> c) { return b().getSystemServiceName(c); }
    @Override public File getFilesDir() { return b().getFilesDir(); }
    @Override public File getCacheDir() { return b().getCacheDir(); }
    @Override public File getDataDir() { return b().getDataDir(); }
    @Override public File getDir(String name, int mode) { return b().getDir(name, mode); }
    @Override public File getExternalFilesDir(String type) { return b().getExternalFilesDir(type); }
    @Override public void startActivity(Intent intent) { b().startActivity(intent); }
    @Override public void startActivity(Intent intent, android.os.Bundle o) { b().startActivity(intent, o); }
    @Override public void sendBroadcast(Intent intent) { b().sendBroadcast(intent); }
    @Override public Intent registerReceiver(BroadcastReceiver r, IntentFilter f) { return b().registerReceiver(r, f); }
    @Override public void unregisterReceiver(BroadcastReceiver r) { b().unregisterReceiver(r); }
    @Override public ComponentName startService(Intent s) { return b().startService(s); }
    @Override public boolean stopService(Intent s) { return b().stopService(s); }
    @Override public boolean bindService(Intent s, ServiceConnection c, int flags) { return b().bindService(s, c, flags); }
    @Override public void unbindService(ServiceConnection c) { b().unbindService(c); }
    @Override public int checkPermission(String p, int pid, int uid) { return b().checkPermission(p, pid, uid); }
    @Override public Context createPackageContext(String pkg, int flags) throws PackageManager.NameNotFoundException { return b().createPackageContext(pkg, flags); }
    @Override public Context createConfigurationContext(Configuration c) { return b().createConfigurationContext(c); }
    @Override public void registerComponentCallbacks(ComponentCallbacks c) { b().registerComponentCallbacks(c); }
    @Override public void unregisterComponentCallbacks(ComponentCallbacks c) { b().unregisterComponentCallbacks(c); }
    @Override public File getDatabasePath(String name) { return b().getDatabasePath(name); }
    @Override public java.util.concurrent.Executor getMainExecutor() { return b().getMainExecutor(); }
}
