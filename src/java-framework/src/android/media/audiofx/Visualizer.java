// Visualizer: valid effect with Android's capture limits; no output is captured, so data reads as silence.
package android.media.audiofx;

@SuppressWarnings({"unchecked", "rawtypes", "deprecation"})
public class Visualizer {
    private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
    public static final int ALREADY_EXISTS = -2;
    public static final int ERROR = -1;
    public static final int ERROR_BAD_VALUE = -4;
    public static final int ERROR_DEAD_OBJECT = -7;
    public static final int ERROR_INVALID_OPERATION = -5;
    public static final int ERROR_NO_INIT = -3;
    public static final int ERROR_NO_MEMORY = -6;
    public static final int MEASUREMENT_MODE_NONE = 0;
    public static final int MEASUREMENT_MODE_PEAK_RMS = 1;
    public static final int SCALING_MODE_AS_PLAYED = 1;
    public static final int SCALING_MODE_NORMALIZED = 0;
    public static final int STATE_ENABLED = 2;
    public static final int STATE_INITIALIZED = 1;
    public static final int STATE_UNINITIALIZED = 0;
    public static final int SUCCESS = 0;
    /** No audio output is captured: the visualizer is a valid, enabled-able effect that reports silence. */
    public Visualizer(int p0) { huskProps.put("CaptureSize", 1024); huskProps.put("SamplingRate", 44100000); huskProps.put("ScalingMode", SCALING_MODE_NORMALIZED); }
    public static int[] getCaptureSizeRange() { return new int[] { 128, 1024 }; }
    public static int getMaxCaptureRate() { return 20000; }
    protected void finalize() {}
    public int getCaptureSize() { return (huskProps.get("CaptureSize") instanceof Integer ? (Integer) huskProps.get("CaptureSize") : 0); }
    public boolean getEnabled() { return (huskProps.get("Enabled") instanceof Boolean ? (Boolean) huskProps.get("Enabled") : false); }
    public int getFft(byte[] p0) { java.util.Arrays.fill(p0, (byte) 0); return SUCCESS; }
    public int getMeasurementMode() { return (huskProps.get("MeasurementMode") instanceof Integer ? (Integer) huskProps.get("MeasurementMode") : 0); }
    public int getMeasurementPeakRms(android.media.audiofx.Visualizer.MeasurementPeakRms p0) { p0.mPeak = -9600; p0.mRms = -9600; return SUCCESS; }
    public int getSamplingRate() { return (huskProps.get("SamplingRate") instanceof Integer ? (Integer) huskProps.get("SamplingRate") : 0); }
    public int getScalingMode() { return (huskProps.get("ScalingMode") instanceof Integer ? (Integer) huskProps.get("ScalingMode") : 0); }
    public int getWaveForm(byte[] p0) { java.util.Arrays.fill(p0, (byte) 0x80); return SUCCESS; }
    public void release() {}
    public int setCaptureSize(int p0) { if (p0 < 128 || p0 > 1024 || Integer.bitCount(p0) != 1) return ERROR_BAD_VALUE; huskProps.put("CaptureSize", p0); return SUCCESS; }
    public int setDataCaptureListener(android.media.audiofx.Visualizer.OnDataCaptureListener p0, int p1, boolean p2, boolean p3) { return 0; }
    public int setEnabled(boolean p0) { huskProps.put("Enabled", p0); return SUCCESS; }
    public int setMeasurementMode(int p0) { huskProps.put("MeasurementMode", p0); return SUCCESS; }
    public int setScalingMode(int p0) { huskProps.put("ScalingMode", p0); return SUCCESS; }
    public int setServerDiedListener(android.media.audiofx.Visualizer.OnServerDiedListener p0) { return 0; }
    Visualizer() { this((int) 0); }
    public static final class MeasurementPeakRms {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public int mPeak;
        public int mRms;
        public MeasurementPeakRms() {}
    }
    public interface OnDataCaptureListener {
        void onFftDataCapture(android.media.audiofx.Visualizer p0, byte[] p1, int p2);
        void onWaveFormDataCapture(android.media.audiofx.Visualizer p0, byte[] p1, int p2);
    }
    public interface OnServerDiedListener {
        void onServerDied();
    }
}
