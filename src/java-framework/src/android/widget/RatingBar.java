package android.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.*;
import android.util.AttributeSet;

/** Stars: drawn (filled to the rating, in the accent colour), tapped or dragged when not an indicator. */
public class RatingBar extends AbsSeekBar {
    public interface OnRatingBarChangeListener { void onRatingChanged(RatingBar r, float rating, boolean fromUser); }
    private int mNumStars = 5, mProgressOnStartTracking;
    private OnRatingBarChangeListener mListener;
    private final Paint mP = new Paint(Paint.ANTI_ALIAS_FLAG);
    public RatingBar(Context c) { this(c, null); }
    public RatingBar(Context c, AttributeSet a) { this(c, a, android.R.attr.ratingBarStyle); }
    public RatingBar(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public RatingBar(Context c, AttributeSet attrs, int s, int r) {
        super(c, attrs, s, r);
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.RatingBar, s, r);
        int numStars = a.getInt(husk.S.RatingBar_numStars, mNumStars);
        setIsIndicator(a.getBoolean(husk.S.RatingBar_isIndicator, !mIsUserSeekable));
        float rating = a.getFloat(husk.S.RatingBar_rating, -1), stepSize = a.getFloat(husk.S.RatingBar_stepSize, -1);
        a.recycle();
        if (numStars > 0 && numStars != mNumStars) setNumStars(numStars);
        if (stepSize >= 0) setStepSize(stepSize); else setStepSize(0.5f);
        if (rating >= 0) setRating(rating);
        mTouchProgressOffset = 0.6f;
    }
    public void setOnRatingBarChangeListener(OnRatingBarChangeListener l) { mListener = l; }
    public OnRatingBarChangeListener getOnRatingBarChangeListener() { return mListener; }
    public void setIsIndicator(boolean i) { mIsUserSeekable = !i; setFocusable(!i); }
    public boolean isIndicator() { return !mIsUserSeekable; }
    public void setNumStars(int n) { if (n <= 0) return; mNumStars = n; requestLayout(); }
    public int getNumStars() { return mNumStars; }
    public void setRating(float r) { setProgress(Math.round(r * getProgressPerStar())); }
    public float getRating() { return getProgress() / getProgressPerStar(); }
    public void setStepSize(float s) { if (s <= 0) return; final float newMax = mNumStars / s; int newProgress = (int) (newMax / getMax() * getProgress()); setMax((int) newMax); setProgress(newProgress); }
    public float getStepSize() { return (float) getNumStars() / getMax(); }
    private float getProgressPerStar() { return mNumStars > 0 ? 1f * getMax() / mNumStars : 1; }
    @Override void onProgressRefresh(float scale, boolean fromUser, int progress) { super.onProgressRefresh(scale, fromUser, progress); if (!fromUser && mListener != null) mListener.onRatingChanged(this, getRating(), false); }
    @Override void onStartTrackingTouch() { mProgressOnStartTracking = getProgress(); super.onStartTrackingTouch(); }
    @Override void onStopTrackingTouch() { super.onStopTrackingTouch(); if (getProgress() != mProgressOnStartTracking && mListener != null) mListener.onRatingChanged(this, getRating(), true); }
    @Override void onKeyChange() { super.onKeyChange(); if (mListener != null) mListener.onRatingChanged(this, getRating(), true); }
    @Override public synchronized void setMax(int max) { if (max <= 0) return; super.setMax(max); }
    private float starSize() { float d = getResources().getDisplayMetrics().density; return getMinHeight() > 0 && getMinHeight() < 30 * d ? getMinHeight() : 36 * d; }
    @Override protected synchronized void onMeasure(int ws, int hs) {
        float s = starSize();
        setMeasuredDimension(resolveSizeAndState((int) (s * mNumStars) + getPaddingLeft() + getPaddingRight(), ws, 0), resolveSizeAndState((int) s + getPaddingTop() + getPaddingBottom(), hs, 0));
    }
    @Override public boolean onTouchEvent(android.view.MotionEvent e) {
        if (!mIsUserSeekable || !isEnabled()) return false;
        float s = (getWidth() - getPaddingLeft() - getPaddingRight()) / (float) mNumStars;
        float stars = Math.max(0, Math.min(mNumStars, (e.getX() - getPaddingLeft()) / s + 0.25f));
        int a = e.getActionMasked();
        if (a == android.view.MotionEvent.ACTION_DOWN) { setPressed(true); onStartTrackingTouch(); if (getParent() != null) getParent().requestDisallowInterceptTouchEvent(true); }
        if (a == android.view.MotionEvent.ACTION_DOWN || a == android.view.MotionEvent.ACTION_MOVE || a == android.view.MotionEvent.ACTION_UP) { float step = getStepSize(); setProgressInternal(Math.round(Math.round(stars / step) * step * getProgressPerStar()), true, false); }
        if (a == android.view.MotionEvent.ACTION_UP || a == android.view.MotionEvent.ACTION_CANCEL) { onStopTrackingTouch(); setPressed(false); }
        return true;
    }
    private static Path star(float cx, float cy, float r) {
        Path p = new Path();
        for (int i = 0; i < 10; i++) { double ang = -Math.PI / 2 + i * Math.PI / 5; float rr = i % 2 == 0 ? r : r * 0.45f; float x = cx + (float) (Math.cos(ang) * rr), y = cy + (float) (Math.sin(ang) * rr); if (i == 0) p.moveTo(x, y); else p.lineTo(x, y); }
        p.close();
        return p;
    }
    @Override protected synchronized void onDraw(Canvas c) {
        int accent = 0xFF6750A4;
        android.util.TypedValue v = new android.util.TypedValue();
        if (getContext().getTheme().resolveAttribute(android.R.attr.colorAccent, v, true) && v.type >= android.util.TypedValue.TYPE_FIRST_COLOR_INT && v.type <= android.util.TypedValue.TYPE_LAST_COLOR_INT) accent = v.data;
        if (getProgressTintList() != null) accent = getProgressTintList().getColorForState(getDrawableState(), accent);
        float s = (getWidth() - getPaddingLeft() - getPaddingRight()) / (float) mNumStars, r = Math.min(s, getHeight() - getPaddingTop() - getPaddingBottom()) * 0.48f, cy = getPaddingTop() + (getHeight() - getPaddingTop() - getPaddingBottom()) / 2f;
        float rating = getRating();
        mP.setStyle(Paint.Style.FILL);
        for (int i = 0; i < mNumStars; i++) {
            float cx = getPaddingLeft() + s * (i + 0.5f);
            Path p = star(cx, cy, r);
            mP.setColor(0x33000000); c.drawPath(p, mP);
            float fill = Math.max(0, Math.min(1, rating - i));
            if (fill > 0) { int sv = c.save(); c.clipRect(cx - r, cy - r, cx - r + 2 * r * fill, cy + r); mP.setColor(accent); c.drawPath(p, mP); c.restoreToCount(sv); }
        }
    }
    @Override public CharSequence getAccessibilityClassName() { return RatingBar.class.getName(); }
    // ---- generated by tools/compat/genstubs.py: the platform's nested classes this class does not write
    public static final class InspectionCompanion implements android.view.inspector.InspectionCompanion {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public InspectionCompanion() {}
        public void mapProperties(android.view.inspector.PropertyMapper p0) {}
        public void readProperties(android.widget.RatingBar p0, android.view.inspector.PropertyReader p1) {}
        public void readProperties(java.lang.Object p0, android.view.inspector.PropertyReader p1) {}
    }
    // ---- end of generated nested classes
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public static final java.lang.String PLURALS_MAX = "max";
    public static final java.lang.String PLURALS_RATING = "rating";
    public void onInitializeAccessibilityNodeInfoInternal(android.view.accessibility.AccessibilityNodeInfo p0) {}
    // ---- end of generated members
}
