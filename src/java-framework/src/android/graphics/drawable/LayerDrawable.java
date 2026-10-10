package android.graphics.drawable;

import android.graphics.*;

public class LayerDrawable extends Drawable implements Drawable.Callback {
    public static final int PADDING_MODE_NEST = 0, PADDING_MODE_STACK = 1, INSET_UNDEFINED = Integer.MIN_VALUE;
    static final class Layer { Drawable d; int id = -1, l, t, r, b, w = -1, h = -1, gravity; }
    final java.util.ArrayList<Layer> mLayers = new java.util.ArrayList<>();
    private int mPaddingMode = PADDING_MODE_NEST, mAlpha = 255;
    private final Rect mTmp = new Rect();

    public LayerDrawable(Drawable[] layers) { for (Drawable d : layers) addLayer(d); }
    LayerDrawable() {}
    public int addLayer(Drawable d) { Layer l = new Layer(); l.d = d; if (d != null) d.setCallback(this); mLayers.add(l); layout(getBounds()); return mLayers.size() - 1; }
    public int getNumberOfLayers() { return mLayers.size(); }
    public Drawable getDrawable(int i) { return i >= 0 && i < mLayers.size() ? mLayers.get(i).d : null; }
    public void setDrawable(int i, Drawable d) { Layer l = mLayers.get(i); l.d = d; if (d != null) d.setCallback(this); layout(getBounds()); invalidateSelf(); }
    public int getId(int i) { return mLayers.get(i).id; }
    public void setId(int i, int id) { mLayers.get(i).id = id; }
    public int findIndexByLayerId(int id) { for (int i = 0; i < mLayers.size(); i++) if (mLayers.get(i).id == id) return i; return -1; }
    public Drawable findDrawableByLayerId(int id) { int i = findIndexByLayerId(id); return i < 0 ? null : mLayers.get(i).d; }
    public boolean setDrawableByLayerId(int id, Drawable d) { int i = findIndexByLayerId(id); if (i < 0) return false; setDrawable(i, d); return true; }
    public void setLayerInset(int i, int l, int t, int r, int b) { Layer y = mLayers.get(i); y.l = l; y.t = t; y.r = r; y.b = b; layout(getBounds()); }
    public void setLayerInsetLeft(int i, int v) { mLayers.get(i).l = v; layout(getBounds()); }
    public void setLayerInsetTop(int i, int v) { mLayers.get(i).t = v; layout(getBounds()); }
    public void setLayerInsetRight(int i, int v) { mLayers.get(i).r = v; layout(getBounds()); }
    public void setLayerInsetBottom(int i, int v) { mLayers.get(i).b = v; layout(getBounds()); }
    public void setLayerInsetStart(int i, int v) { setLayerInsetLeft(i, v); }
    public void setLayerInsetEnd(int i, int v) { setLayerInsetRight(i, v); }
    public void setLayerSize(int i, int w, int h) { Layer y = mLayers.get(i); y.w = w; y.h = h; layout(getBounds()); }
    public void setLayerWidth(int i, int w) { mLayers.get(i).w = w; layout(getBounds()); }
    public void setLayerHeight(int i, int h) { mLayers.get(i).h = h; layout(getBounds()); }
    public void setLayerGravity(int i, int g) { mLayers.get(i).gravity = g; layout(getBounds()); }
    public int getLayerGravity(int i) { return mLayers.get(i).gravity; }
    public void setPaddingMode(int m) { mPaddingMode = m; }
    public int getPaddingMode() { return mPaddingMode; }
    public void setPadding(int l, int t, int r, int b) {}

    /* Android only lays the layers out from onBoundsChange; the setters must not call it, because apps override it
       and read layers the constructor has not added yet (Shazam's home background) */
    @Override protected void onBoundsChange(Rect b) { layout(b); }
    private void layout(Rect b) {
        for (Layer y : mLayers) {
            if (y.d == null) continue;
            mTmp.set(b.left + y.l, b.top + y.t, b.right - y.r, b.bottom - y.b);
            if (y.w > 0 || y.h > 0 || y.gravity != 0) {
                int w = y.w > 0 ? y.w : y.d.getIntrinsicWidth(), h = y.h > 0 ? y.h : y.d.getIntrinsicHeight();
                int g = y.gravity != 0 ? y.gravity : android.view.Gravity.CENTER;
                if (w <= 0) { w = mTmp.width(); g |= android.view.Gravity.FILL_HORIZONTAL; }
                if (h <= 0) { h = mTmp.height(); g |= android.view.Gravity.FILL_VERTICAL; }
                Rect out = new Rect();
                android.view.Gravity.apply(g, w, h, mTmp, out);
                y.d.setBounds(out);
            } else y.d.setBounds(mTmp);
        }
    }
    @Override public void draw(Canvas c) { for (Layer y : mLayers) if (y.d != null && y.d.isVisible()) y.d.draw(c); }
    @Override public int getIntrinsicWidth() { int w = -1; for (Layer y : mLayers) if (y.d != null) w = Math.max(w, (y.w > 0 ? y.w : y.d.getIntrinsicWidth()) + y.l + y.r); return w; }
    @Override public int getIntrinsicHeight() { int h = -1; for (Layer y : mLayers) if (y.d != null) h = Math.max(h, (y.h > 0 ? y.h : y.d.getIntrinsicHeight()) + y.t + y.b); return h; }
    @Override public boolean getPadding(Rect p) {
        p.set(0, 0, 0, 0);
        Rect q = new Rect();
        for (Layer y : mLayers) if (y.d != null && y.d.getPadding(q)) { p.left = Math.max(p.left, q.left); p.top = Math.max(p.top, q.top); p.right = Math.max(p.right, q.right); p.bottom = Math.max(p.bottom, q.bottom); }
        return (p.left | p.top | p.right | p.bottom) != 0;
    }
    @Override public boolean isStateful() { for (Layer y : mLayers) if (y.d != null && y.d.isStateful()) return true; return false; }
    @Override protected boolean onStateChange(int[] s) { boolean ch = false; for (Layer y : mLayers) if (y.d != null) ch |= y.d.setState(s); return ch; }
    @Override protected boolean onLevelChange(int lv) { boolean ch = false; for (Layer y : mLayers) if (y.d != null) ch |= y.d.setLevel(lv); return ch; }
    @Override public boolean setVisible(boolean v, boolean restart) { for (Layer y : mLayers) if (y.d != null) y.d.setVisible(v, restart); return super.setVisible(v, restart); }
    @Override public void setAlpha(int a) { mAlpha = a; for (Layer y : mLayers) if (y.d != null) y.d.setAlpha(a); }
    @Override public int getAlpha() { return mAlpha; }
    @Override public void setColorFilter(ColorFilter f) { for (Layer y : mLayers) if (y.d != null) y.d.setColorFilter(f); }
    @Override public void setTintList(android.content.res.ColorStateList t) { for (Layer y : mLayers) if (y.d != null) y.d.setTintList(t); }
    @Override public void setHotspot(float x, float y) { for (Layer l : mLayers) if (l.d != null) l.d.setHotspot(x, y); }
    @Override public void jumpToCurrentState() { for (Layer y : mLayers) if (y.d != null) y.d.jumpToCurrentState(); }
    @Override public Drawable mutate() { for (Layer y : mLayers) if (y.d != null) y.d.mutate(); return this; }
    public void invalidateDrawable(Drawable who) { invalidateSelf(); }
    public void scheduleDrawable(Drawable who, Runnable what, long when) { scheduleSelf(what, when); }
    public void unscheduleDrawable(Drawable who, Runnable what) { unscheduleSelf(what); }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public void clearMutated() {}
    public int getBottomPadding() { return 0; }
    public int getEndPadding() { return 0; }
    public int getLayerHeight(int p0) { return 0; }
    public int getLayerInsetBottom(int p0) { return 0; }
    public int getLayerInsetEnd(int p0) { return 0; }
    public int getLayerInsetLeft(int p0) { return 0; }
    public int getLayerInsetRight(int p0) { return 0; }
    public int getLayerInsetStart(int p0) { return 0; }
    public int getLayerInsetTop(int p0) { return 0; }
    public int getLayerWidth(int p0) { return 0; }
    public int getLeftPadding() { return 0; }
    public int getRightPadding() { return 0; }
    public int getStartPadding() { return 0; }
    public int getTopPadding() { return 0; }
    public boolean isProjected() { return false; }
    public void setLayerInsetRelative(int p0, int p1, int p2, int p3, int p4) {}
    public void setOpacity(int p0) {}
    public void setPaddingRelative(int p0, int p1, int p2, int p3) {}
    // ---- end of generated members
}
