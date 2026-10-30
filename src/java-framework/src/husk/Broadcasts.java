package husk;

import android.content.*;
import android.os.Handler;
import android.os.Looper;
import java.util.ArrayList;

/** Broadcasts within the app: to receivers registered at run time and those in the manifest; the system's own few (battery) answered. */
public final class Broadcasts {
    private static final ArrayList<Object[]> sReceivers = new ArrayList<>();
    private Broadcasts() {}
    public static Intent register(BroadcastReceiver r, IntentFilter f) {
        synchronized (sReceivers) { sReceivers.add(new Object[] { r, f }); }
        if (f != null && f.hasAction(Intent.ACTION_BATTERY_CHANGED)) return android.os.BatteryManager.huskStickyIntent();
        if (f != null && f.hasAction(android.net.ConnectivityManager.CONNECTIVITY_ACTION)) {
            Intent i = new Intent(android.net.ConnectivityManager.CONNECTIVITY_ACTION);
            i.putExtra(android.net.ConnectivityManager.EXTRA_NETWORK_INFO, new android.net.NetworkInfo());
            i.putExtra(android.net.ConnectivityManager.EXTRA_NO_CONNECTIVITY, false);
            return i;
        }
        return null;
    }
    public static void unregister(BroadcastReceiver r) { synchronized (sReceivers) { for (int i = sReceivers.size() - 1; i >= 0; i--) if (sReceivers.get(i)[0] == r) sReceivers.remove(i); } }
    public static void send(Context from, Intent i) {
        final Context ctx = AppRunner.application() != null ? AppRunner.application() : from;
        ArrayList<BroadcastReceiver> targets = new ArrayList<>();
        synchronized (sReceivers) { for (Object[] e : sReceivers) if (((IntentFilter) e[1]).hasAction(i.getAction())) targets.add((BroadcastReceiver) e[0]); }
        for (Manifest.Component c : Manifest.receivers) {
            boolean match = i.getComponent() != null && c.name.equals(i.getComponent().getClassName());
            for (Manifest.Filter f : c.filters) if (f.actions.contains(i.getAction())) match = true;
            if (!match) continue;
            try { targets.add((BroadcastReceiver) Class.forName(c.name, true, AppRunner.classLoader()).newInstance()); } catch (Throwable t) { android.util.Log.w("Husk", "receiver " + c.name + ": " + t); }
        }
        Handler h = new Handler(Looper.getMainLooper());
        for (BroadcastReceiver r : targets) h.post(() -> r.onReceive(ctx, i));
    }
}
