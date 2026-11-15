package android.graphics.drawable;

import android.graphics.*;

public class ClipDrawable extends DrawableWrapper {
    public static final int HORIZONTAL = 1, VERTICAL = 2;
    private final int mGravity, mOrientation;
    public ClipDrawable(Drawable d, int gravity, int orientation) { super(d); mGravity = gravity; mOrientation = orientation; }
    @Override protected boolean onLevelChange(int l) { super.onLevelChange(l); invalidateSelf(); return true; }
    @Override public void draw(Canvas c) {
        Drawable d = getDrawable();
        if (d == null || getLevel() == 0) return;
        Rect b = getBounds(), r = new Rect();
        int w = b.width(), h = b.height();
        if ((mOrientation & HORIZONTAL) != 0) w = w * getLevel() / 10000;
        if ((mOrientation & VERTICAL) != 0) h = h * getLevel() / 10000;
        android.view.Gravity.apply(mGravity, w, h, b, r);
        c.save(); c.clipRect(r); d.draw(c); c.restore();
    }
    @Override public ConstantState getConstantState() {
        if (!huskCanCloneWrappedDrawable()) return null;
        final ClipDrawable src = this;
        return new ConstantState() {
            @Override public Drawable newDrawable() { return newDrawable(null, null); }
            @Override public Drawable newDrawable(android.content.res.Resources r) { return newDrawable(r, null); }
            @Override public Drawable newDrawable(android.content.res.Resources r, android.content.res.Resources.Theme t) {
                ClipDrawable d = new ClipDrawable(null, src.mGravity, src.mOrientation);
                d.huskCopyWrapper(src, r, t);
                return d;
            }
            @Override public int getChangingConfigurations() { return src.getChangingConfigurations(); }
        };
    }
}
