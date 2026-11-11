package android.app;

import android.content.pm.ApplicationInfo;

/**
 * The hidden record of the app's APK that packers and loader shells (APKVISION, Jiagu-style stubs) reach by reflection through
 * ContextImpl.mPackageInfo or ActivityThread.mPackages: they swap mClassLoader for one over the real app's code, clear
 * mApplication and call makeApplication for the real Application. The fields keep AOSP's names; Husk loads the app's
 * activities, services and providers through mClassLoader, so a loader the app installs here takes effect.
 */
public final class LoadedApk {
    private static LoadedApk sSelf;
    public static synchronized void resetSession() { sSelf = null; }

    final ActivityThread mActivityThread;
    final String mPackageName;
    ApplicationInfo mApplicationInfo;
    String mAppDir, mResDir, mDataDir, mLibDir;
    String[] mSplitAppDirs, mSplitResDirs;
    ClassLoader mBaseClassLoader;
    ClassLoader mClassLoader;
    Application mApplication;
    android.content.res.Resources mResources;
    boolean mIncludeCode = true;
    final android.util.ArrayMap<android.content.Context, android.util.ArrayMap<android.content.BroadcastReceiver, Object>> mReceivers = new android.util.ArrayMap<>();
    final android.util.ArrayMap<android.content.Context, android.util.ArrayMap<android.content.ServiceConnection, Object>> mServices = new android.util.ArrayMap<>();

    private LoadedApk() {
        mActivityThread = ActivityThread.currentActivityThread();
        mApplicationInfo = ApplicationInfo.self();
        mPackageName = mApplicationInfo.packageName;
        mAppDir = mResDir = mApplicationInfo.sourceDir;
        mSplitAppDirs = mSplitResDirs = mApplicationInfo.splitSourceDirs;
        mDataDir = mApplicationInfo.dataDir;
        mLibDir = mApplicationInfo.nativeLibraryDir;
    }
    public static synchronized LoadedApk self() { if (sSelf == null) sSelf = new LoadedApk(); return sSelf; }

    /** The loader the app's components come from: one the app put here, or the app's own. */
    public ClassLoader getClassLoader() { return mClassLoader != null ? mClassLoader : husk.AppRunner.baseClassLoader(); }
    /** Husk: the loader the app is launched with, before the app may replace it. */
    public void huskSetClassLoader(ClassLoader cl) { mBaseClassLoader = mClassLoader = cl; }
    public void huskSetApplication(Application app) { mApplication = app; }

    public Application makeApplication(boolean forceDefaultAppClass, Instrumentation instrumentation) {
        if (mApplication != null) return mApplication;
        String cls = forceDefaultAppClass ? null : mApplicationInfo.className;
        mApplication = husk.AppRunner.makeApplication(cls, getClassLoader());
        return mApplication;
    }
    public Application getApplication() { return mApplication != null ? mApplication : husk.AppRunner.application(); }
    public String getPackageName() { return mPackageName; }
    public ApplicationInfo getApplicationInfo() { return mApplicationInfo; }
    public String getAppDir() { return mAppDir; }
    public String getResDir() { return mResDir; }
    public String getDataDir() { return mDataDir; }
    public String getLibDir() { return mLibDir; }
    public String[] getSplitAppDirs() { return mSplitAppDirs; }
    public String[] getSplitResDirs() { return mSplitResDirs; }
    public android.content.res.Resources getResources() { return mResources != null ? mResources : husk.ContextImpl.app().getResources(); }
    public android.content.res.AssetManager getAssets() { return getResources().getAssets(); }
    public boolean isSecurityViolation() { return false; }
    public java.io.File getDataDirFile() { return new java.io.File(mDataDir); }
    public void updateApplicationInfo(ApplicationInfo ai, java.util.List<String> oldPaths) { if (ai != null) mApplicationInfo = ai; }
}
