package android.webkit;

@SuppressWarnings({"unchecked", "rawtypes", "deprecation"})
public abstract class ServiceWorkerWebSettings {
    private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
    public ServiceWorkerWebSettings() {}
    public abstract boolean getAllowContentAccess();
    public abstract boolean getAllowFileAccess();
    public abstract boolean getBlockNetworkLoads();
    public abstract int getCacheMode();
    public abstract void setAllowContentAccess(boolean p0);
    public abstract void setAllowFileAccess(boolean p0);
    public abstract void setBlockNetworkLoads(boolean p0);
    public abstract void setCacheMode(int p0);
    static final class Husk extends ServiceWorkerWebSettings {
        public boolean getAllowContentAccess() { return false; }
        public boolean getAllowFileAccess() { return false; }
        public boolean getBlockNetworkLoads() { return false; }
        public int getCacheMode() { return 0; }
        public void setAllowContentAccess(boolean p0) {}
        public void setAllowFileAccess(boolean p0) {}
        public void setBlockNetworkLoads(boolean p0) {}
        public void setCacheMode(int p0) {}
    }
    private static ServiceWorkerWebSettings sInstance;
}
