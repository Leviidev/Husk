package android.widget;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.*;
import android.graphics.drawable.*;
import android.util.AttributeSet;
import android.view.View;

/**
 * Determinate bars draw the app's progressDrawable (a layer list whose progress layer is clipped by level) when it gives one,
 * else a plain Material track; indeterminate ones spin a Material arc (or a horizontal sweep) unless the app supplied a drawable.
 */
public class ProgressBar extends View {
    private static final int MAX_LEVEL = 10000;
    int mMinWidth = 24, mMaxWidth = 48, mMinHeight = 24, mMaxHeight = 48;
    private int mProgress, mSecondaryProgress, mMax = 100, mMin;
    private boolean mIndeterminate, mOnlyIndeterminate, mHorizontal, mAppDrawable, mAppIndeterminate;
    private Drawable mProgressDrawable, mIndeterminateDrawable, mCurrentDrawable;
    private ColorStateList mProgressTint, mProgressBgTint, mIndeterminateTint;
    private final Paint mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private long mStart = android.os.SystemClock.uptimeMillis();
    private boolean mAnimating;
    public ProgressBar(Context c) { this(c, null); }
    public ProgressBar(Context c, AttributeSet a) { this(c, a, android.R.attr.progressBarStyle); }
    public ProgressBar(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public ProgressBar(Context c, AttributeSet attrs, int s, int r) {
        super(c, attrs, s, r);
        float d = c.getResources().getDisplayMetrics().density;
        mMinWidth = mMinHeight = (int) (24 * d); mMaxWidth = mMaxHeight = (int) (48 * d);
        mHorizontal = s == android.R.attr.progressBarStyleHorizontal;
        if (attrs != null) for (int i = 0; i < attrs.getAttributeCount(); i++) if ("style".equals(attrs.getAttributeName(i))) { String v = attrs.getAttributeValue(i); if (v != null && v.toLowerCase().contains("horizontal")) mHorizontal = true; }
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.ProgressBar, s, r);
        Drawable pd = null;
        try { pd = a.getDrawable(husk.S.ProgressBar_progressDrawable); } catch (RuntimeException e) {}
        if (pd != null) { mHorizontal = true; setProgressDrawable(pd); }
        Drawable id = null;
        try { id = a.getDrawable(husk.S.ProgressBar_indeterminateDrawable); } catch (RuntimeException e) {}
        // the platform's own indeterminate drawables are animated vectors Husk draws itself; take the app's
        if (id != null && !(id instanceof Animatable) || id instanceof AnimationDrawable) { mIndeterminateDrawable = id; mAppIndeterminate = true; id.setCallback(this); }
        mMinWidth = a.getDimensionPixelSize(husk.S.ProgressBar_minWidth, mMinWidth);
        mMaxWidth = a.getDimensionPixelSize(husk.S.ProgressBar_maxWidth, mMaxWidth);
        mMinHeight = a.getDimensionPixelSize(husk.S.ProgressBar_minHeight, mMinHeight);
        mMaxHeight = a.getDimensionPixelSize(husk.S.ProgressBar_maxHeight, mMaxHeight);
        if (mHorizontal && !a.hasValue(husk.S.ProgressBar_minHeight)) { mMinHeight = (int) (4 * d); mMaxHeight = (int) (16 * d); }
        mMin = a.getInt(husk.S.ProgressBar_min, 0);
        mMax = a.getInt(husk.S.ProgressBar_max, 100);
        mProgress = a.getInt(husk.S.ProgressBar_progress, 0);
        mSecondaryProgress = a.getInt(husk.S.ProgressBar_secondaryProgress, 0);
        mOnlyIndeterminate = a.getBoolean(husk.S.ProgressBar_indeterminateOnly, false);
        mIndeterminate = mOnlyIndeterminate || a.getBoolean(husk.S.ProgressBar_indeterminate, !mHorizontal);
        mProgressTint = a.getColorStateList(husk.S.ProgressBar_progressTint);
        mProgressBgTint = a.getColorStateList(husk.S.ProgressBar_progressBackgroundTint);
        mIndeterminateTint = a.getColorStateList(husk.S.ProgressBar_indeterminateTint);
        a.recycle();
        if (!mHorizontal) mIndeterminate = true;
        refreshProgress();
    }
    private float dp(float v) { return v * getResources().getDisplayMetrics().density; }
    public synchronized boolean isIndeterminate() { return mIndeterminate; }
    public synchronized void setIndeterminate(boolean i) { if ((!mOnlyIndeterminate || i) && i != mIndeterminate) { mIndeterminate = i; if (i) startAnimation(); else stopAnimation(); invalidate(); } }
    public Drawable getIndeterminateDrawable() { return mIndeterminateDrawable; }
    public void setIndeterminateDrawable(Drawable d) { if (mIndeterminateDrawable != null) mIndeterminateDrawable.setCallback(null); mIndeterminateDrawable = d; mAppIndeterminate = d != null; if (d != null) d.setCallback(this); requestLayout(); invalidate(); }
    public void setIndeterminateDrawableTiled(Drawable d) { setIndeterminateDrawable(d); }
    public void setIndeterminateTintList(ColorStateList t) { mIndeterminateTint = t; invalidate(); }
    public ColorStateList getIndeterminateTintList() { return mIndeterminateTint; }
    public void setIndeterminateTintMode(PorterDuff.Mode m) {}
    public Drawable getProgressDrawable() { return mProgressDrawable; }
    public void setProgressDrawable(Drawable d) {
        if (mProgressDrawable == d) return;
        if (mProgressDrawable != null) mProgressDrawable.setCallback(null);
        mProgressDrawable = d; mAppDrawable = d != null;
        if (d != null) { d.setCallback(this); if (d.isStateful()) d.setState(getDrawableState()); int h = d.getMinimumHeight(); if (mMaxHeight < h) { mMaxHeight = h; requestLayout(); } }
        updateDrawableBounds(getWidth(), getHeight());
        refreshProgress();
    }
    public void setProgressDrawableTiled(Drawable d) { setProgressDrawable(d); }
    public Drawable getCurrentDrawable() { return mIndeterminate ? mIndeterminateDrawable : mProgressDrawable; }
    public void setProgressTintList(ColorStateList t) { mProgressTint = t; if (mProgressDrawable != null) { Drawable l = layer(android.R.id.progress); if (l != null) l.setTintList(t); } invalidate(); }
    public ColorStateList getProgressTintList() { return mProgressTint; }
    public void setProgressTintMode(PorterDuff.Mode m) {}
    public void setProgressBackgroundTintList(ColorStateList t) { mProgressBgTint = t; if (mProgressDrawable != null) { Drawable l = layer(android.R.id.background); if (l != null) l.setTintList(t); } invalidate(); }
    public ColorStateList getProgressBackgroundTintList() { return mProgressBgTint; }
    public void setProgressBackgroundTintMode(PorterDuff.Mode m) {}
    public void setSecondaryProgressTintList(ColorStateList t) {}
    public void setSecondaryProgressTintMode(PorterDuff.Mode m) {}
    public void setInterpolator(Context c, int r) {}
    public void setInterpolator(android.view.animation.Interpolator i) {}
    public android.view.animation.Interpolator getInterpolator() { return null; }
    private Drawable layer(int id) { return mProgressDrawable instanceof LayerDrawable ? ((LayerDrawable) mProgressDrawable).findDrawableByLayerId(id) : null; }
    private void refreshProgress() {
        if (mProgressDrawable != null) {
            int range = mMax - mMin;
            float scale = range > 0 ? (mProgress - mMin) / (float) range : 0, scale2 = range > 0 ? (mSecondaryProgress - mMin) / (float) range : 0;
            if (mProgressDrawable instanceof LayerDrawable) {
                Drawable p = layer(android.R.id.progress), sp = layer(android.R.id.secondaryProgress);
                if (p != null) p.setLevel((int) (scale * MAX_LEVEL)); else mProgressDrawable.setLevel((int) (scale * MAX_LEVEL));
                if (sp != null) sp.setLevel((int) (scale2 * MAX_LEVEL));
            } else mProgressDrawable.setLevel((int) (scale * MAX_LEVEL));
        }
        invalidate();
    }
    void onProgressRefresh(float scale, boolean fromUser, int progress) {}
    public synchronized void setProgress(int p) { setProgressInternal(p, false, false); }
    public void setProgress(int p, boolean animate) { setProgressInternal(p, false, animate); }
    synchronized boolean setProgressInternal(int p, boolean fromUser, boolean animate) {
        if (mIndeterminate) return false;
        p = Math.max(mMin, Math.min(mMax, p));
        if (p == mProgress) return false;
        mProgress = p;
        refreshProgress();
        onProgressRefresh(getScale(), fromUser, p);
        return true;
    }
    float getScale() { int range = mMax - mMin; return range > 0 ? (mProgress - mMin) / (float) range : 0; }
    public synchronized void setSecondaryProgress(int p) { if (mIndeterminate) return; p = Math.max(mMin, Math.min(mMax, p)); if (p != mSecondaryProgress) { mSecondaryProgress = p; refreshProgress(); } }
    public synchronized int getProgress() { return mIndeterminate ? 0 : mProgress; }
    public synchronized int getSecondaryProgress() { return mIndeterminate ? 0 : mSecondaryProgress; }
    public synchronized int getMin() { return mMin; }
    public synchronized int getMax() { return mMax; }
    public synchronized void setMin(int m) { if (m > mMax) m = mMax; if (m != mMin) { mMin = m; if (mProgress < m) mProgress = m; refreshProgress(); } }
    public synchronized void setMax(int m) { if (m < mMin) m = mMin; if (m != mMax) { mMax = m; if (mProgress > m) mProgress = m; refreshProgress(); } }
    public final synchronized void incrementProgressBy(int d) { setProgress(mProgress + d); }
    public final synchronized void incrementSecondaryProgressBy(int d) { setSecondaryProgress(mSecondaryProgress + d); }
    public boolean isAnimating() { return mAnimating; }
    void startAnimation() { if (getVisibility() != VISIBLE || getWindowVisibility() != VISIBLE) return; mAnimating = true; if (mIndeterminateDrawable instanceof Animatable) ((Animatable) mIndeterminateDrawable).start(); postInvalidateOnAnimation(); }
    void stopAnimation() { mAnimating = false; if (mIndeterminateDrawable instanceof Animatable) ((Animatable) mIndeterminateDrawable).stop(); }
    @Override protected void onAttachedToWindow() { super.onAttachedToWindow(); if (mIndeterminate) startAnimation(); }
    @Override protected void onDetachedFromWindow() { stopAnimation(); super.onDetachedFromWindow(); }
    @Override protected void onVisibilityChanged(View v, int vis) { super.onVisibilityChanged(v, vis); if (mIndeterminate) { if (vis == VISIBLE && getVisibility() == VISIBLE) startAnimation(); else stopAnimation(); } }
    @Override protected boolean verifyDrawable(Drawable who) { return who == mProgressDrawable || who == mIndeterminateDrawable || super.verifyDrawable(who); }
    @Override public void invalidateDrawable(Drawable d) { if (verifyDrawable(d)) { Rect b = d.getBounds(); invalidate(b.left, b.top, b.right, b.bottom); } else super.invalidateDrawable(d); }
    @Override protected void drawableStateChanged() { super.drawableStateChanged(); int[] s = getDrawableState(); if (mProgressDrawable != null && mProgressDrawable.isStateful()) mProgressDrawable.setState(s); if (mIndeterminateDrawable != null && mIndeterminateDrawable.isStateful()) mIndeterminateDrawable.setState(s); }
    @Override protected void onSizeChanged(int w, int h, int ow, int oh) { updateDrawableBounds(w, h); }
    private void updateDrawableBounds(int w, int h) {
        w -= getPaddingLeft() + getPaddingRight(); h -= getPaddingTop() + getPaddingBottom();
        if (mProgressDrawable != null) mProgressDrawable.setBounds(0, 0, w, h);
        if (mIndeterminateDrawable != null) {
            int right = w, bottom = h, top = 0, left = 0;
            int iw = mIndeterminateDrawable.getIntrinsicWidth(), ih = mIndeterminateDrawable.getIntrinsicHeight();
            if (iw > 0 && ih > 0 && !(mIndeterminateDrawable instanceof AnimationDrawable && mHorizontal)) {
                float ia = (float) iw / ih, ba = (float) w / h;
                if (ia != ba) { if (ba > ia) { int width = (int) (h * ia); left = (w - width) / 2; right = left + width; } else { int height = (int) (w / ia); top = (h - height) / 2; bottom = top + height; } }
            }
            mIndeterminateDrawable.setBounds(left, top, right, bottom);
        }
    }
    private int accent() {
        android.util.TypedValue v = new android.util.TypedValue();
        if (getContext().getTheme().resolveAttribute(android.R.attr.colorAccent, v, true) && v.type >= android.util.TypedValue.TYPE_FIRST_COLOR_INT && v.type <= android.util.TypedValue.TYPE_LAST_COLOR_INT) return v.data;
        if (getContext().getTheme().resolveAttribute(android.R.attr.colorAccent, v, true) && v.resourceId != 0) { try { return getContext().getColor(v.resourceId); } catch (Exception e) {} }
        return 0xFF6750A4;
    }
    private int tint(ColorStateList t, int def) { return t != null ? t.getColorForState(getDrawableState(), def) : def; }
    @Override protected synchronized void onDraw(Canvas c) {
        super.onDraw(c);
        int save = c.save();
        c.translate(getPaddingLeft(), getPaddingTop());
        int w = getWidth() - getPaddingLeft() - getPaddingRight(), h = getHeight() - getPaddingTop() - getPaddingBottom();
        long t = android.os.SystemClock.uptimeMillis() - mStart;
        if (mIndeterminate) {
            if (mIndeterminateDrawable != null && mAppIndeterminate) {
                if (mIndeterminateDrawable instanceof AnimationDrawable) mIndeterminateDrawable.draw(c);
                else { c.rotate((t % 1333) / 1333f * 360f, w / 2f, h / 2f); mIndeterminateDrawable.draw(c); }
            } else if (mHorizontal) {
                int col = tint(mIndeterminateTint, accent());
                mPaint.setStyle(Paint.Style.FILL);
                mPaint.setColor((col & 0xFFFFFF) | 0x40000000);
                float th = Math.min(h, dp(4)), top = (h - th) / 2;
                c.drawRect(0, top, w, top + th, mPaint);
                mPaint.setColor(col);
                float f = (t % 2000) / 2000f, start = (f * 1.5f - 0.5f) * w, end = start + w * 0.5f;
                c.drawRect(Math.max(0, start), top, Math.min(w, end), top + th, mPaint);
            } else {
                int col = tint(mIndeterminateTint, accent());
                float size = Math.min(w, h), stroke = Math.max(dp(2), size / 12f), r = size / 2 - stroke;
                mPaint.setStyle(Paint.Style.STROKE); mPaint.setStrokeWidth(stroke); mPaint.setStrokeCap(Paint.Cap.ROUND); mPaint.setColor(col);
                float cx = w / 2f, cy = h / 2f;
                float cycle = (t % 1333) / 1333f, sweep = 20 + 250 * (float) (0.5 - 0.5 * Math.cos(cycle * 2 * Math.PI)), rot = (t % 2000) / 2000f * 360f + cycle * 180f;
                c.drawArc(new RectF(cx - r, cy - r, cx + r, cy + r), rot, sweep, false, mPaint);
                mPaint.setStrokeCap(Paint.Cap.BUTT);
            }
            if (mAnimating) postInvalidateOnAnimation();
        } else if (mProgressDrawable != null) {
            mProgressDrawable.draw(c);
        } else {
            int col = tint(mProgressTint, accent());
            int bg = tint(mProgressBgTint, (col & 0xFFFFFF) | 0x40000000);
            float th = Math.min(h, dp(4)), top = (h - th) / 2;
            mPaint.setStyle(Paint.Style.FILL);
            mPaint.setColor(bg); c.drawRect(0, top, w, top + th, mPaint);
            int range = mMax - mMin;
            if (range > 0 && mSecondaryProgress > mMin) { mPaint.setColor((col & 0xFFFFFF) | 0x80000000); c.drawRect(0, top, w * (mSecondaryProgress - mMin) / (float) range, top + th, mPaint); }
            mPaint.setColor(col);
            c.drawRect(0, top, w * getScale(), top + th, mPaint);
        }
        c.restoreToCount(save);
    }
    @Override protected synchronized void onMeasure(int ws, int hs) {
        int dw = 0, dh = 0;
        Drawable d = mIndeterminate ? mIndeterminateDrawable : mProgressDrawable;
        if (d != null) { dw = Math.max(mMinWidth, Math.min(mMaxWidth, d.getIntrinsicWidth())); dh = Math.max(mMinHeight, Math.min(mMaxHeight, d.getIntrinsicHeight())); }
        else if (mHorizontal) { dw = mMinWidth; dh = mMinHeight; }
        else { dw = mMaxWidth; dh = mMaxHeight; }
        dw += getPaddingLeft() + getPaddingRight(); dh += getPaddingTop() + getPaddingBottom();
        setMeasuredDimension(resolveSizeAndState(dw, ws, 0), resolveSizeAndState(dh, hs, 0));
    }
    public int getMinWidth() { return mMinWidth; } public int getMaxWidth() { return mMaxWidth; } public int getMinHeight() { return mMinHeight; } public int getMaxHeight() { return mMaxHeight; }
    public void setMinWidth(int v) { mMinWidth = v; requestLayout(); } public void setMaxWidth(int v) { mMaxWidth = v; requestLayout(); } public void setMinHeight(int v) { mMinHeight = v; requestLayout(); } public void setMaxHeight(int v) { mMaxHeight = v; requestLayout(); }
    boolean isHorizontalHusk() { return mHorizontal; }
    @Override public CharSequence getAccessibilityClassName() { return ProgressBar.class.getName(); }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
    protected void encodeProperties(android.view.ViewHierarchyEncoder p0) {}
    public android.graphics.BlendMode getIndeterminateTintBlendMode() { return (android.graphics.BlendMode) huskFill.get("IndeterminateTintBlendMode"); }
    public android.graphics.PorterDuff.Mode getIndeterminateTintMode() { return null; }
    public boolean getMirrorForRtl() { return false; }
    public android.graphics.BlendMode getProgressBackgroundTintBlendMode() { return (android.graphics.BlendMode) huskFill.get("ProgressBackgroundTintBlendMode"); }
    public android.graphics.PorterDuff.Mode getProgressBackgroundTintMode() { return null; }
    public android.graphics.BlendMode getProgressTintBlendMode() { return (android.graphics.BlendMode) huskFill.get("ProgressTintBlendMode"); }
    public android.graphics.PorterDuff.Mode getProgressTintMode() { return null; }
    public android.graphics.BlendMode getSecondaryProgressTintBlendMode() { return (android.graphics.BlendMode) huskFill.get("SecondaryProgressTintBlendMode"); }
    public android.content.res.ColorStateList getSecondaryProgressTintList() { return null; }
    public android.graphics.PorterDuff.Mode getSecondaryProgressTintMode() { return null; }
    public void onInitializeAccessibilityEventInternal(android.view.accessibility.AccessibilityEvent p0) {}
    public void onInitializeAccessibilityNodeInfoInternal(android.view.accessibility.AccessibilityNodeInfo p0) {}
    public void onResolveDrawables(int p0) {}
    public void setIndeterminateTintBlendMode(android.graphics.BlendMode p0) { huskFill.put("IndeterminateTintBlendMode", p0); }
    public void setProgressBackgroundTintBlendMode(android.graphics.BlendMode p0) { huskFill.put("ProgressBackgroundTintBlendMode", p0); }
    public void setProgressTintBlendMode(android.graphics.BlendMode p0) { huskFill.put("ProgressTintBlendMode", p0); }
    public void setSecondaryProgressTintBlendMode(android.graphics.BlendMode p0) { huskFill.put("SecondaryProgressTintBlendMode", p0); }
    // ---- end of generated members
}
