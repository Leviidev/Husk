package android.graphics.drawable;

import android.graphics.*;
import android.graphics.drawable.shapes.Shape;

public class ShapeDrawable extends Drawable {
    private Shape mShape; private final Paint mPaint = new Paint(Paint.ANTI_ALIAS_FLAG); private int mW = -1, mH = -1; private Rect mPadding;
    public ShapeDrawable() {}
    public ShapeDrawable(Shape s) { mShape = s; }
    public Shape getShape() { return mShape; }
    public void setShape(Shape s) { mShape = s; onBoundsChange(getBounds()); invalidateSelf(); }
    public Paint getPaint() { return mPaint; }
    public void setIntrinsicWidth(int w) { mW = w; } public void setIntrinsicHeight(int h) { mH = h; }
    public void setPadding(int l, int t, int r, int b) { mPadding = new Rect(l, t, r, b); }
    public void setPadding(Rect p) { mPadding = p; }
    @Override public int getIntrinsicWidth() { return mW; }
    @Override public int getIntrinsicHeight() { return mH; }
    @Override public boolean getPadding(Rect p) { if (mPadding != null) { p.set(mPadding); return true; } return super.getPadding(p); }
    @Override protected void onBoundsChange(Rect b) { if (mShape != null) mShape.resize(b.width(), b.height()); }
    @Override public void setAlpha(int a) { mPaint.setAlpha(a); invalidateSelf(); }
    @Override public int getAlpha() { return mPaint.getAlpha(); }
    @Override public void draw(Canvas c) {
        Rect b = getBounds();
        mPaint.setColorFilter(huskTintFilter());
        c.save(); c.translate(b.left, b.top);
        if (mShape != null) mShape.draw(c, mPaint); else c.drawRect(0, 0, b.width(), b.height(), mPaint);
        c.restore();
    }
}
