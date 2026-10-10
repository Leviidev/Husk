package android.content;
public interface ServiceConnection {
    void onServiceConnected(ComponentName n, android.os.IBinder b);
    void onServiceDisconnected(ComponentName n);
    default void onBindingDied(ComponentName n) {}
    default void onNullBinding(ComponentName n) {}
}
