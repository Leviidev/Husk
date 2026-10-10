package android.widget;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.Gravity;

/** A button with a checked state, its check mark (the button drawable) at its start. */
public abstract class CompoundButton extends Button implements Checkable {
    public interface OnCheckedChangeListener { void onCheckedChanged(CompoundButton b, boolean checked); }
    private static final int[] CHECKED_STATE_SET = { android.R.attr.state_checked };
    private boolean mChecked, mBroadcasting;
    private Drawable mButtonDrawable;
    private ColorStateList mButtonTint;
    private PorterDuff.Mode mButtonTintMode;
    private OnCheckedChangeListener mOnChecked, mOnCheckedInternal;
    public CompoundButton(Context c) { this(c, null); }
    public CompoundButton(Context c, AttributeSet a) { this(c, a, 0); }
    public CompoundButton(Context c, AttributeSet a, int defStyleAttr) { this(c, a, defStyleAttr, 0); }
    public CompoundButton(Context c, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(c, attrs, defStyleAttr, defStyleRes);
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.CompoundButton, defStyleAttr, defStyleRes);
        Drawable d = a.getDrawable(husk.S.CompoundButton_button);
        if (d != null) setButtonDrawable(d);
        if (a.hasValue(husk.S.CompoundButton_buttonTint)) mButtonTint = a.getColorStateList(husk.S.CompoundButton_buttonTint);
        boolean checked = a.getBoolean(husk.S.CompoundButton_checked, false);
        a.recycle();
        setChecked(checked);
        applyButtonTint();
    }
    public void toggle() { setChecked(!mChecked); }
    @Override public boolean performClick() { toggle(); boolean h = super.performClick(); if (!h) playSoundEffect(android.view.SoundEffectConstants.CLICK); return h; }
    public boolean isChecked() { return mChecked; }
    public void setChecked(boolean checked) {
        if (mChecked != checked) {
            mChecked = checked;
            refreshDrawableState();
            if (mBroadcasting) return;
            mBroadcasting = true;
            if (mOnChecked != null) mOnChecked.onCheckedChanged(this, mChecked);
            if (mOnCheckedInternal != null) mOnCheckedInternal.onCheckedChanged(this, mChecked);
            mBroadcasting = false;
        }
    }
    public void setOnCheckedChangeListener(OnCheckedChangeListener l) { mOnChecked = l; }
    void setOnCheckedChangeWidgetListener(OnCheckedChangeListener l) { mOnCheckedInternal = l; }
    public void setButtonDrawable(int resId) { setButtonDrawable(resId != 0 ? getContext().getDrawable(resId) : null); }
    public void setButtonDrawable(Drawable d) {
        if (mButtonDrawable != d) {
            if (mButtonDrawable != null) { mButtonDrawable.setCallback(null); unscheduleDrawable(mButtonDrawable); }
            mButtonDrawable = d;
            if (d != null) { d.setCallback(this); if (d.isStateful()) d.setState(getDrawableState()); d.setVisible(getVisibility() == VISIBLE, false); setMinHeight(d.getIntrinsicHeight()); applyButtonTint(); }
            requestLayout();
        }
    }
    public void setButtonIcon(android.graphics.drawable.Icon i) { setButtonDrawable(i == null ? null : i.loadDrawable(getContext())); }
    public Drawable getButtonDrawable() { return mButtonDrawable; }
    public void setButtonTintList(ColorStateList t) { mButtonTint = t; applyButtonTint(); }
    public ColorStateList getButtonTintList() { return mButtonTint; }
    public void setButtonTintMode(PorterDuff.Mode m) { mButtonTintMode = m; applyButtonTint(); }
    public PorterDuff.Mode getButtonTintMode() { return mButtonTintMode; }
    private void applyButtonTint() {
        if (mButtonDrawable != null && (mButtonTint != null || mButtonTintMode != null)) {
            mButtonDrawable = mButtonDrawable.mutate();
            if (mButtonTint != null) mButtonDrawable.setTintList(mButtonTint);
            if (mButtonTintMode != null) mButtonDrawable.setTintMode(mButtonTintMode);
            if (mButtonDrawable.isStateful()) mButtonDrawable.setState(getDrawableState());
        }
    }
    @Override public int getCompoundPaddingLeft() { int p = super.getCompoundPaddingLeft(); Drawable d = mButtonDrawable; if (d != null) p += d.getIntrinsicWidth(); return p; }
    @Override protected void onDraw(Canvas c) {
        Drawable d = mButtonDrawable;
        if (d != null) {
            int vg = getGravity() & Gravity.VERTICAL_GRAVITY_MASK, h = d.getIntrinsicHeight(), w = d.getIntrinsicWidth();
            int top = vg == Gravity.BOTTOM ? getHeight() - h : vg == Gravity.CENTER_VERTICAL ? (getHeight() - h) / 2 : 0;
            int left = getPaddingLeft();
            d.setBounds(left, top, left + w, top + h);
            if (getScrollX() == 0 && getScrollY() == 0) d.draw(c);
            else { c.translate(getScrollX(), getScrollY()); d.draw(c); c.translate(-getScrollX(), -getScrollY()); }
        }
        super.onDraw(c);
    }
    @Override protected int[] onCreateDrawableState(int extra) { int[] s = super.onCreateDrawableState(extra + 1); if (isChecked()) mergeDrawableStates(s, CHECKED_STATE_SET); return s; }
    @Override protected void drawableStateChanged() { super.drawableStateChanged(); if (mButtonDrawable != null && mButtonDrawable.isStateful() && mButtonDrawable.setState(getDrawableState())) invalidate(); }
    @Override protected boolean verifyDrawable(android.graphics.drawable.Drawable who) { return super.verifyDrawable(who) || who == mButtonDrawable; }
    @Override public void jumpDrawablesToCurrentState() { super.jumpDrawablesToCurrentState(); if (mButtonDrawable != null) mButtonDrawable.jumpToCurrentState(); }
    @Override public CharSequence getAccessibilityClassName() { return CompoundButton.class.getName(); }
}
