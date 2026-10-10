package android.animation;
public class ArgbEvaluator implements TypeEvaluator {
    private static final ArgbEvaluator sInstance = new ArgbEvaluator();
    public static ArgbEvaluator getInstance() { return sInstance; }
    public Object evaluate(float f, Object a, Object b) {
        int s = (Integer) a, e = (Integer) b;
        float sa = ((s >> 24) & 255) / 255f, sr = lin(((s >> 16) & 255) / 255f), sg = lin(((s >> 8) & 255) / 255f), sb = lin((s & 255) / 255f);
        float ea = ((e >> 24) & 255) / 255f, er = lin(((e >> 16) & 255) / 255f), eg = lin(((e >> 8) & 255) / 255f), eb = lin((e & 255) / 255f);
        float al = sa + f * (ea - sa), r = srgb(sr + f * (er - sr)), g = srgb(sg + f * (eg - sg)), bl = srgb(sb + f * (eb - sb));
        return Math.round(al * 255) << 24 | Math.round(r * 255) << 16 | Math.round(g * 255) << 8 | Math.round(bl * 255);
    }
    private static float lin(float c) { return c <= 0.04045f ? c / 12.92f : (float) Math.pow((c + 0.055f) / 1.055f, 2.4); }
    private static float srgb(float c) { return c <= 0.0031308f ? c * 12.92f : (float) (1.055 * Math.pow(c, 1 / 2.4) - 0.055); }
}
