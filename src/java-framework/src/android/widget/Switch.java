package android.widget;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.*;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.MotionEvent;

/** A switch: its track and thumb drawn (the theme's drawables when it has them, else a plain Material one), dragged or tapped. */
public class Switch extends CompoundButton {
    private Drawable mThumb, mTrack;
    private CharSequence mTextOn, mTextOff;
    private int mSwitchMinWidth, mSwitchWidth, mSwitchHeight;
    private float mPos;
    private boolean mShowText;
    private final Paint mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private ColorStateList mThumbTint, mTrackTint;
    public Switch(Context c) { this(c, null); }
    public Switch(Context c, AttributeSet a) { this(c, a, android.R.attr.switchStyle); }
    public Switch(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public Switch(Context c, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(c, attrs, defStyleAttr, defStyleRes);
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.Switch, defStyleAttr, defStyleRes);
        mThumb = a.getDrawable(husk.S.Switch_thumb); mTrack = a.getDrawable(husk.S.Switch_track);
        mTextOn = a.getText(husk.S.Switch_textOn); mTextOff = a.getText(husk.S.Switch_textOff);
        mShowText = a.getBoolean(husk.S.Switch_showText, false);
        mSwitchMinWidth = a.getDimensionPixelSize(husk.S.Switch_switchMinWidth, 0);
        mThumbTint = a.getColorStateList(husk.S.Switch_thumbTint); mTrackTint = a.getColorStateList(husk.S.Switch_trackTint);
        a.recycle();
        if (mThumb != null) mThumb.setCallback(this);
        if (mTrack != null) mTrack.setCallback(this);
        mPos = isChecked() ? 1 : 0;
    }
    public void setThumbDrawable(Drawable d) { mThumb = d; requestLayout(); } public Drawable getThumbDrawable() { return mThumb; }
    public void setThumbResource(int r) { setThumbDrawable(getContext().getDrawable(r)); }
    public void setTrackDrawable(Drawable d) { mTrack = d; requestLayout(); } public Drawable getTrackDrawable() { return mTrack; }
    public void setTrackResource(int r) { setTrackDrawable(getContext().getDrawable(r)); }
    public void setThumbTintList(ColorStateList t) { mThumbTint = t; invalidate(); } public void setTrackTintList(ColorStateList t) { mTrackTint = t; invalidate(); }
    public void setTextOn(CharSequence t) { mTextOn = t; } public void setTextOff(CharSequence t) { mTextOff = t; } public CharSequence getTextOn() { return mTextOn; } public CharSequence getTextOff() { return mTextOff; }
    public void setShowText(boolean s) { mShowText = s; } public boolean getShowText() { return mShowText; }
    public void setSwitchMinWidth(int w) { mSwitchMinWidth = w; requestLayout(); } public int getSwitchMinWidth() { return mSwitchMinWidth; }
    public void setSwitchPadding(int p) {} public int getSwitchPadding() { return 0; } public void setThumbTextPadding(int p) {} public void setSplitTrack(boolean s) {}
    public void setSwitchTextAppearance(Context c, int r) {} public void setSwitchTypeface(Typeface t) {}
    private float dp(float v) { return v * getResources().getDisplayMetrics().density; }
    @Override protected void onMeasure(int ws, int hs) {
        mSwitchWidth = Math.max(mSwitchMinWidth, mTrack != null && mTrack.getIntrinsicWidth() > 0 ? mTrack.getIntrinsicWidth() : (int) dp(52));
        mSwitchHeight = Math.max(mThumb != null && mThumb.getIntrinsicHeight() > 0 ? mThumb.getIntrinsicHeight() : 0, (int) dp(32));
        super.onMeasure(ws, hs);
        if (getMeasuredHeight() < mSwitchHeight) setMeasuredDimension(getMeasuredWidthAndState(), mSwitchHeight);
    }
    @Override public int getCompoundPaddingRight() { return super.getCompoundPaddingRight() + mSwitchWidth + (getText() != null && getText().length() > 0 ? (int) dp(16) : 0); }
    @Override public void setChecked(boolean c) { super.setChecked(c); mPos = c ? 1 : 0; invalidate(); }
    private boolean mDragging; private float mDownX;
    @Override public boolean onTouchEvent(MotionEvent e) {
        int right = getWidth() - getPaddingRight(), left = right - mSwitchWidth;
        switch (e.getActionMasked()) {
        case MotionEvent.ACTION_DOWN: mDownX = e.getX(); mDragging = false; break;
        case MotionEvent.ACTION_MOVE: if (Math.abs(e.getX() - mDownX) > dp(8)) { mDragging = true; mPos = Math.max(0, Math.min(1, (e.getX() - left) / (float) mSwitchWidth)); getParent().requestDisallowInterceptTouchEvent(true); invalidate(); return true; } break;
        case MotionEvent.ACTION_UP: if (mDragging) { mDragging = false; boolean on = mPos > 0.5f; setPressed(false); if (on != isChecked()) setChecked(on); else { mPos = on ? 1 : 0; invalidate(); } return true; } break;
        }
        return super.onTouchEvent(e);
    }
    @Override protected void onDraw(Canvas c) {
        super.onDraw(c);
        int right = getWidth() - getPaddingRight(), left = right - mSwitchWidth, cy = getHeight() / 2;
        int[] state = getDrawableState();
        if (mTrack != null && mThumb != null) {
            int th = mThumb.getIntrinsicHeight() > 0 ? mThumb.getIntrinsicHeight() : (int) dp(24), tw = mThumb.getIntrinsicWidth() > 0 ? mThumb.getIntrinsicWidth() : th;
            mTrack.setBounds(left, cy - mSwitchHeight / 2, right, cy + mSwitchHeight / 2);
            if (mTrackTint != null) mTrack.setTintList(mTrackTint);
            mTrack.setState(state); mTrack.draw(c);
            int tx = left + (int) ((mSwitchWidth - tw) * mPos);
            mThumb.setBounds(tx, cy - th / 2, tx + tw, cy + th / 2);
            if (mThumbTint != null) mThumb.setTintList(mThumbTint);
            mThumb.setState(state); mThumb.draw(c);
            return;
        }
        // Material 3 switch, drawn
        int accent = 0xFF6750A4;
        android.util.TypedValue v = new android.util.TypedValue();
        if (getContext().getTheme().resolveAttribute(android.R.attr.colorAccent, v, true) && v.type >= android.util.TypedValue.TYPE_FIRST_INT) accent = v.data;
        float h = dp(32), w = mSwitchWidth, r = h / 2;
        RectF track = new RectF(left, cy - h / 2, left + w, cy + h / 2);
        mPaint.setStyle(Paint.Style.FILL);
        mPaint.setColor(isChecked() ? accent : 0xFFE6E0E9);
        c.drawRoundRect(track, r, r, mPaint);
        if (!isChecked()) { mPaint.setStyle(Paint.Style.STROKE); mPaint.setStrokeWidth(dp(2)); mPaint.setColor(0xFF79747E); c.drawRoundRect(new RectF(track.left + dp(1), track.top + dp(1), track.right - dp(1), track.bottom - dp(1)), r, r, mPaint); mPaint.setStyle(Paint.Style.FILL); }
        float tr = isChecked() ? dp(12) : dp(8);
        float cx = left + r + (w - 2 * r) * mPos;
        mPaint.setColor(isChecked() ? 0xFFFFFFFF : 0xFF79747E);
        c.drawCircle(cx, cy, tr, mPaint);
    }
    @Override public CharSequence getAccessibilityClassName() { return Switch.class.getName(); }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
    protected java.lang.CharSequence getButtonStateDescription() { return null; }
    public boolean getSplitTrack() { return false; }
    public int getThumbTextPadding() { return 0; }
    public android.graphics.BlendMode getThumbTintBlendMode() { return (android.graphics.BlendMode) huskFill.get("ThumbTintBlendMode"); }
    public android.content.res.ColorStateList getThumbTintList() { return null; }
    public android.graphics.PorterDuff.Mode getThumbTintMode() { return (android.graphics.PorterDuff.Mode) huskFill.get("ThumbTintMode"); }
    public android.graphics.BlendMode getTrackTintBlendMode() { return (android.graphics.BlendMode) huskFill.get("TrackTintBlendMode"); }
    public android.content.res.ColorStateList getTrackTintList() { return null; }
    public android.graphics.PorterDuff.Mode getTrackTintMode() { return (android.graphics.PorterDuff.Mode) huskFill.get("TrackTintMode"); }
    public void onPopulateAccessibilityEventInternal(android.view.accessibility.AccessibilityEvent p0) {}
    protected void onProvideStructure(android.view.ViewStructure p0, int p1, int p2) {}
    public void setSwitchTypeface(android.graphics.Typeface p0, int p1) {}
    public void setThumbIcon(android.graphics.drawable.Icon p0) {}
    public java.lang.Runnable setThumbIconAsync(android.graphics.drawable.Icon p0) { return null; }
    public java.lang.Runnable setThumbResourceAsync(int p0) { return null; }
    public void setThumbTintBlendMode(android.graphics.BlendMode p0) { huskFill.put("ThumbTintBlendMode", p0); }
    public void setThumbTintMode(android.graphics.PorterDuff.Mode p0) { huskFill.put("ThumbTintMode", p0); }
    public void setTrackIcon(android.graphics.drawable.Icon p0) {}
    public java.lang.Runnable setTrackIconAsync(android.graphics.drawable.Icon p0) { return null; }
    public java.lang.Runnable setTrackResourceAsync(int p0) { return null; }
    public void setTrackTintBlendMode(android.graphics.BlendMode p0) { huskFill.put("TrackTintBlendMode", p0); }
    public void setTrackTintMode(android.graphics.PorterDuff.Mode p0) { huskFill.put("TrackTintMode", p0); }
    // ---- end of generated members
}
