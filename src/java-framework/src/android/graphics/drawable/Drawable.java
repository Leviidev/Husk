package android.graphics.drawable;

import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.*;

public abstract class Drawable {
    public interface Callback {
        void invalidateDrawable(Drawable who);
        void scheduleDrawable(Drawable who, Runnable what, long when);
        void unscheduleDrawable(Drawable who, Runnable what);
    }
    public static abstract class ConstantState {
        public abstract Drawable newDrawable();
        public Drawable newDrawable(Resources res) { return newDrawable(); }
        public Drawable newDrawable(Resources res, Resources.Theme t) { return newDrawable(res); }
        public int getChangingConfigurations() { return 0; }
        public boolean canApplyTheme() { return false; }
    }
    private static final Rect ZERO = new Rect();
    private final Rect mBounds = new Rect();
    private int[] mState = android.util.StateSet.WILD_CARD;
    private int mLevel, mChangingConfigurations, mLayoutDirection;
    private boolean mVisible = true;
    private java.lang.ref.WeakReference<Callback> mCallback;
    protected ColorStateList mTintList;
    protected PorterDuff.Mode mTintMode = PorterDuff.Mode.SRC_IN;
    protected ColorFilter mColorFilter;

    public abstract void draw(Canvas c);
    public void setAlpha(int a) {}
    public int getAlpha() { return 255; }
    public void setColorFilter(ColorFilter f) { mColorFilter = f; }
    public void setColorFilter(int color, PorterDuff.Mode m) { setColorFilter(new PorterDuffColorFilter(color, m)); }
    public ColorFilter getColorFilter() { return mColorFilter; }
    public void clearColorFilter() { setColorFilter(null); }
    public int getOpacity() { return PixelFormat.TRANSLUCENT; }

    public void setBounds(int l, int t, int r, int b) {
        if (mBounds.left != l || mBounds.top != t || mBounds.right != r || mBounds.bottom != b) {
            mBounds.set(l, t, r, b);
            onBoundsChange(mBounds);
        }
    }
    public void setBounds(Rect r) { setBounds(r.left, r.top, r.right, r.bottom); }
    public final void copyBounds(Rect out) { out.set(mBounds); }
    public final Rect copyBounds() { return new Rect(mBounds); }
    public final Rect getBounds() { return mBounds; }
    public Rect getDirtyBounds() { return mBounds; }
    protected void onBoundsChange(Rect b) {}

    public void setChangingConfigurations(int c) { mChangingConfigurations = c; }
    public int getChangingConfigurations() { return mChangingConfigurations; }
    public void setDither(boolean d) {}
    public void setFilterBitmap(boolean f) {}
    public boolean isFilterBitmap() { return false; }
    public final void setCallback(Callback cb) { mCallback = cb == null ? null : new java.lang.ref.WeakReference<>(cb); }
    public Callback getCallback() { return mCallback == null ? null : mCallback.get(); }
    public void invalidateSelf() { Callback c = getCallback(); if (c != null) c.invalidateDrawable(this); }
    public void scheduleSelf(Runnable what, long when) { Callback c = getCallback(); if (c != null) c.scheduleDrawable(this, what, when); }
    public void unscheduleSelf(Runnable what) { Callback c = getCallback(); if (c != null) c.unscheduleDrawable(this, what); }
    public int getLayoutDirection() { return mLayoutDirection; }
    public final boolean setLayoutDirection(int d) { if (mLayoutDirection == d) return false; mLayoutDirection = d; return onLayoutDirectionChanged(d); }
    public boolean onLayoutDirectionChanged(int d) { return false; }
    public void setAutoMirrored(boolean m) {}
    public boolean isAutoMirrored() { return false; }

    public void setTint(int c) { setTintList(ColorStateList.valueOf(c)); }
    public void setTintList(ColorStateList t) { mTintList = t; updateTint(); invalidateSelf(); }
    public void setTintMode(PorterDuff.Mode m) { mTintMode = m == null ? PorterDuff.Mode.SRC_IN : m; updateTint(); invalidateSelf(); }
    public void setTintBlendMode(BlendMode m) {}
    /** The tint as a colour filter for this state, or the colour filter set; subclasses draw with it. */
    protected ColorFilter huskTintFilter() {
        if (mColorFilter != null) return mColorFilter;
        if (mTintList == null) return null;
        return new PorterDuffColorFilter(mTintList.getColorForState(getState(), mTintList.getDefaultColor()), mTintMode);
    }
    void updateTint() {}
    public void setHotspot(float x, float y) {}
    public void setHotspotBounds(int l, int t, int r, int b) {}
    public void getHotspotBounds(Rect out) { out.set(mBounds); }

    public boolean isStateful() { return mTintList != null && mTintList.isStateful(); }
    public boolean hasFocusStateSpecified() { return false; }
    public boolean setState(int[] state) {
        if (!java.util.Arrays.equals(mState, state)) { mState = state; return onStateChange(state); }
        return false;
    }
    public int[] getState() { return mState; }
    protected boolean onStateChange(int[] state) { if (mTintList != null && mTintList.isStateful()) { invalidateSelf(); return true; } return false; }
    public void jumpToCurrentState() {}
    public Drawable getCurrent() { return this; }
    public final boolean setLevel(int level) { if (mLevel != level) { mLevel = level; return onLevelChange(level); } return false; }
    public final int getLevel() { return mLevel; }
    protected boolean onLevelChange(int level) { return false; }
    public boolean setVisible(boolean visible, boolean restart) { boolean changed = mVisible != visible; mVisible = visible; if (changed) invalidateSelf(); return changed; }
    public final boolean isVisible() { return mVisible; }
    public int getIntrinsicWidth() { return -1; }
    public int getIntrinsicHeight() { return -1; }
    public int getMinimumWidth() { int w = getIntrinsicWidth(); return w > 0 ? w : 0; }
    public int getMinimumHeight() { int h = getIntrinsicHeight(); return h > 0 ? h : 0; }
    public boolean getPadding(Rect padding) { padding.set(0, 0, 0, 0); return false; }
    public Insets getOpticalInsets() { return Insets.NONE; }
    public void getOutline(Outline o) { o.setRect(getBounds()); o.setAlpha(0); }
    public Drawable mutate() { return this; }
    public ConstantState getConstantState() { return null; }
    public boolean canApplyTheme() { return false; }
    public void applyTheme(Resources.Theme t) {}
    public Region getTransparentRegion() { return null; }
    public void inflate(Resources r, org.xmlpull.v1.XmlPullParser p, android.util.AttributeSet a) throws org.xmlpull.v1.XmlPullParserException, java.io.IOException {}
    public void inflate(Resources r, org.xmlpull.v1.XmlPullParser p, android.util.AttributeSet a, Resources.Theme t) throws org.xmlpull.v1.XmlPullParserException, java.io.IOException { inflate(r, p, a); }

    public static int resolveOpacity(int a, int b) { if (a == b) return a; if (a == PixelFormat.UNKNOWN || b == PixelFormat.UNKNOWN) return PixelFormat.UNKNOWN; if (a == PixelFormat.TRANSLUCENT || b == PixelFormat.TRANSLUCENT) return PixelFormat.TRANSLUCENT; if (a == PixelFormat.TRANSPARENT || b == PixelFormat.TRANSPARENT) return PixelFormat.TRANSPARENT; return PixelFormat.OPAQUE; }
    public static Drawable createFromStream(java.io.InputStream in, String src) { Bitmap b = BitmapFactory.decodeStream(in); return b == null ? null : new BitmapDrawable(null, b); }
    public static Drawable createFromPath(String path) { Bitmap b = BitmapFactory.decodeFile(path); return b == null ? null : new BitmapDrawable(null, b); }
    public static Drawable createFromResourceStream(Resources res, android.util.TypedValue v, java.io.InputStream in, String src) { return createFromStream(in, src); }
    public static Drawable createFromResourceStream(Resources res, android.util.TypedValue v, java.io.InputStream in, String src, BitmapFactory.Options o) { return createFromStream(in, src); }
    public static Drawable createFromXml(Resources r, org.xmlpull.v1.XmlPullParser p) throws org.xmlpull.v1.XmlPullParserException, java.io.IOException { return createFromXml(r, p, null); }
    public static Drawable createFromXml(Resources r, org.xmlpull.v1.XmlPullParser p, Resources.Theme t) throws org.xmlpull.v1.XmlPullParserException, java.io.IOException { return r.huskInflateDrawable(p, t); }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    protected int mSrcDensityOverride;
    public static android.graphics.drawable.Drawable createFromXmlForDensity(android.content.res.Resources p0, org.xmlpull.v1.XmlPullParser p1, int p2, android.content.res.Resources.Theme p3) { return null; }
    public static android.graphics.drawable.Drawable createFromXmlInner(android.content.res.Resources p0, org.xmlpull.v1.XmlPullParser p1, android.util.AttributeSet p2) { return null; }
    public static android.graphics.drawable.Drawable createFromXmlInner(android.content.res.Resources p0, org.xmlpull.v1.XmlPullParser p1, android.util.AttributeSet p2, android.content.res.Resources.Theme p3) { return null; }
    protected static android.content.res.TypedArray obtainAttributes(android.content.res.Resources p0, android.content.res.Resources.Theme p1, android.util.AttributeSet p2, int[] p3) { return null; }
    public static android.graphics.BlendMode parseBlendMode(int p0, android.graphics.BlendMode p1) { return null; }
    public static android.graphics.PorterDuff.Mode parseTintMode(int p0, android.graphics.PorterDuff.Mode p1) { return null; }
    public void setXfermode(android.graphics.Xfermode p0) {}
    // ---- end of generated members
}
