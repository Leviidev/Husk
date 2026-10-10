package android.graphics;
public class SweepGradient extends Shader {
    public SweepGradient(float cx, float cy, int[] colors, float[] positions) { mNative = husk.Gfx.shSweep(cx, cy, colors, positions); }
    public SweepGradient(float cx, float cy, int c0, int c1) { this(cx, cy, new int[] { c0, c1 }, null); }
    public SweepGradient(float cx, float cy, long[] colors, float[] positions) { this(cx, cy, Color.huskInts(colors), positions); }
}
