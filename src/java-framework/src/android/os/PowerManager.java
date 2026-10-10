package android.os;

public class PowerManager {
    public static final int PARTIAL_WAKE_LOCK = 1, SCREEN_DIM_WAKE_LOCK = 6, SCREEN_BRIGHT_WAKE_LOCK = 10, FULL_WAKE_LOCK = 26;
    public final class WakeLock {
        public void acquire() {} public void acquire(long t) {} public void release() {} public boolean isHeld() { return false; }
        public void setReferenceCounted(boolean b) {}
    }
    public WakeLock newWakeLock(int flags, String tag) { return new WakeLock(); }
    public boolean isScreenOn() { return true; }
    public boolean isInteractive() { return true; }
    public boolean isPowerSaveMode() { return false; }
}
