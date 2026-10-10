package android.graphics.drawable.shapes;
import android.graphics.*;
public class RoundRectShape extends RectShape {
    private final float[] mOuter, mInnerRadii; private final RectF mInset;
    public RoundRectShape(float[] outer, RectF inset, float[] inner) { mOuter = outer; mInset = inset; mInnerRadii = inner; }
    @Override public void draw(Canvas c, Paint p) {
        Path path = new Path();
        RectF r = rect();
        if (mOuter != null) path.addRoundRect(r, mOuter, Path.Direction.CW); else path.addRect(r, Path.Direction.CW);
        if (mInset != null) {
            RectF in = new RectF(r.left + mInset.left, r.top + mInset.top, r.right - mInset.right, r.bottom - mInset.bottom);
            if (mInnerRadii != null) path.addRoundRect(in, mInnerRadii, Path.Direction.CCW); else path.addRect(in, Path.Direction.CCW);
            path.setFillType(Path.FillType.EVEN_ODD);
        }
        c.drawPath(path, p);
    }
}
