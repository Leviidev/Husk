package android.view.animation;

/** The old view animations: a transformation applied while the view draws; the view itself does not move. */
public abstract class Animation implements Cloneable {
    public static final int INFINITE = -1, RESTART = 1, REVERSE = 2, START_ON_FIRST_FRAME = -1, ABSOLUTE = 0, RELATIVE_TO_SELF = 1, RELATIVE_TO_PARENT = 2,
        ZORDER_NORMAL = 0, ZORDER_TOP = 1, ZORDER_BOTTOM = -1;
    public interface AnimationListener { void onAnimationStart(Animation a); void onAnimationEnd(Animation a); void onAnimationRepeat(Animation a); }
    protected static class Description { public int type; public float value; }
    long mStartTime = -1, mStartOffset, mDuration;
    int mRepeatCount, mRepeated, mRepeatMode = RESTART;
    boolean mFillBefore = true, mFillAfter, mFillEnabled, mStarted, mEnded, mInitialized, mCycleFlip;
    protected Interpolator mInterpolator;
    private AnimationListener mListener;
    public Animation() { mInterpolator = new AccelerateDecelerateInterpolator(); }
    public Animation(android.content.Context c, android.util.AttributeSet a) { this(); }
    public void reset() { mInitialized = false; mStarted = false; mEnded = false; mRepeated = 0; mCycleFlip = false; mStartTime = -1; }
    public void cancel() { if (mStarted && !mEnded) { mEnded = true; if (mListener != null) mListener.onAnimationEnd(this); } mStartTime = Long.MIN_VALUE; }
    public boolean isInitialized() { return mInitialized; }
    public void initialize(int w, int h, int pw, int ph) { reset(); mInitialized = true; }
    public void setInterpolator(android.content.Context c, int id) { setInterpolator(AnimationUtils.loadInterpolator(c, id)); }
    public void setInterpolator(Interpolator i) { mInterpolator = i; }
    public Interpolator getInterpolator() { return mInterpolator; }
    public void setStartOffset(long o) { mStartOffset = o; }
    public long getStartOffset() { return mStartOffset; }
    public void setDuration(long d) { mDuration = Math.max(0, d); }
    public long getDuration() { return mDuration; }
    public long computeDurationHint() { return (mStartOffset + mDuration) * (mRepeatCount < 0 ? 1 : mRepeatCount + 1); }
    public void restrictDuration(long d) {}
    public void scaleCurrentDuration(float s) { mDuration = (long) (mDuration * s); }
    public void setStartTime(long t) { mStartTime = t; mStarted = mEnded = false; mRepeated = 0; mCycleFlip = false; }
    public long getStartTime() { return mStartTime; }
    public void start() { setStartTime(-1); }
    public void startNow() { setStartTime(AnimationUtils.currentAnimationTimeMillis()); }
    public void setRepeatMode(int m) { mRepeatMode = m; }
    public int getRepeatMode() { return mRepeatMode; }
    public void setRepeatCount(int c) { mRepeatCount = c < 0 ? INFINITE : c; }
    public int getRepeatCount() { return mRepeatCount; }
    public void setFillEnabled(boolean f) { mFillEnabled = f; }
    public boolean isFillEnabled() { return mFillEnabled; }
    public void setFillBefore(boolean f) { mFillBefore = f; }
    public boolean getFillBefore() { return mFillBefore; }
    public void setFillAfter(boolean f) { mFillAfter = f; }
    public boolean getFillAfter() { return mFillAfter; }
    public void setZAdjustment(int z) {}
    public void setBackgroundColor(int c) {}
    public void setDetachWallpaper(boolean d) {}
    public boolean willChangeTransformationMatrix() { return true; }
    public boolean willChangeBounds() { return true; }
    public boolean hasStarted() { return mStarted; }
    public boolean hasEnded() { return mEnded; }
    public void setAnimationListener(AnimationListener l) { mListener = l; }
    protected void applyTransformation(float t, Transformation out) {}
    protected float resolveSize(int type, float value, int size, int parentSize) { return type == RELATIVE_TO_SELF ? size * value : type == RELATIVE_TO_PARENT ? parentSize * value : value; }
    protected void ensureInterpolator() { if (mInterpolator == null) mInterpolator = new AccelerateDecelerateInterpolator(); }
    /** True while it still has frames to give. */
    public boolean getTransformation(long now, Transformation out) {
        if (mStartTime == -1) mStartTime = now;
        long start = mStartTime + mStartOffset;
        float t = mDuration != 0 ? (float) (now - start) / mDuration : now < start ? 0 : 1;
        boolean expired = t >= 1 || mStartTime == Long.MIN_VALUE;
        boolean more = !expired;
        if (!mFillEnabled) t = Math.max(Math.min(t, 1), 0);
        if ((t >= 0 || mFillBefore) && (t <= 1 || mFillAfter)) {
            if (!mStarted) { mStarted = true; if (mListener != null) mListener.onAnimationStart(this); }
            if (mFillEnabled) t = Math.max(Math.min(t, 1), 0);
            if (mCycleFlip) t = 1 - t;
            applyTransformation(mInterpolator.getInterpolation(t), out);
        }
        if (expired) {
            if (mRepeatCount == mRepeated || mStartTime == Long.MIN_VALUE) {
                if (!mEnded) { mEnded = true; if (mListener != null) mListener.onAnimationEnd(this); }
            } else {
                if (mRepeatCount > 0) mRepeated++;
                if (mRepeatMode == REVERSE) mCycleFlip = !mCycleFlip;
                mStartTime = -1; more = true;
                if (mListener != null) mListener.onAnimationRepeat(this);
            }
        }
        return more;
    }
    public boolean getTransformation(long now, Transformation out, float scale) { return getTransformation(now, out); }
    @Override protected Animation clone() throws CloneNotSupportedException { return (Animation) super.clone(); }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
    public void detach() {}
    public int getBackdropColor() { return (huskFill.get("BackdropColor") instanceof Integer ? (Integer) huskFill.get("BackdropColor") : 0); }
    public int getBackgroundColor() { return 0; }
    public boolean getDetachWallpaper() { return false; }
    public int getExtensionEdges() { return 0; }
    public void getInvalidateRegion(int p0, int p1, int p2, int p3, android.graphics.RectF p4, android.view.animation.Transformation p5) {}
    protected float getScaleFactor() { return 0f; }
    public boolean getShowBackdrop() { return (huskFill.get("ShowBackdrop") instanceof Boolean ? (Boolean) huskFill.get("ShowBackdrop") : false); }
    public boolean getShowWallpaper() { return (huskFill.get("ShowWallpaper") instanceof Boolean ? (Boolean) huskFill.get("ShowWallpaper") : false); }
    public void getTransformationAt(float p0, android.view.animation.Transformation p1) {}
    public int getZAdjustment() { return 0; }
    public boolean hasAlpha() { return false; }
    public boolean hasRoundedCorners() { return false; }
    public void initializeInvalidateRegion(int p0, int p1, int p2, int p3) {}
    public void setBackdropColor(int p0) { huskFill.put("BackdropColor", Integer.valueOf(p0)); }
    public void setHasRoundedCorners(boolean p0) {}
    public void setListenerHandler(android.os.Handler p0) {}
    public void setShowBackdrop(boolean p0) { huskFill.put("ShowBackdrop", Boolean.valueOf(p0)); }
    public void setShowWallpaper(boolean p0) { huskFill.put("ShowWallpaper", Boolean.valueOf(p0)); }
    // ---- end of generated members
}
