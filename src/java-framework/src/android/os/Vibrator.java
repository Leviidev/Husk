package android.os;

public class Vibrator {
    public boolean hasVibrator() { return true; }
    public void vibrate(long ms) { husk.Native.vibrate(ms); }
    public void vibrate(long[] pattern, int repeat) { if (pattern != null && pattern.length > 1) husk.Native.vibrate(pattern[1]); }
    public void vibrate(VibrationEffect e) { husk.Native.vibrate(40); }
    public void cancel() {}
}
