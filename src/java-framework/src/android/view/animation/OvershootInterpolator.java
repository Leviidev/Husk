package android.view.animation;
public class OvershootInterpolator extends BaseInterpolator {
    private final float k;
    public OvershootInterpolator() { this(2); }
    public OvershootInterpolator(float tension) { k = tension; }
    public OvershootInterpolator(android.content.Context c, android.util.AttributeSet a) { this(2); }
    public float getInterpolation(float t) { t -= 1.0f; return t * t * ((k + 1) * t + k) + 1.0f; }
}
