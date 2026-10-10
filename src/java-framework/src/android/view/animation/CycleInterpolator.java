package android.view.animation;
public class CycleInterpolator extends BaseInterpolator {
    private final float c;
    public CycleInterpolator(float cycles) { c = cycles; }
    public CycleInterpolator(android.content.Context ctx, android.util.AttributeSet a) { this(1); }
    public float getInterpolation(float t) { return (float) Math.sin(2 * c * Math.PI * t); }
}
