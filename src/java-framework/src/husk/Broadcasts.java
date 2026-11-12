package husk;

import android.content.*;
import android.os.Handler;
import android.os.Looper;
import java.util.ArrayList;

/** Broadcasts within the app: to receivers registered at run time and those in the manifest; the system's own few (battery) answered. */
public final class Broadcasts {
    private static final ArrayList<Object[]> sReceivers = new ArrayList<>();
    private Broadcasts() {}
    public static void huskReset() { synchronized (sReceivers) { sReceivers.clear(); } }
    public static Intent register(BroadcastReceiver r, IntentFilter f) {
        synchronized (sReceivers) { sReceivers.add(new Object[] { r, f, Processes.current() }); }
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
    public static void send(Context from, Intent i) { send(from, i, false); }
    /** system: a broadcast Husk makes up as the system would (boot completed at every start, where a phone sends it once after
     *  boot, in the background): a receiver throwing on it is logged rather than ending the app the user opened (Gmail's
     *  TimeZoneDataBaseUpgradeReceiver) */
    public static void send(Context from, Intent i, boolean system) {
        ArrayList<Object[]> targets = new ArrayList<>();
        synchronized (sReceivers) { for (Object[] e : sReceivers) if (((IntentFilter) e[1]).hasAction(i.getAction())) targets.add(e); }
        for (Manifest.Component c : Manifest.receivers) {
            boolean match = i.getComponent() != null && c.name.equals(i.getComponent().getClassName());
            for (Manifest.Filter f : c.filters) if (f.actions.contains(i.getAction())) match = true;
            if (!match) continue;
            if (c.enabled) targets.add(new Object[] { c, null, Processes.component(c) });
        }
        Handler h = new Handler(Looper.getMainLooper());
        for (Object[] target : targets) h.post(() -> {
            Processes.State process = (Processes.State)target[2]; process.start();
            try (Processes.Scope ignored = Processes.enter(process)) {
                BroadcastReceiver r;
                try {
                    r = target[0] instanceof BroadcastReceiver ? (BroadcastReceiver)target[0]
                        : process.factory.instantiateReceiver(AppRunner.classLoader(), ((Manifest.Component)target[0]).name, i);
                } catch (Exception e) { android.util.Log.w("Husk", "receiver " + target[0] + ": " + e); return; }
                Intent received = new Intent(i);
                if (i.getExtras() != null) received.replaceExtras((android.os.Bundle)Processes.copy(i.getExtras(), AppRunner.classLoader()));
                Context ctx = process.application != null ? process.application : from;
                if (!system) { r.onReceive(ctx, received); return; }
                try { r.onReceive(ctx, received); } catch (RuntimeException e) { android.util.Log.w("Husk", "receiver " + r.getClass().getName() + " threw on " + i.getAction(), e); }
            }
        });
    }
}
