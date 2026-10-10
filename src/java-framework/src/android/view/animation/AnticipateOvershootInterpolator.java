package android.view.animation;
public class AnticipateOvershootInterpolator extends BaseInterpolator {
    private final float k;
    public AnticipateOvershootInterpolator() { this(2); }
    public AnticipateOvershootInterpolator(float tension) { k = tension * 1.5f; }
    public AnticipateOvershootInterpolator(float tension, float extra) { k = tension * extra; }
    public AnticipateOvershootInterpolator(android.content.Context c, android.util.AttributeSet a) { this(2); }
    private static float a(float t, float s) { return t * t * ((s + 1) * t - s); }
    private static float o(float t, float s) { return t * t * ((s + 1) * t + s); }
    public float getInterpolation(float t) { return t < 0.5f ? 0.5f * a(t * 2.0f, k) : 0.5f * (o(t * 2.0f - 2.0f, k) + 2.0f); }
}
