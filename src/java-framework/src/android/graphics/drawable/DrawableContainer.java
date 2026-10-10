package android.graphics.drawable;

import android.graphics.*;

public class DrawableContainer extends Drawable implements Drawable.Callback {
    final java.util.ArrayList<Drawable> mChildren = new java.util.ArrayList<>();
    int mCurIndex = -1;
    private int mAlpha = 255;
    private ColorFilter mFilter;
    public int addChild(Drawable d) { mChildren.add(d); if (d != null) { d.setCallback(this); d.setBounds(getBounds()); } return mChildren.size() - 1; }
    public boolean selectDrawable(int i) {
        if (i == mCurIndex) return false;
        mCurIndex = i < mChildren.size() ? i : -1;
        Drawable d = getCurrent();
        if (d != null) { d.setBounds(getBounds()); d.setState(getState()); d.setLevel(getLevel()); d.setAlpha(mAlpha); if (mFilter != null) d.setColorFilter(mFilter); }
        invalidateSelf();
        return true;
    }
    @Override public Drawable getCurrent() { return mCurIndex >= 0 && mCurIndex < mChildren.size() ? mChildren.get(mCurIndex) : null; }
    @Override public void draw(Canvas c) { Drawable d = getCurrent(); if (d != null) d.draw(c); }
    @Override protected void onBoundsChange(Rect b) { Drawable d = getCurrent(); if (d != null) d.setBounds(b); }
    @Override public int getIntrinsicWidth() { Drawable d = getCurrent(); return d == null ? -1 : d.getIntrinsicWidth(); }
    @Override public int getIntrinsicHeight() { Drawable d = getCurrent(); return d == null ? -1 : d.getIntrinsicHeight(); }
    @Override public boolean getPadding(Rect p) { Drawable d = getCurrent(); return d != null ? d.getPadding(p) : super.getPadding(p); }
    @Override public void setAlpha(int a) { mAlpha = a; Drawable d = getCurrent(); if (d != null) d.setAlpha(a); }
    @Override public int getAlpha() { return mAlpha; }
    @Override public void setColorFilter(ColorFilter f) { mFilter = f; for (Drawable d : mChildren) if (d != null) d.setColorFilter(f); }
    @Override public void setTintList(android.content.res.ColorStateList t) { for (Drawable d : mChildren) if (d != null) d.setTintList(t); }
    @Override protected boolean onLevelChange(int l) { Drawable d = getCurrent(); return d != null && d.setLevel(l); }
    @Override protected boolean onStateChange(int[] s) { Drawable d = getCurrent(); return d != null && d.setState(s); }
    @Override public boolean isStateful() { return true; }
    public void invalidateDrawable(Drawable who) { if (who == getCurrent()) invalidateSelf(); }
    public void scheduleDrawable(Drawable who, Runnable what, long when) { scheduleSelf(what, when); }
    public void unscheduleDrawable(Drawable who, Runnable what) { unscheduleSelf(what); }
}
