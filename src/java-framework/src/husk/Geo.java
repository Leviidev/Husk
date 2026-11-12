package husk;

import android.location.Location;
import android.location.LocationListener;
import android.os.*;
import java.util.*;
import java.util.concurrent.Executor;
import java.util.function.Consumer;

/** Package-wide Core Location subscription broker. Every listener keeps its own callback executor/process and filters. */
public final class Geo {
    private static final ArrayList<Subscription> sSubscriptions = new ArrayList<>();
    private static final ArrayList<Subscription> sPending = new ArrayList<>();
    private static Handler sPoll;
    private static long sSequence;
    private static int sStatus = -1;
    private static Location sLast;
    private static boolean sRunning;
    private static final class Subscription {
        final Object owner;
        final String provider;
        final Executor executor;
        final Processes.State process = Processes.current();
        final LocationListener listener;
        final Consumer<Location> result;
        final long interval, deadline;
        final float distance;
        Location delivered;
        boolean active = true;
        Subscription(Object owner, String provider, long interval, float distance, Executor executor, LocationListener listener, Consumer<Location> result, long deadline) {
            this.owner = owner; this.provider = provider; this.interval = Math.max(0, interval); this.distance = Math.max(0, distance);
            this.executor = executor; this.listener = listener; this.result = result; this.deadline = deadline;
        }
        void callback(Runnable action) {
            executor.execute(() -> {
                synchronized (Geo.class) { if (!active) return; }
                try (Processes.Scope ignored = Processes.enter(process)) { action.run(); }
            });
        }
    }
    public static boolean supported(String p) { return "gps".equals(p) || "network".equals(p) || "fused".equals(p) || "passive".equals(p); }
    public static boolean enabled() { int s = GeoNative.status(); return s == GeoNative.PENDING || s == GeoNative.AUTHORIZED; }
    public static Executor executor(Looper looper) { Handler h = new Handler(looper != null ? looper : Looper.getMainLooper()); return r -> h.post(r); }
    public static void request(String provider, long interval, float distance, LocationListener listener, Executor executor) {
        if (!supported(provider)) throw new IllegalArgumentException("Unknown location provider " + provider);
        if (listener == null || executor == null) throw new IllegalArgumentException("listener and executor are required");
        add(new Subscription(listener, provider, interval, distance, executor, listener, null, Long.MAX_VALUE));
    }
    public static void current(String provider, CancellationSignal cancellation, Executor executor, Consumer<Location> result) {
        if (!supported(provider) || executor == null || result == null) throw new IllegalArgumentException("provider, executor and consumer are required");
        if (cancellation != null && cancellation.isCanceled()) return;
        Subscription s = new Subscription(new Object(), provider, 0, 0, executor, null, result, SystemClock.uptimeMillis() + 30000);
        if (cancellation != null) cancellation.setOnCancelListener(() -> { synchronized (Geo.class) { s.active = false; } remove(s.owner); });
        add(s);
        if (cancellation != null && cancellation.isCanceled()) remove(s.owner);
    }
    public static void single(String provider, LocationListener listener, Executor executor) {
        if (!supported(provider) || listener == null || executor == null) throw new IllegalArgumentException("provider, listener and executor are required");
        add(new Subscription(listener, provider, 0, 0, executor, null, l -> { if (l != null) listener.onLocationChanged(l); }, SystemClock.uptimeMillis()+30000));
    }
    private static synchronized void add(Subscription s) {
        android.util.Log.d("HuskLocation", "request provider=" + s.provider + " interval=" + s.interval + " ms process=" + s.process.name);
        // Android allows one listener to request several providers; replace only that listener's same-provider request.
        for (Iterator<Subscription> it = sSubscriptions.iterator(); it.hasNext();) {
            Subscription old = it.next();
            if (old.owner == s.owner && old.provider.equals(s.provider)) { old.active = false; it.remove(); }
        }
        boolean start = sSubscriptions.isEmpty(); sSubscriptions.add(s);
        if (sPoll == null) sPoll = new Handler(Looper.getMainLooper());
        if (start) { sStatus = -1; sSequence = 0; }
        reconcile();
        sPoll.removeCallbacks(POLL); sPoll.post(POLL);
    }
    private static final Runnable POLL = Geo::poll;
    public static synchronized void remove(Object owner) {
        for (Iterator<Subscription> it = sSubscriptions.iterator(); it.hasNext();) { Subscription s = it.next(); if (s.owner == owner) { s.active = false; it.remove(); } }
        for (Iterator<Subscription> it = sPending.iterator(); it.hasNext();) { Subscription s = it.next(); if (s.owner == owner) { s.active = false; it.remove(); } }
        reconcile();
        if (sSubscriptions.isEmpty() && sPoll != null) sPoll.removeCallbacks(POLL);
    }
    private static void reconcile() {
        boolean wanted = false;
        for (Subscription s : sSubscriptions) if (s.active && !"passive".equals(s.provider)) wanted = true;
        if (wanted != sRunning) { sRunning = wanted; GeoNative.enable(wanted); }
    }
    public static synchronized Location last(String provider) {
        if (!supported(provider)) throw new IllegalArgumentException("Unknown location provider " + provider);
        read();
        if (GeoNative.status() != GeoNative.AUTHORIZED || sLast == null) return null;
        Location location = new Location(sLast); location.setProvider(provider); return location;
    }
    private static void read() {
        double[] v = new double[12]; long sequence = GeoNative.read(v);
        if (sequence == 0 || sequence == sSequence) return;
        sSequence = sequence;
        Location l = new Location("fused"); l.setLatitude(v[0]); l.setLongitude(v[1]); l.setTime((long)v[7]); l.setElapsedRealtimeNanos((long)v[8]);
        l.setAccuracy((float)v[3]);
        if (v[4] >= 0) { l.setAltitude(v[2]); l.setVerticalAccuracyMeters((float)v[4]); }
        if (v[5] >= 0) l.setSpeed((float)v[5]);
        if (v[6] >= 0) l.setBearing((float)v[6]);
        if (v[9] >= 0) l.setSpeedAccuracyMetersPerSecond((float)v[9]);
        if (v[10] >= 0) l.setBearingAccuracyDegrees((float)v[10]);
        l.setMock(v[11] != 0);
        sLast = l;
    }
    private static synchronized void poll() {
        if (sSubscriptions.isEmpty()) return;
        int status = GeoNative.status(); read();
        boolean enabled = status == GeoNative.AUTHORIZED || status == GeoNative.PENDING;
        if (status != sStatus && AppRunner.application() != null) {
            android.content.Intent changed = new android.content.Intent(android.location.LocationManager.PROVIDERS_CHANGED_ACTION);
            changed.putExtra(android.location.LocationManager.EXTRA_PROVIDER_NAME, "gps");
            changed.putExtra(android.location.LocationManager.EXTRA_PROVIDER_ENABLED, enabled);
            Broadcasts.send(AppRunner.application(), changed, true);
        }
        long now = SystemClock.uptimeMillis();
        for (Subscription s : new ArrayList<>(sSubscriptions)) {
            if (!s.active) continue;
            if (s.listener != null && status != sStatus) s.callback(() -> { if (enabled) s.listener.onProviderEnabled(s.provider); else s.listener.onProviderDisabled(s.provider); });
            Location location = status == GeoNative.AUTHORIZED && sLast != null ? new Location(sLast) : null;
            if (location != null) {
                location.setProvider(s.provider);
                if (s.delivered != null && (location.getElapsedRealtimeNanos() <= s.delivered.getElapsedRealtimeNanos()
                    || location.getElapsedRealtimeNanos() - s.delivered.getElapsedRealtimeNanos() < s.interval * 1000000L
                    || location.distanceTo(s.delivered) < s.distance)) continue;
                s.delivered = new Location(location);
                if (s.result != null) {
                    complete(s, location);
                } else s.callback(() -> s.listener.onLocationChanged(location));
            } else if (s.result != null && (!enabled || now >= s.deadline)) {
                complete(s, null);
            }
        }
        sStatus = status;
        reconcile(); if (!sSubscriptions.isEmpty()) sPoll.postDelayed(POLL, 500);
    }
    private static void complete(Subscription s, Location result) {
        sSubscriptions.remove(s); sPending.add(s);
        s.callback(() -> { try { s.result.accept(result); } finally { synchronized (Geo.class) { s.active = false; sPending.remove(s); } } });
    }
    public static synchronized void reset() {
        for (Subscription s : sSubscriptions) s.active = false;
        for (Subscription s : sPending) s.active = false;
        sPending.clear();
        sSubscriptions.clear(); if (sPoll != null) sPoll.removeCallbacks(POLL);
        sPoll = null; sSequence = 0; sStatus = -1; sLast = null; sRunning = false; GeoNative.reset();
    }
}
