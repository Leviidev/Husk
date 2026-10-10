package android.os;
public class VibratorManager {
    private final Vibrator mV = new Vibrator();
    public int[] getVibratorIds() { return new int[] { 0 }; }
    public Vibrator getVibrator(int id) { return mV; }
    public Vibrator getDefaultVibrator() { return mV; }
    public void vibrate(CombinedVibration v) { mV.vibrate(40); }
    public void cancel() {}
}
