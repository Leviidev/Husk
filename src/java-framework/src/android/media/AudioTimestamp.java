package android.media;
public final class AudioTimestamp { public static final int TIMEBASE_MONOTONIC = 0, TIMEBASE_BOOTTIME = 1; public long framePosition, nanoTime; public AudioTimestamp() {} }
