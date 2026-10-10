package husk;

import android.window.OnBackInvokedCallback;
import android.window.OnBackInvokedDispatcher;
import java.util.ArrayList;

/** OnBackInvokedDispatcher: the highest-priority callback registered last gets the back gesture. */
public final class BackDispatcher implements OnBackInvokedDispatcher {
    private final ArrayList<Object[]> mCallbacks = new ArrayList<>();
    public void registerOnBackInvokedCallback(int priority, OnBackInvokedCallback cb) { unregisterOnBackInvokedCallback(cb); mCallbacks.add(new Object[] { priority, cb }); }
    public void unregisterOnBackInvokedCallback(OnBackInvokedCallback cb) { for (int i = mCallbacks.size() - 1; i >= 0; i--) if (mCallbacks.get(i)[1] == cb) mCallbacks.remove(i); }
    /** True when a callback took it. */
    public boolean dispatch() {
        Object[] best = null;
        for (Object[] e : mCallbacks) if (best == null || (Integer) e[0] >= (Integer) best[0]) best = e;
        if (best == null || (Integer) best[0] < 0) return false;
        ((OnBackInvokedCallback) best[1]).onBackInvoked();
        return true;
    }
}
