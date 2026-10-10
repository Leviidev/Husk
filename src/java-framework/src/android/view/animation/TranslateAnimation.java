package android.view.animation;
public class TranslateAnimation extends Animation {
    private int fxT = ABSOLUTE, txT = ABSOLUTE, fyT = ABSOLUTE, tyT = ABSOLUTE;
    private float fxV, txV, fyV, tyV, fx, tx, fy, ty;
    public TranslateAnimation(float fromX, float toX, float fromY, float toY) { fxV = fromX; txV = toX; fyV = fromY; tyV = toY; }
    public TranslateAnimation(int fxType, float fxValue, int txType, float txValue, int fyType, float fyValue, int tyType, float tyValue) {
        fxT = fxType; fxV = fxValue; txT = txType; txV = txValue; fyT = fyType; fyV = fyValue; tyT = tyType; tyV = tyValue;
    }
    public TranslateAnimation(android.content.Context c, android.util.AttributeSet a) {}
    @Override public void initialize(int w, int h, int pw, int ph) {
        super.initialize(w, h, pw, ph);
        fx = resolveSize(fxT, fxV, w, pw); tx = resolveSize(txT, txV, w, pw); fy = resolveSize(fyT, fyV, h, ph); ty = resolveSize(tyT, tyV, h, ph);
    }
    @Override protected void applyTransformation(float t, Transformation out) { out.getMatrix().setTranslate(fx + (tx - fx) * t, fy + (ty - fy) * t); }
}
