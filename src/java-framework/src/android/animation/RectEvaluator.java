package android.animation;
import android.graphics.Rect;
public class RectEvaluator implements TypeEvaluator<Rect> {
    private Rect mR;
    public RectEvaluator() {}
    public RectEvaluator(Rect reuse) { mR = reuse; }
    public Rect evaluate(float f, Rect a, Rect b) {
        int l = a.left + (int) ((b.left - a.left) * f), t = a.top + (int) ((b.top - a.top) * f), r = a.right + (int) ((b.right - a.right) * f), bo = a.bottom + (int) ((b.bottom - a.bottom) * f);
        if (mR != null) { mR.set(l, t, r, bo); return mR; } return new Rect(l, t, r, bo);
    }
}
