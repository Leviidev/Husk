package android.database;
public abstract class ContentObserver {
    private final android.os.Handler mHandler;
    public ContentObserver(android.os.Handler h) { mHandler = h; }
    public boolean deliverSelfNotifications() { return false; }
    public void onChange(boolean self) {}
    public void onChange(boolean self, android.net.Uri u) { onChange(self); }
    public void onChange(boolean self, android.net.Uri u, int flags) { onChange(self, u); }
    public void onChange(boolean self, java.util.Collection<android.net.Uri> uris, int flags) { for (android.net.Uri u : uris) onChange(self, u, flags); }
    public final void dispatchChange(boolean self) { dispatchChange(self, null); }
    public final void dispatchChange(boolean self, android.net.Uri u) { if (mHandler == null) onChange(self, u); else mHandler.post(() -> onChange(self, u)); }
}
