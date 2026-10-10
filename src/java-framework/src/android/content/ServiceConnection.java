package android.content;
public interface ServiceConnection { void onServiceConnected(ComponentName n, android.os.IBinder b); void onServiceDisconnected(ComponentName n); }
