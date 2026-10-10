package android.os;

public final class SystemClock {
    private SystemClock() {}
    public static long uptimeMillis() { return husk.Native.uptimeNanos() / 1000000L; }
    public static long elapsedRealtime() { return husk.Native.uptimeNanos() / 1000000L; }
    public static long elapsedRealtimeNanos() { return husk.Native.uptimeNanos(); }
    public static long currentThreadTimeMillis() { return uptimeMillis(); }
    public static void sleep(long ms) { try { Thread.sleep(ms); } catch (InterruptedException e) { } }
    public static boolean setCurrentTimeMillis(long ms) { return false; }
    public static long uptimeNanos() { return husk.Native.uptimeNanos(); }
    public static long currentThreadTimeMicro() { return husk.Native.uptimeNanos() / 1000L; }
    public static long currentTimeMicro() { return System.currentTimeMillis() * 1000L; }
    public static long currentNetworkTimeMillis() { return System.currentTimeMillis(); }
    public static java.time.Clock currentNetworkTimeClock() { return java.time.Clock.systemUTC(); }
    public static java.time.Clock currentGnssTimeClock() { return java.time.Clock.systemUTC(); }
    /** Clocks whose instant is the time since boot, as Android's are */
    public static java.time.Clock elapsedRealtimeClock() { return new BootClock(); }
    public static java.time.Clock uptimeClock() { return new BootClock(); }
    private static final class BootClock extends java.time.Clock {
        @Override public java.time.ZoneId getZone() { return java.time.ZoneOffset.UTC; }
        @Override public java.time.Clock withZone(java.time.ZoneId z) { return this; }
        @Override public long millis() { return elapsedRealtime(); }
        @Override public java.time.Instant instant() { long n = elapsedRealtimeNanos(); return java.time.Instant.ofEpochSecond(n / 1000000000L, n % 1000000000L); }
    }
}
