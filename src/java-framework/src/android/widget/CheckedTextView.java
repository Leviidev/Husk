package android.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

public class CheckedTextView extends TextView implements Checkable {
    private static final int[] CHECKED_STATE_SET = { android.R.attr.state_checked };
    private boolean mChecked;
    private Drawable mCheckMark;
    public CheckedTextView(Context c) { this(c, null); }
    public CheckedTextView(Context c, AttributeSet a) { this(c, a, android.R.attr.checkedTextViewStyle); }
    public CheckedTextView(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public CheckedTextView(Context c, AttributeSet attrs, int s, int r) {
        super(c, attrs, s, r);
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.CheckedTextView, s, r);
        Drawable d = a.getDrawable(husk.S.CheckedTextView_checkMark);
        if (d != null) setCheckMarkDrawable(d);
        setChecked(a.getBoolean(husk.S.CheckedTextView_checked, false));
        a.recycle();
    }
    public void toggle() { setChecked(!mChecked); }
    public boolean isChecked() { return mChecked; }
    public void setChecked(boolean c) { if (mChecked != c) { mChecked = c; refreshDrawableState(); } }
    public void setCheckMarkDrawable(int r) { setCheckMarkDrawable(r != 0 ? getContext().getDrawable(r) : null); }
    public void setCheckMarkDrawable(Drawable d) { if (mCheckMark != null) mCheckMark.setCallback(null); mCheckMark = d; if (d != null) { d.setCallback(this); if (d.isStateful()) d.setState(getDrawableState()); } requestLayout(); }
    public Drawable getCheckMarkDrawable() { return mCheckMark; }
    public void setCheckMarkTintList(android.content.res.ColorStateList t) { if (mCheckMark != null) mCheckMark.setTintList(t); }
    @Override public int getCompoundPaddingRight() { return super.getCompoundPaddingRight() + (mCheckMark != null ? mCheckMark.getIntrinsicWidth() : 0); }
    @Override protected int[] onCreateDrawableState(int extra) { int[] s = super.onCreateDrawableState(extra + 1); if (isChecked()) mergeDrawableStates(s, CHECKED_STATE_SET); return s; }
    @Override protected void drawableStateChanged() { super.drawableStateChanged(); if (mCheckMark != null && mCheckMark.isStateful()) mCheckMark.setState(getDrawableState()); invalidate(); }
    @Override protected boolean verifyDrawable(Drawable who) { return who == mCheckMark || super.verifyDrawable(who); }
    @Override protected void onDraw(Canvas c) {
        super.onDraw(c);
        if (mCheckMark != null) {
            int h = mCheckMark.getIntrinsicHeight(), w = mCheckMark.getIntrinsicWidth(), top = (getHeight() - h) / 2, right = getWidth() - getPaddingRight();
            mCheckMark.setBounds(right - w + getScrollX(), top, right + getScrollX(), top + h);
            mCheckMark.draw(c);
        }
    }
}
