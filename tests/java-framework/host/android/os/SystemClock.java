package android.os;

/** Host JVM clock stub so production AppRunner/ViewRoot touch dispatch runs without JNI. */
public final class SystemClock {
    private static final long START_NS = System.nanoTime();
    private SystemClock() {}
    private static long monotonicNanos() { return Math.max(1L, System.nanoTime() - START_NS); }
    public static long uptimeMillis() { return monotonicNanos() / 1_000_000L; }
    public static long elapsedRealtime() { return uptimeMillis(); }
    public static long elapsedRealtimeNanos() { return monotonicNanos(); }
    public static long currentThreadTimeMillis() { return uptimeMillis(); }
    public static void sleep(long ms) { try { Thread.sleep(ms); } catch (InterruptedException e) { } }
    public static boolean setCurrentTimeMillis(long ms) { return false; }
    public static long uptimeNanos() { return monotonicNanos(); }
    public static long currentThreadTimeMicro() { return monotonicNanos() / 1000L; }
    public static long currentTimeMicro() { return System.currentTimeMillis() * 1000L; }
    public static long currentNetworkTimeMillis() { return System.currentTimeMillis(); }
    public static java.time.Clock currentNetworkTimeClock() { return java.time.Clock.systemUTC(); }
    public static java.time.Clock currentGnssTimeClock() { return java.time.Clock.systemUTC(); }
    public static java.time.Clock elapsedRealtimeClock() { return new BootClock(); }
    public static java.time.Clock uptimeClock() { return new BootClock(); }
    private static final class BootClock extends java.time.Clock {
        @Override public java.time.ZoneId getZone() { return java.time.ZoneOffset.UTC; }
        @Override public java.time.Clock withZone(java.time.ZoneId z) { return this; }
        @Override public long millis() { return elapsedRealtime(); }
        @Override public java.time.Instant instant() {
            long n = elapsedRealtimeNanos();
            return java.time.Instant.ofEpochSecond(n / 1_000_000_000L, n % 1_000_000_000L);
        }
    }
}
