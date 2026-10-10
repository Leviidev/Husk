package android.view.animation;
public class RotateAnimation extends Animation {
    private final float a, b; private int pxT = ABSOLUTE, pyT = ABSOLUTE; private float pxV, pyV, px, py;
    public RotateAnimation(float from, float to) { a = from; b = to; }
    public RotateAnimation(float from, float to, float pivotX, float pivotY) { this(from, to); pxV = pivotX; pyV = pivotY; }
    public RotateAnimation(float from, float to, int pxType, float pxValue, int pyType, float pyValue) { this(from, to); pxT = pxType; pxV = pxValue; pyT = pyType; pyV = pyValue; }
    public RotateAnimation(android.content.Context c, android.util.AttributeSet at) { this(0, 0); }
    @Override public void initialize(int w, int h, int pw, int ph) { super.initialize(w, h, pw, ph); px = resolveSize(pxT, pxV, w, pw); py = resolveSize(pyT, pyV, h, ph); }
    @Override protected void applyTransformation(float t, Transformation out) { out.getMatrix().setRotate(a + (b - a) * t, px, py); }
}
