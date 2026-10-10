package android.graphics;
public class LinearGradient extends Shader {
    public LinearGradient(float x0, float y0, float x1, float y1, int[] colors, float[] positions, TileMode tile) { mNative = husk.Gfx.shLinear(x0, y0, x1, y1, colors, positions, tile(tile)); }
    public LinearGradient(float x0, float y0, float x1, float y1, int c0, int c1, TileMode tile) { this(x0, y0, x1, y1, new int[] { c0, c1 }, null, tile); }
    public LinearGradient(float x0, float y0, float x1, float y1, long[] colors, float[] positions, TileMode tile) { this(x0, y0, x1, y1, Color.huskInts(colors), positions, tile); }
    public LinearGradient(float x0, float y0, float x1, float y1, long c0, long c1, TileMode tile) { this(x0, y0, x1, y1, Color.toArgb(c0), Color.toArgb(c1), tile); }
}
