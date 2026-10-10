package android.view.animation;
public class AnticipateInterpolator extends BaseInterpolator {
    private final float k;
    public AnticipateInterpolator() { this(2); }
    public AnticipateInterpolator(float tension) { k = tension; }
    public AnticipateInterpolator(android.content.Context c, android.util.AttributeSet a) { this(2); }
    public float getInterpolation(float t) { return t * t * ((k + 1) * t - k); }
}
