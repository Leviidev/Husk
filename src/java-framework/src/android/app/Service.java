package android.app;

import android.content.Intent;

public abstract class Service extends android.content.ContextWrapper implements android.content.ComponentCallbacks2 {
    public static final int START_CONTINUATION_MASK = 15, START_STICKY_COMPATIBILITY = 0, START_STICKY = 1, START_NOT_STICKY = 2, START_REDELIVER_INTENT = 3,
        START_FLAG_REDELIVERY = 1, START_FLAG_RETRY = 2, STOP_FOREGROUND_REMOVE = 1, STOP_FOREGROUND_DETACH = 2, STOP_FOREGROUND_LEGACY = 0;
    public Service() { super(null); }
    public final void huskAttach(android.content.Context base) { attachBaseContext(base); }
    public final Application getApplication() { return Application.huskGet(); }
    public void onCreate() {}
    @Deprecated public void onStart(Intent i, int id) {}
    public int onStartCommand(Intent i, int flags, int id) { onStart(i, id); return START_STICKY; }
    public void onDestroy() {}
    public void onConfigurationChanged(android.content.res.Configuration c) {}
    public void onLowMemory() {}
    public void onTrimMemory(int level) {}
    public abstract android.os.IBinder onBind(Intent i);
    public boolean onUnbind(Intent i) { return false; }
    public void onRebind(Intent i) {}
    public void onTaskRemoved(Intent root) {}
    public void onTimeout(int startId) {}
    public final void stopSelf() { husk.Services.stopSelf(this); }
    public final void stopSelf(int id) { stopSelf(); }
    public final boolean stopSelfResult(int id) { stopSelf(); return true; }
    public final void setForeground(boolean f) {}
    public final void startForeground(int id, Notification n) {}
    public final void startForeground(int id, Notification n, int type) {}
    public final void stopForeground(boolean remove) {}
    public final void stopForeground(int flags) {}
    public final int getForegroundServiceType() { return 0; }
    protected void dump(java.io.FileDescriptor fd, java.io.PrintWriter w, String[] args) {}
}
