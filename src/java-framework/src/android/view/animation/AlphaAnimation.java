package android.view.animation;
public class AlphaAnimation extends Animation {
    private final float a, b;
    public AlphaAnimation(float from, float to) { a = from; b = to; }
    public AlphaAnimation(android.content.Context c, android.util.AttributeSet at) { this(1, 1); }
    @Override protected void applyTransformation(float t, Transformation out) { out.setAlpha(a + (b - a) * t); }
    @Override public boolean willChangeTransformationMatrix() { return false; }
    @Override public boolean willChangeBounds() { return false; }
}
