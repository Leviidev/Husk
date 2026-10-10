package android.app;
public abstract class Service extends android.content.ContextWrapper {
    public Service() { super(null); }
    public void onCreate() {} public void onDestroy() {}
    public abstract android.os.IBinder onBind(android.content.Intent i);
}
