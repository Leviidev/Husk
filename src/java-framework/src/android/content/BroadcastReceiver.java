package android.content;

public abstract class BroadcastReceiver {
    public static class PendingResult {
        public final void setResultCode(int c) {} public final int getResultCode() { return 0; }
        public final void setResultData(String d) {} public final String getResultData() { return null; }
        public final void setResult(int c, String d, android.os.Bundle e) {} public final void finish() {}
        public final void abortBroadcast() {} public final boolean getAbortBroadcast() { return false; }
        public final android.os.Bundle getResultExtras(boolean make) { return make ? new android.os.Bundle() : null; }
    }
    private PendingResult mPending;
    private boolean mOrdered, mInitialSticky;
    public BroadcastReceiver() {}
    public abstract void onReceive(Context c, Intent i);
    public final PendingResult goAsync() { PendingResult p = mPending != null ? mPending : new PendingResult(); mPending = null; return p; }
    public final void setResultCode(int c) {} public final int getResultCode() { return 0; }
    public final void setResultData(String d) {} public final String getResultData() { return null; }
    public final void setResult(int c, String d, android.os.Bundle e) {}
    public final android.os.Bundle getResultExtras(boolean make) { return make ? new android.os.Bundle() : null; }
    public final void setResultExtras(android.os.Bundle b) {}
    public final void abortBroadcast() {} public final void clearAbortBroadcast() {} public final boolean getAbortBroadcast() { return false; }
    public final boolean isOrderedBroadcast() { return mOrdered; }
    public final boolean isInitialStickyBroadcast() { return mInitialSticky; }
    public final void setOrderedHint(boolean b) { mOrdered = b; }
    public final void setDebugUnregister(boolean b) {}
    public final boolean getDebugUnregister() { return false; }
    public android.os.IBinder peekService(Context c, Intent s) { return null; }
}
