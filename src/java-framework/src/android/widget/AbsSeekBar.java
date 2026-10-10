package android.widget;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.*;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.MotionEvent;

public abstract class AbsSeekBar extends ProgressBar {
    private Drawable mThumb;
    private ColorStateList mThumbTint;
    private int mThumbOffset, mKeyProgressIncrement = 1;
    private boolean mIsDragging;
    float mTouchProgressOffset;
    boolean mIsUserSeekable = true;
    private final Paint mP = new Paint(Paint.ANTI_ALIAS_FLAG);
    public AbsSeekBar(Context c) { this(c, null); }
    public AbsSeekBar(Context c, AttributeSet a) { this(c, a, 0); }
    public AbsSeekBar(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public AbsSeekBar(Context c, AttributeSet attrs, int s, int r) {
        super(c, attrs, s, r);
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.AbsSeekBar, s, r);
        Drawable t = null;
        try { t = a.getDrawable(husk.S.AbsSeekBar_thumb); } catch (RuntimeException e) {}
        if (t != null && !(t instanceof android.graphics.drawable.StateListDrawable && t.getIntrinsicWidth() <= 0)) setThumb(t);
        mThumbTint = a.getColorStateList(husk.S.AbsSeekBar_thumbTint);
        mThumbOffset = a.getDimensionPixelOffset(husk.S.AbsSeekBar_thumbOffset, mThumb != null ? mThumb.getIntrinsicWidth() / 2 : (int) (10 * c.getResources().getDisplayMetrics().density));
        a.recycle();
        setFocusable(true);
        setMinHeight((int) (32 * c.getResources().getDisplayMetrics().density));
    }
    public void setThumb(Drawable t) { if (mThumb != null) mThumb.setCallback(null); mThumb = t; if (t != null) { t.setCallback(this); mThumbOffset = t.getIntrinsicWidth() / 2; } invalidate(); }
    public Drawable getThumb() { return mThumb; }
    public void setThumbTintList(ColorStateList t) { mThumbTint = t; invalidate(); }
    public ColorStateList getThumbTintList() { return mThumbTint; }
    public void setThumbTintMode(PorterDuff.Mode m) {}
    public int getThumbOffset() { return mThumbOffset; }
    public void setThumbOffset(int o) { mThumbOffset = o; invalidate(); }
    public void setSplitTrack(boolean s) {}
    public boolean getSplitTrack() { return false; }
    public void setTickMark(Drawable d) {}
    public Drawable getTickMark() { return null; }
    public void setKeyProgressIncrement(int i) { mKeyProgressIncrement = i < 0 ? -i : i; }
    public int getKeyProgressIncrement() { return mKeyProgressIncrement; }
    @Override public synchronized void setMax(int m) { super.setMax(m); int range = getMax() - getMin(); if (mKeyProgressIncrement == 0 || range / mKeyProgressIncrement > 20) setKeyProgressIncrement(Math.max(1, Math.round((float) range / 20))); }
    @Override protected boolean verifyDrawable(Drawable who) { return who == mThumb || super.verifyDrawable(who); }
    @Override protected void drawableStateChanged() { super.drawableStateChanged(); if (mThumb != null && mThumb.isStateful()) mThumb.setState(getDrawableState()); }
    private float trackLeft() { return getPaddingLeft() + mThumbOffset; }
    private float trackWidth() { return getWidth() - getPaddingLeft() - getPaddingRight() - 2 * mThumbOffset; }
    @Override protected synchronized void onDraw(Canvas c) {
        int save = c.save();
        float d = getResources().getDisplayMetrics().density;
        int h = getHeight(), cy = h / 2;
        float left = trackLeft(), w = trackWidth(), x = left + w * getScale();
        int accent = 0xFF6750A4;
        android.util.TypedValue v = new android.util.TypedValue();
        if (getContext().getTheme().resolveAttribute(android.R.attr.colorAccent, v, true) && v.type >= android.util.TypedValue.TYPE_FIRST_COLOR_INT && v.type <= android.util.TypedValue.TYPE_LAST_COLOR_INT) accent = v.data;
        if (getProgressDrawable() != null) {
            Drawable pd = getProgressDrawable();
            int th = Math.min(pd.getIntrinsicHeight() > 0 ? pd.getIntrinsicHeight() : (int) (4 * d), h);
            pd.setBounds((int) left, cy - th / 2, (int) (left + w), cy + th / 2);
            pd.draw(c);
        } else {
            int col = getProgressTintList() != null ? getProgressTintList().getColorForState(getDrawableState(), accent) : accent;
            mP.setStyle(Paint.Style.FILL);
            mP.setColor((col & 0xFFFFFF) | 0x40000000); c.drawRect(left, cy - 2 * d, left + w, cy + 2 * d, mP);
            mP.setColor(col); c.drawRect(left, cy - 2 * d, x, cy + 2 * d, mP);
        }
        if (mThumb != null) {
            int tw = mThumb.getIntrinsicWidth(), th = mThumb.getIntrinsicHeight();
            if (mThumbTint != null) mThumb.setTintList(mThumbTint);
            mThumb.setBounds((int) (x - tw / 2f), cy - th / 2, (int) (x + tw / 2f), cy + th / 2);
            mThumb.draw(c);
        } else {
            int col = mThumbTint != null ? mThumbTint.getColorForState(getDrawableState(), accent) : accent;
            mP.setColor(col);
            c.drawCircle(x, cy, (mIsDragging ? 12 : 10) * d, mP);
        }
        c.restoreToCount(save);
    }
    @Override protected synchronized void onMeasure(int ws, int hs) {
        float d = getResources().getDisplayMetrics().density;
        int dh = Math.max(getMinHeight(), mThumb != null ? mThumb.getIntrinsicHeight() : (int) (24 * d)) + getPaddingTop() + getPaddingBottom();
        int dw = (int) (64 * d) + getPaddingLeft() + getPaddingRight();
        setMeasuredDimension(resolveSizeAndState(dw, ws, 0), resolveSizeAndState(dh, hs, 0));
    }
    @Override public boolean onTouchEvent(MotionEvent e) {
        if (!mIsUserSeekable || !isEnabled()) return false;
        switch (e.getAction()) {
        case MotionEvent.ACTION_DOWN: setPressed(true); onStartTrackingTouch(); trackTouchEvent(e); if (getParent() != null) getParent().requestDisallowInterceptTouchEvent(true); break;
        case MotionEvent.ACTION_MOVE: if (mIsDragging) trackTouchEvent(e); break;
        case MotionEvent.ACTION_UP: if (mIsDragging) { trackTouchEvent(e); onStopTrackingTouch(); setPressed(false); } invalidate(); break;
        case MotionEvent.ACTION_CANCEL: if (mIsDragging) { onStopTrackingTouch(); setPressed(false); } invalidate(); break;
        }
        return true;
    }
    private void trackTouchEvent(MotionEvent e) {
        float w = trackWidth(), x = e.getX() - trackLeft();
        float scale = w <= 0 ? 0 : Math.max(0, Math.min(1, x / w));
        int range = getMax() - getMin();
        setProgressInternal(Math.round(scale * range + getMin()), true, false);
    }
    void onStartTrackingTouch() { mIsDragging = true; }
    void onStopTrackingTouch() { mIsDragging = false; }
    void onKeyChange() {}
    @Override public boolean onKeyDown(int keyCode, KeyEvent e) {
        if (isEnabled()) {
            int inc = mKeyProgressIncrement;
            if (keyCode == KeyEvent.KEYCODE_DPAD_LEFT || keyCode == KeyEvent.KEYCODE_MINUS) inc = -inc;
            else if (!(keyCode == KeyEvent.KEYCODE_DPAD_RIGHT || keyCode == KeyEvent.KEYCODE_PLUS || keyCode == KeyEvent.KEYCODE_EQUALS)) return super.onKeyDown(keyCode, e);
            if (setProgressInternal(getProgress() + inc, true, true)) { onKeyChange(); return true; }
        }
        return super.onKeyDown(keyCode, e);
    }
    @Override public CharSequence getAccessibilityClassName() { return AbsSeekBar.class.getName(); }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
    protected void drawTickMarks(android.graphics.Canvas p0) {}
    public android.graphics.BlendMode getThumbTintBlendMode() { return (android.graphics.BlendMode) huskFill.get("ThumbTintBlendMode"); }
    public android.graphics.PorterDuff.Mode getThumbTintMode() { return null; }
    public android.graphics.BlendMode getTickMarkTintBlendMode() { return (android.graphics.BlendMode) huskFill.get("TickMarkTintBlendMode"); }
    public android.content.res.ColorStateList getTickMarkTintList() { return (android.content.res.ColorStateList) huskFill.get("TickMarkTintList"); }
    public android.graphics.PorterDuff.Mode getTickMarkTintMode() { return (android.graphics.PorterDuff.Mode) huskFill.get("TickMarkTintMode"); }
    public void growRectTo(android.graphics.Rect p0, int p1) {}
    public void onInitializeAccessibilityNodeInfoInternal(android.view.accessibility.AccessibilityNodeInfo p0) {}
    public void onResolveDrawables(int p0) {}
    public boolean performAccessibilityActionInternal(int p0, android.os.Bundle p1) { return false; }
    public void setThumbTintBlendMode(android.graphics.BlendMode p0) { huskFill.put("ThumbTintBlendMode", p0); }
    public void setTickMarkTintBlendMode(android.graphics.BlendMode p0) { huskFill.put("TickMarkTintBlendMode", p0); }
    public void setTickMarkTintList(android.content.res.ColorStateList p0) { huskFill.put("TickMarkTintList", p0); }
    public void setTickMarkTintMode(android.graphics.PorterDuff.Mode p0) { huskFill.put("TickMarkTintMode", p0); }
    protected int updateTouchProgress(int p0, int p1) { return 0; }
    // ---- end of generated members
}
