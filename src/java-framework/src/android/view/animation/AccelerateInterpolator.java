package android.view.animation;
public class AccelerateInterpolator extends BaseInterpolator {
    private final float f; private final double d;
    public AccelerateInterpolator() { this(1); }
    public AccelerateInterpolator(float factor) { f = factor; d = 2 * factor; }
    public AccelerateInterpolator(android.content.Context c, android.util.AttributeSet a) { this(1); }
    public float getInterpolation(float t) { return f == 1 ? t * t : (float) Math.pow(t, d); }
}
