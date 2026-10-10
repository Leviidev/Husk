package android.graphics.drawable;

import android.graphics.*;

public class RotateDrawable extends DrawableWrapper {
    private float mFrom = 0, mTo = 360, mPx = 0.5f, mPy = 0.5f;
    public RotateDrawable() { super(null); }
    public void setFromDegrees(float d) { mFrom = d; } public void setToDegrees(float d) { mTo = d; }
    public void setPivotX(float x) { mPx = x; } public void setPivotY(float y) { mPy = y; }
    public float getFromDegrees() { return mFrom; } public float getToDegrees() { return mTo; }
    @Override protected boolean onLevelChange(int l) { super.onLevelChange(l); invalidateSelf(); return true; }
    @Override public void draw(Canvas c) {
        Drawable d = getDrawable(); if (d == null) return;
        Rect b = getBounds();
        c.save();
        c.rotate(mFrom + (mTo - mFrom) * getLevel() / 10000f, b.left + b.width() * mPx, b.top + b.height() * mPy);
        d.draw(c);
        c.restore();
    }
}
