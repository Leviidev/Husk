package android.graphics.drawable;

import android.graphics.*;

public class DrawableWrapper extends Drawable implements Drawable.Callback {
    private Drawable mDrawable;
    public DrawableWrapper(Drawable d) { setDrawable(d); }
    public void setDrawable(Drawable d) { if (mDrawable != null) mDrawable.setCallback(null); mDrawable = d; if (d != null) { d.setCallback(this); d.setBounds(getBounds()); } invalidateSelf(); }
    public Drawable getDrawable() { return mDrawable; }
    @Override public void draw(Canvas c) { if (mDrawable != null) mDrawable.draw(c); }
    @Override protected void onBoundsChange(Rect b) { if (mDrawable != null) mDrawable.setBounds(b); }
    @Override public int getIntrinsicWidth() { return mDrawable == null ? -1 : mDrawable.getIntrinsicWidth(); }
    @Override public int getIntrinsicHeight() { return mDrawable == null ? -1 : mDrawable.getIntrinsicHeight(); }
    @Override public boolean getPadding(Rect p) { return mDrawable != null ? mDrawable.getPadding(p) : super.getPadding(p); }
    @Override public void setAlpha(int a) { if (mDrawable != null) mDrawable.setAlpha(a); }
    @Override public int getAlpha() { return mDrawable == null ? 255 : mDrawable.getAlpha(); }
    @Override public void setColorFilter(ColorFilter f) { if (mDrawable != null) mDrawable.setColorFilter(f); }
    @Override public void setTintList(android.content.res.ColorStateList t) { if (mDrawable != null) mDrawable.setTintList(t); }
    @Override public boolean isStateful() { return mDrawable != null && mDrawable.isStateful(); }
    @Override protected boolean onStateChange(int[] s) { return mDrawable != null && mDrawable.setState(s); }
    @Override protected boolean onLevelChange(int l) { return mDrawable != null && mDrawable.setLevel(l); }
    public void invalidateDrawable(Drawable who) { invalidateSelf(); }
    public void scheduleDrawable(Drawable who, Runnable what, long when) { scheduleSelf(what, when); }
    public void unscheduleDrawable(Drawable who, Runnable what) { unscheduleSelf(what); }
}
