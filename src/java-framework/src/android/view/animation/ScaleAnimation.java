package android.view.animation;
public class ScaleAnimation extends Animation {
    private final float fx, tx, fy, ty; private int pxT = ABSOLUTE, pyT = ABSOLUTE; private float pxV, pyV, px, py;
    public ScaleAnimation(float fromX, float toX, float fromY, float toY) { fx = fromX; tx = toX; fy = fromY; ty = toY; }
    public ScaleAnimation(float fromX, float toX, float fromY, float toY, float pivotX, float pivotY) { this(fromX, toX, fromY, toY); pxV = pivotX; pyV = pivotY; }
    public ScaleAnimation(float fromX, float toX, float fromY, float toY, int pxType, float pxValue, int pyType, float pyValue) { this(fromX, toX, fromY, toY); pxT = pxType; pxV = pxValue; pyT = pyType; pyV = pyValue; }
    public ScaleAnimation(android.content.Context c, android.util.AttributeSet a) { this(1, 1, 1, 1); }
    @Override public void initialize(int w, int h, int pw, int ph) { super.initialize(w, h, pw, ph); px = resolveSize(pxT, pxV, w, pw); py = resolveSize(pyT, pyV, h, ph); }
    @Override protected void applyTransformation(float t, Transformation out) { out.getMatrix().setScale(fx + (tx - fx) * t, fy + (ty - fy) * t, px, py); }
}
