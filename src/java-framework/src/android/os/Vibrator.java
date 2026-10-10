package android.os;

/** The phone's haptics (husk.Native.vibrate): one-shots and the first step of a pattern; amplitude is not controllable. */
public class Vibrator {
    public static final int VIBRATION_EFFECT_SUPPORT_UNKNOWN = 0, VIBRATION_EFFECT_SUPPORT_YES = 1, VIBRATION_EFFECT_SUPPORT_NO = 2;
    public interface OnVibratorStateChangedListener { void onVibratorStateChanged(boolean isVibrating); }
    public Vibrator() {}
    public int getId() { return 0; }
    public boolean hasVibrator() { return true; }
    public boolean hasAmplitudeControl() { return false; }
    public boolean hasFrequencyControl() { return false; }
    public boolean areVibrationFeaturesSupported(VibrationEffect effect) { return false; }
    public int areAllEffectsSupported(int... effectIds) { return VIBRATION_EFFECT_SUPPORT_NO; }
    public int[] areEffectsSupported(int... effectIds) { int[] r = new int[effectIds.length]; java.util.Arrays.fill(r, VIBRATION_EFFECT_SUPPORT_NO); return r; }
    public boolean areAllPrimitivesSupported(int... primitiveIds) { return false; }
    public boolean[] arePrimitivesSupported(int... primitiveIds) { return new boolean[primitiveIds.length]; }
    public int[] getPrimitiveDurations(int... primitiveIds) { return new int[primitiveIds.length]; }
    public float getResonantFrequency() { return Float.NaN; }
    public float getQFactor() { return Float.NaN; }
    public boolean isVibrating() { return false; }
    public void addVibratorStateListener(OnVibratorStateChangedListener l) {}
    public void addVibratorStateListener(java.util.concurrent.Executor e, OnVibratorStateChangedListener l) {}
    public void removeVibratorStateListener(OnVibratorStateChangedListener l) {}
    @Deprecated public void vibrate(long ms) { if (ms > 0) husk.Native.vibrate(ms); }
    @Deprecated public void vibrate(long ms, android.media.AudioAttributes a) { vibrate(ms); }
    @Deprecated public void vibrate(long[] pattern, int repeat) { if (pattern != null) { for (int i = 1; i < pattern.length; i += 2) if (pattern[i] > 0) { husk.Native.vibrate(pattern[i]); break; } } }
    @Deprecated public void vibrate(long[] pattern, int repeat, android.media.AudioAttributes a) { vibrate(pattern, repeat); }
    public void vibrate(VibrationEffect e) { long ms = e != null ? e.huskDuration() : 0; if (ms > 0) husk.Native.vibrate(ms); }
    public void vibrate(VibrationEffect e, android.media.AudioAttributes a) { vibrate(e); }
    public void vibrate(VibrationEffect e, VibrationAttributes a) { vibrate(e); }
    public void cancel() {}
}
