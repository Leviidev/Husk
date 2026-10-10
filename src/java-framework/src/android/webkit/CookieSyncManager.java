package android.webkit;

import android.content.Context;

@Deprecated
public final class CookieSyncManager extends WebSyncManager {
    private static CookieSyncManager sRef;
    private CookieSyncManager() { super(null, null); }
    public static synchronized CookieSyncManager getInstance() { if (sRef == null) throw new IllegalStateException("CookieSyncManager::createInstance() needs to be called before CookieSyncManager::getInstance()"); return sRef; }
    public static synchronized CookieSyncManager createInstance(Context context) { if (sRef == null) sRef = new CookieSyncManager(); return sRef; }
    @Override public void sync() { CookieManager.getInstance().flush(); }
    protected void syncFromRamToFlash() { CookieManager.getInstance().flush(); }
    @Override public void resetSync() {}
    @Override public void startSync() {}
    @Override public void stopSync() {}
}
