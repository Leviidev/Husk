package android.os;

public final class SystemClock {
    private SystemClock() {}
    public static long uptimeMillis() { return husk.Native.uptimeNanos() / 1000000L; }
    public static long elapsedRealtime() { return husk.Native.uptimeNanos() / 1000000L; }
    public static long elapsedRealtimeNanos() { return husk.Native.uptimeNanos(); }
    public static long currentThreadTimeMillis() { return uptimeMillis(); }
    public static void sleep(long ms) { try { Thread.sleep(ms); } catch (InterruptedException e) { } }
    public static boolean setCurrentTimeMillis(long ms) { return false; }
}
