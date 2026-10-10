package android.view.animation;
public class DecelerateInterpolator extends BaseInterpolator {
    private final float f;
    public DecelerateInterpolator() { this(1); }
    public DecelerateInterpolator(float factor) { f = factor; }
    public DecelerateInterpolator(android.content.Context c, android.util.AttributeSet a) { this(1); }
    public float getInterpolation(float t) { return f == 1 ? 1 - (1 - t) * (1 - t) : (float) (1 - Math.pow(1 - t, 2 * f)); }
}
