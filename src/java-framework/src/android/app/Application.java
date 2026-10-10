package android.app;
public class Application extends android.content.ContextWrapper {
    public Application() { super(null); }
    public void onCreate() {}
    public void onTerminate() {}
    public void onLowMemory() {}
    public void onTrimMemory(int level) {}
    public void onConfigurationChanged(android.content.res.Configuration c) {}
    public void registerActivityLifecycleCallbacks(Object cb) {}
    public static String getProcessName() { return husk.Native.packageName(); }
}
