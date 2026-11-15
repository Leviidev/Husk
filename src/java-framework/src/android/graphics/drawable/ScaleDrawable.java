package android.graphics.drawable;

import android.graphics.*;

public class ScaleDrawable extends DrawableWrapper {
    private final int mGravity; private final float sw, sh;
    public ScaleDrawable(Drawable d, int gravity, float sw, float sh) { super(d); mGravity = gravity; this.sw = sw; this.sh = sh; }
    @Override protected boolean onLevelChange(int l) { super.onLevelChange(l); onBoundsChange(getBounds()); invalidateSelf(); return true; }
    @Override protected void onBoundsChange(Rect b) {
        Drawable d = getDrawable(); if (d == null) return;
        int lv = getLevel(); int w = b.width(), h = b.height();
        if (sw > 0) w -= (int) (w * (10000 - lv) * sw / 10000);
        if (sh > 0) h -= (int) (h * (10000 - lv) * sh / 10000);
        Rect r = new Rect(); android.view.Gravity.apply(mGravity, w, h, b, r); d.setBounds(r);
    }
    @Override public void draw(Canvas c) { if (getLevel() != 0) super.draw(c); }
    @Override public ConstantState getConstantState() {
        if (!huskCanCloneWrappedDrawable()) return null;
        final ScaleDrawable src = this;
        return new ConstantState() {
            @Override public Drawable newDrawable() { return newDrawable(null, null); }
            @Override public Drawable newDrawable(android.content.res.Resources r) { return newDrawable(r, null); }
            @Override public Drawable newDrawable(android.content.res.Resources r, android.content.res.Resources.Theme t) {
                ScaleDrawable d = new ScaleDrawable(null, src.mGravity, src.sw, src.sh);
                d.huskCopyWrapper(src, r, t);
                return d;
            }
            @Override public int getChangingConfigurations() { return src.getChangingConfigurations(); }
        };
    }
}
