package android.widget;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

/** A wheel of values: the selected one in the middle with its neighbours above and below; drag or tap above/below to change. */
public class NumberPicker extends LinearLayout {
    public interface OnValueChangeListener { void onValueChange(NumberPicker picker, int oldVal, int newVal); }
    public interface OnScrollListener { int SCROLL_STATE_IDLE = 0, SCROLL_STATE_TOUCH_SCROLL = 1, SCROLL_STATE_FLING = 2; void onScrollStateChange(NumberPicker v, int state); }
    public interface Formatter { String format(int value); }
    private int mMin, mMax, mValue;
    private boolean mWrap = true;
    private String[] mDisplayed;
    private Formatter mFormatter;
    private OnValueChangeListener mOnChange;
    private OnScrollListener mOnScroll;
    private final Paint mPaint = new Paint(Paint.ANTI_ALIAS_FLAG), mDivider = new Paint();
    private final float mD;
    private float mDownY, mOffset;
    private boolean mDragging;
    private int mTextColor = 0xDE000000;
    public NumberPicker(Context c) { this(c, null); }
    public NumberPicker(Context c, AttributeSet a) { this(c, a, android.R.attr.numberPickerStyle); }
    public NumberPicker(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public NumberPicker(Context c, AttributeSet a, int s, int r) {
        super(c, a, s, r);
        mD = c.getResources().getDisplayMetrics().density;
        setWillNotDraw(false);
        mPaint.setTextAlign(Paint.Align.CENTER);
        mPaint.setTextSize(18 * c.getResources().getDisplayMetrics().scaledDensity);
        android.util.TypedValue v = new android.util.TypedValue();
        if (c.getTheme().resolveAttribute(android.R.attr.textColorPrimary, v, true)) { if (v.type >= android.util.TypedValue.TYPE_FIRST_COLOR_INT && v.type <= android.util.TypedValue.TYPE_LAST_COLOR_INT) mTextColor = v.data; else if (v.resourceId != 0) try { mTextColor = c.getColorStateList(v.resourceId).getDefaultColor(); } catch (Exception e) {} }
        int accent = 0xFF6750A4;
        if (c.getTheme().resolveAttribute(android.R.attr.colorAccent, v, true) && v.type >= android.util.TypedValue.TYPE_FIRST_COLOR_INT && v.type <= android.util.TypedValue.TYPE_LAST_COLOR_INT) accent = v.data;
        mDivider.setColor(accent);
        setClickable(true); setFocusable(true);
    }
    public void setOnValueChangedListener(OnValueChangeListener l) { mOnChange = l; }
    public void setOnScrollListener(OnScrollListener l) { mOnScroll = l; }
    public void setFormatter(Formatter f) { mFormatter = f; invalidate(); }
    public void setValue(int v) { setValueInternal(v, false); }
    public int getValue() { return mValue; }
    public int getMinValue() { return mMin; }
    public void setMinValue(int m) { mMin = m; if (mValue < m) mValue = m; invalidate(); }
    public int getMaxValue() { return mMax; }
    public void setMaxValue(int m) { mMax = m; if (mValue > m) mValue = m; invalidate(); }
    public String[] getDisplayedValues() { return mDisplayed; }
    public void setDisplayedValues(String[] d) { mDisplayed = d; invalidate(); }
    public boolean getWrapSelectorWheel() { return mWrap; }
    public void setWrapSelectorWheel(boolean w) { mWrap = w; }
    public void setOnLongPressUpdateInterval(long i) {}
    public void setTextColor(int c) { mTextColor = c; invalidate(); }
    public int getTextColor() { return mTextColor; }
    public void setTextSize(float s) { mPaint.setTextSize(s); invalidate(); }
    public float getTextSize() { return mPaint.getTextSize(); }
    public void setSelectionDividerHeight(int h) {} public int getSelectionDividerHeight() { return (int) (2 * mD); }
    public void setSelectionDividerColor(int c) { mDivider.setColor(c); }
    public int getSolidColor() { return 0; }
    public void scrollBy(int x, int y) {}
    private void setValueInternal(int v, boolean notify) {
        if (mMax < mMin) return;
        if (mWrap) { int range = mMax - mMin + 1; v = ((v - mMin) % range + range) % range + mMin; } else v = Math.max(mMin, Math.min(mMax, v));
        int old = mValue;
        mValue = v;
        invalidate();
        if (notify && old != v && mOnChange != null) mOnChange.onValueChange(this, old, v);
    }
    private String label(int v) {
        if (mDisplayed != null) { int i = v - mMin; return i >= 0 && i < mDisplayed.length ? mDisplayed[i] : ""; }
        return mFormatter != null ? mFormatter.format(v) : String.valueOf(v);
    }
    private float rowH() { return 48 * mD; }
    @Override protected void onMeasure(int ws, int hs) {
        float w = 0;
        for (int v = mMin; v <= Math.min(mMax, mMin + 60); v++) w = Math.max(w, mPaint.measureText(label(v)));
        int dw = (int) Math.max(64 * mD, w + 32 * mD) + getPaddingLeft() + getPaddingRight(), dh = (int) (rowH() * 3) + getPaddingTop() + getPaddingBottom();
        setMeasuredDimension(resolveSizeAndState(dw, ws, 0), resolveSizeAndState(dh, hs, 0));
    }
    @Override protected void onDraw(Canvas c) {
        float cx = getWidth() / 2f, cy = getHeight() / 2f, rh = rowH();
        Paint.FontMetrics fm = mPaint.getFontMetrics();
        float base = -(fm.ascent + fm.descent) / 2;
        for (int k = -2; k <= 2; k++) {
            int v = mValue + k;
            if (!mWrap && (v < mMin || v > mMax)) continue;
            if (mWrap) { int range = mMax - mMin + 1; if (range <= 0) continue; v = ((v - mMin) % range + range) % range + mMin; }
            float y = cy + k * rh + mOffset;
            float dist = Math.abs(y - cy) / (rh * 1.5f);
            mPaint.setColor(mTextColor);
            mPaint.setAlpha((int) (255 * Math.max(0.15f, 1 - dist * 0.7f)));
            c.drawText(label(v), cx, y + base, mPaint);
        }
        c.drawRect(getPaddingLeft(), cy - rh / 2, getWidth() - getPaddingRight(), cy - rh / 2 + 2 * mD, mDivider);
        c.drawRect(getPaddingLeft(), cy + rh / 2 - 2 * mD, getWidth() - getPaddingRight(), cy + rh / 2, mDivider);
    }
    @Override public boolean onInterceptTouchEvent(MotionEvent e) { return true; }
    @Override public boolean onTouchEvent(MotionEvent e) {
        if (!isEnabled()) return false;
        switch (e.getActionMasked()) {
        case MotionEvent.ACTION_DOWN: mDownY = e.getY(); mDragging = false; mOffset = 0; if (getParent() != null) getParent().requestDisallowInterceptTouchEvent(true); return true;
        case MotionEvent.ACTION_MOVE: {
            float dy = e.getY() - mDownY;
            if (!mDragging && Math.abs(dy) > 8 * mD) { mDragging = true; if (mOnScroll != null) mOnScroll.onScrollStateChange(this, OnScrollListener.SCROLL_STATE_TOUCH_SCROLL); }
            if (mDragging) {
                float rh = rowH();
                mOffset = dy;
                while (mOffset > rh / 2) { if (!mWrap && mValue <= mMin) { mOffset = Math.min(mOffset, rh / 2); break; } setValueInternal(mValue - 1, true); mDownY += rh; mOffset -= rh; }
                while (mOffset < -rh / 2) { if (!mWrap && mValue >= mMax) { mOffset = Math.max(mOffset, -rh / 2); break; } setValueInternal(mValue + 1, true); mDownY -= rh; mOffset += rh; }
                invalidate();
            }
            return true;
        }
        case MotionEvent.ACTION_UP:
            if (!mDragging) { float cy = getHeight() / 2f, rh = rowH(); if (e.getY() < cy - rh / 2) setValueInternal(mValue - 1, true); else if (e.getY() > cy + rh / 2) setValueInternal(mValue + 1, true); else performClick(); }
            else if (mOnScroll != null) mOnScroll.onScrollStateChange(this, OnScrollListener.SCROLL_STATE_IDLE);
            mOffset = 0; mDragging = false; invalidate();
            return true;
        case MotionEvent.ACTION_CANCEL: mOffset = 0; mDragging = false; invalidate(); return true;
        }
        return super.onTouchEvent(e);
    }
    @Override public CharSequence getAccessibilityClassName() { return NumberPicker.class.getName(); }
}
