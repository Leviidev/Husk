package android.os;

/** A vibration: how long it buzzes is what Husk keeps (the iPhone's haptics have no amplitude or waveform control here). */
public abstract class VibrationEffect implements Parcelable {
    public static final int DEFAULT_AMPLITUDE = -1, EFFECT_CLICK = 0, EFFECT_DOUBLE_CLICK = 1, EFFECT_TICK = 2, EFFECT_HEAVY_CLICK = 5;
    private static final class Effect extends VibrationEffect { final long ms; Effect(long ms) { this.ms = ms; } long huskDuration() { return ms; } public long getDuration() { return ms; } }
    VibrationEffect() {}
    long huskDuration() { return 0; }
    public long getDuration() { return huskDuration(); }
    public static VibrationEffect createOneShot(long ms, int amplitude) { if (ms <= 0) throw new IllegalArgumentException("Timing must be greater than 0"); return new Effect(ms); }
    public static VibrationEffect createWaveform(long[] timings, int repeat) { return new Effect(firstOn(timings, null)); }
    public static VibrationEffect createWaveform(long[] timings, int[] amplitudes, int repeat) { return new Effect(firstOn(timings, amplitudes)); }
    public static VibrationEffect createPredefined(int id) { return new Effect(id == EFFECT_TICK ? 10 : id == EFFECT_HEAVY_CLICK ? 40 : 20); }
    public static Composition startComposition() { return new Composition(); }
    private static long firstOn(long[] t, int[] a) { if (t == null) return 0; for (int i = 0; i < t.length; i++) { boolean on = a != null ? i < a.length && a[i] != 0 : (i & 1) == 1; if (on && t[i] > 0) return t[i]; } return 0; }
    public int describeContents() { return 0; }
    public void writeToParcel(Parcel p, int f) { p.writeLong(huskDuration()); }
    public static final Creator<VibrationEffect> CREATOR = new Creator<VibrationEffect>() { public VibrationEffect createFromParcel(Parcel p) { return new Effect(p.readLong()); } public VibrationEffect[] newArray(int n) { return new VibrationEffect[n]; } };
    public static final class Composition {
        public static final int PRIMITIVE_CLICK = 1, PRIMITIVE_THUD = 2, PRIMITIVE_SPIN = 3, PRIMITIVE_QUICK_RISE = 4, PRIMITIVE_SLOW_RISE = 5, PRIMITIVE_QUICK_FALL = 6, PRIMITIVE_TICK = 7, PRIMITIVE_LOW_TICK = 8;
        private long ms;
        Composition() {}
        public Composition addPrimitive(int id) { ms += 15; return this; }
        public Composition addPrimitive(int id, float scale) { ms += 15; return this; }
        public Composition addPrimitive(int id, float scale, int delay) { ms += 15; return this; }
        public VibrationEffect compose() { return new Effect(Math.max(10, ms)); }
    }
}
