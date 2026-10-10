package android.window;
public interface OnBackInvokedDispatcher {
    int PRIORITY_DEFAULT = 0, PRIORITY_OVERLAY = 1000000, PRIORITY_SYSTEM_NAVIGATION_OBSERVER = -2;
    void registerOnBackInvokedCallback(int priority, OnBackInvokedCallback cb);
    void unregisterOnBackInvokedCallback(OnBackInvokedCallback cb);
}
