package android.animation;

import android.view.Choreographer;
import java.util.ArrayList;
import java.util.HashMap;

/** Driven by the thread's Choreographer: one frame callback runs every animator that is going. */
public class ValueAnimator extends Animator {
    public static final int RESTART = 1, REVERSE = 2, INFINITE = -1;
    public interface AnimatorUpdateListener { void onAnimationUpdate(ValueAnimator a); }
    private static float sDurationScale = 1;
    private static final TimeInterpolator sDefault = new android.view.animation.AccelerateDecelerateInterpolator();

    PropertyValuesHolder[] mValues;
    HashMap<String, PropertyValuesHolder> mValuesMap;
    private long mDuration = 300, mStartDelay, mStartTime = -1, mSeekTime = -1, mPauseTime;
    private TimeInterpolator mInterpolator = sDefault;
    private int mRepeatCount, mRepeatMode = RESTART, mIteration;
    private boolean mRunning, mStarted, mReversing, mStartListenersCalled;
    private float mFraction, mOverallFraction;
    private ArrayList<AnimatorUpdateListener> mUpdate;

    public ValueAnimator() {}
    public static void setDurationScale(float s) { sDurationScale = s; }
    public static float getDurationScale() { return sDurationScale; }
    public static boolean areAnimatorsEnabled() { return sDurationScale != 0; }
    /** Hidden API (Compose and Material read it to follow the animator duration scale setting): the scale never changes here. */
    public interface DurationScaleChangeListener { void onChanged(float scale); }
    public static boolean registerDurationScaleChangeListener(DurationScaleChangeListener l) { return true; }
    public static boolean unregisterDurationScaleChangeListener(DurationScaleChangeListener l) { return true; }
    public static long getFrameDelay() { return 16; }
    public static void setFrameDelay(long d) {}
    public static ValueAnimator ofInt(int... v) { ValueAnimator a = new ValueAnimator(); a.setIntValues(v); return a; }
    public static ValueAnimator ofArgb(int... v) { ValueAnimator a = new ValueAnimator(); a.setIntValues(v); a.setEvaluator(ArgbEvaluator.getInstance()); return a; }
    public static ValueAnimator ofFloat(float... v) { ValueAnimator a = new ValueAnimator(); a.setFloatValues(v); return a; }
    public static ValueAnimator ofPropertyValuesHolder(PropertyValuesHolder... v) { ValueAnimator a = new ValueAnimator(); a.setValues(v); return a; }
    public static ValueAnimator ofObject(TypeEvaluator e, Object... v) { ValueAnimator a = new ValueAnimator(); a.setObjectValues(v); a.setEvaluator(e); return a; }

    public void setIntValues(int... v) { if (mValues != null && mValues.length > 0) mValues[0].setIntValues(v); else setValues(PropertyValuesHolder.ofInt("", v)); }
    public void setFloatValues(float... v) { if (mValues != null && mValues.length > 0) mValues[0].setFloatValues(v); else setValues(PropertyValuesHolder.ofFloat("", v)); }
    public void setObjectValues(Object... v) { if (mValues != null && mValues.length > 0) mValues[0].setObjectValues(v); else setValues(PropertyValuesHolder.ofObject("", null, v)); }
    public void setValues(PropertyValuesHolder... v) { mValues = v; mValuesMap = new HashMap<>(); for (PropertyValuesHolder h : v) mValuesMap.put(h.getPropertyName(), h); }
    public PropertyValuesHolder[] getValues() { return mValues; }
    public void setEvaluator(TypeEvaluator e) { if (mValues != null && mValues.length > 0) mValues[0].setEvaluator(e); }
    @Override public ValueAnimator setDuration(long d) { if (d < 0) throw new IllegalArgumentException("Animators cannot have negative duration: " + d); mDuration = d; return this; }
    @Override public long getDuration() { return mDuration; }
    @Override public long getTotalDuration() { return mRepeatCount == INFINITE ? DURATION_INFINITE : mStartDelay + mDuration * (mRepeatCount + 1); }
    @Override public long getStartDelay() { return mStartDelay; }
    @Override public void setStartDelay(long d) { mStartDelay = Math.max(0, d); }
    @Override public void setInterpolator(TimeInterpolator i) { mInterpolator = i != null ? i : new android.view.animation.LinearInterpolator(); }
    @Override public TimeInterpolator getInterpolator() { return mInterpolator; }
    public void setRepeatCount(int n) { mRepeatCount = n; }
    public int getRepeatCount() { return mRepeatCount; }
    public void setRepeatMode(int m) { mRepeatMode = m; }
    public int getRepeatMode() { return mRepeatMode; }
    public void addUpdateListener(AnimatorUpdateListener l) { if (mUpdate == null) mUpdate = new ArrayList<>(); mUpdate.add(l); }
    public void removeUpdateListener(AnimatorUpdateListener l) { if (mUpdate != null) mUpdate.remove(l); }
    public void removeAllUpdateListeners() { mUpdate = null; }
    public Object getAnimatedValue() { return mValues != null && mValues.length > 0 ? mValues[0].getAnimatedValue() : null; }
    public Object getAnimatedValue(String name) { PropertyValuesHolder h = mValuesMap == null ? null : mValuesMap.get(name); return h == null ? null : h.getAnimatedValue(); }
    public float getAnimatedFraction() { return mFraction; }
    public long getCurrentPlayTime() { return mStartTime < 0 ? 0 : android.view.animation.AnimationUtils.currentAnimationTimeMillis() - mStartTime; }
    public void setCurrentPlayTime(long t) { float f = mDuration > 0 ? (float) t / mDuration : 1; setCurrentFraction(f); }
    public void setCurrentFraction(float f) {
        initAnimation();
        long now = android.view.animation.AnimationUtils.currentAnimationTimeMillis();
        long d = scaled();
        if (mRunning) mStartTime = now - (long) (f * d) - mStartDelay; else mSeekTime = (long) (f * d);
        animateValue(clampFraction(f));
    }
    private long scaled() { return (long) (mDuration * sDurationScale); }
    @Override public boolean isRunning() { return mRunning; }
    @Override public boolean isStarted() { return mStarted; }
    @Override public boolean canReverse() { return true; }

    boolean mInitialized;
    void initAnimation() { if (!mInitialized) { if (mValues != null) for (PropertyValuesHolder h : mValues) h.calculate(0); mInitialized = true; } }

    @Override public void start() { start(false); }
    private void start(boolean reverse) {
        if (android.os.Looper.myLooper() == null) throw new android.util.AndroidRuntimeException("Animators may only be run on Looper threads");
        mReversing = reverse;
        mStarted = true; mPaused = false; mRunning = false; mIteration = 0; mStartListenersCalled = false;
        mStartTime = -1;
        Handler.get().add(this);
        if (mStartDelay == 0 || mSeekTime >= 0) {
            startNow(android.view.animation.AnimationUtils.currentAnimationTimeMillis());
            setCurrentPlayTimeInternal();
        }
    }
    private void setCurrentPlayTimeInternal() {
        long d = scaled();
        if (mSeekTime >= 0) { animateValue(clampFraction(d == 0 ? 1 : (float) mSeekTime / d)); mSeekTime = -1; }
        else animateValue(mReversing ? 1 : 0);
    }
    private void startNow(long now) {
        mRunning = true;
        mStartTime = now - (mSeekTime >= 0 ? mSeekTime : 0);
        initAnimation();
        if (!mStartListenersCalled) { mStartListenersCalled = true; for (AnimatorListener l : listeners()) l.onAnimationStart(this, mReversing); }
    }
    private float clampFraction(float f) { if (f < 0) return 0; if (mRepeatCount != INFINITE) f = Math.min(f, mRepeatCount + 1); return f; }
    @Override public void cancel() {
        if (!mStarted) return;
        Handler.get().remove(this);
        if (!mStartListenersCalled) for (AnimatorListener l : listeners()) l.onAnimationStart(this, mReversing);
        for (AnimatorListener l : listeners()) l.onAnimationCancel(this);
        finish();
    }
    @Override public void end() {
        if (!mStarted) { start(); }
        else if (!mRunning) startNow(android.view.animation.AnimationUtils.currentAnimationTimeMillis());
        animateValue(shouldPlayBackward(mRepeatCount) ? 0 : 1);
        Handler.get().remove(this);
        finish();
    }
    private void finish() {
        boolean notify = mStarted;
        mRunning = false; mStarted = false; mPaused = false; mStartTime = -1;
        if (notify) { if (!mStartListenersCalled) for (AnimatorListener l : listeners()) l.onAnimationStart(this, mReversing); for (AnimatorListener l : listeners()) l.onAnimationEnd(this, mReversing); }
        mReversing = false; mStartListenersCalled = false;
    }
    @Override public void pause() { if (mStarted && !mPaused) mPauseTime = android.view.animation.AnimationUtils.currentAnimationTimeMillis(); super.pause(); }
    @Override public void resume() { if (mPaused && mStartTime >= 0) mStartTime += android.view.animation.AnimationUtils.currentAnimationTimeMillis() - mPauseTime; super.resume(); }
    @Override public void reverse() {
        if (mRunning) { long now = android.view.animation.AnimationUtils.currentAnimationTimeMillis(), played = now - mStartTime; mStartTime = now - (scaled() - played); mReversing = !mReversing; }
        else if (mStarted) { mReversing = !mReversing; end(); }
        else start(true);
    }
    private boolean shouldPlayBackward(int iteration) {
        if (iteration > 0 && mRepeatMode == REVERSE && (iteration < mRepeatCount + 1 || mRepeatCount == INFINITE)) return mReversing ? iteration % 2 == 0 : iteration % 2 != 0;
        return mReversing;
    }
    /** One frame; true when it is done. */
    boolean doFrame(long now) {
        if (mPaused) return false;
        if (!mRunning) {
            if (mStartTime < 0) mStartTime = now + (long) (mStartDelay * sDurationScale);
            if (now < mStartTime) return false;
            mStartTime = -1;
            startNow(now - 0);
            mStartTime = now;
        }
        long d = scaled();
        float f = d > 0 ? (float) (now - mStartTime) / d : 1;
        boolean done = false;
        int it = (int) f;
        if (f >= 1) {
            if (it <= mRepeatCount || mRepeatCount == INFINITE) {
                if (it != mIteration) { mIteration = it; for (AnimatorListener l : listeners()) l.onAnimationRepeat(this); }
            } else { done = true; f = Math.min(f, mRepeatCount + 1); }
        }
        animateValue(clampFraction(f));
        return done;
    }
    void animateValue(float overall) {
        mOverallFraction = overall;
        int it = (int) overall; float local = overall - it;
        if (overall >= mRepeatCount + 1 && mRepeatCount != INFINITE) { it = mRepeatCount; local = 1; }
        if (shouldPlayBackward(it)) local = 1 - local;
        float f = mInterpolator.getInterpolation(local);
        mFraction = f;
        if (mValues != null) for (PropertyValuesHolder h : mValues) h.calculate(f);
        if (mUpdate != null) for (AnimatorUpdateListener l : new ArrayList<>(mUpdate)) l.onAnimationUpdate(this);
    }
    void frameDone() { finish(); }

    @Override public ValueAnimator clone() {
        ValueAnimator a = (ValueAnimator) super.clone();
        if (mUpdate != null) a.mUpdate = new ArrayList<>(mUpdate);
        if (mValues != null) { PropertyValuesHolder[] v = new PropertyValuesHolder[mValues.length]; for (int i = 0; i < v.length; i++) v[i] = mValues[i].clone(); a.setValues(v); }
        a.mRunning = false; a.mStarted = false; a.mStartTime = -1; a.mInitialized = false;
        return a;
    }
    @Override public String toString() { return "ValueAnimator@" + Integer.toHexString(hashCode()); }

    /** The per-thread driver. */
    static final class Handler implements Choreographer.FrameCallback {
        private static final ThreadLocal<Handler> sHandler = new ThreadLocal<>();
        private final ArrayList<ValueAnimator> mAnims = new ArrayList<>();
        private boolean mPosted;
        static Handler get() { Handler h = sHandler.get(); if (h == null) { h = new Handler(); sHandler.set(h); } return h; }
        void add(ValueAnimator a) { if (!mAnims.contains(a)) mAnims.add(a); post(); }
        void remove(ValueAnimator a) { mAnims.remove(a); }
        private void post() { if (!mPosted) { mPosted = true; Choreographer.getInstance().postFrameCallback(this); } }
        public void doFrame(long nanos) {
            mPosted = false;
            long now = android.view.animation.AnimationUtils.currentAnimationTimeMillis();
            for (ValueAnimator a : new ArrayList<>(mAnims)) {
                if (!mAnims.contains(a)) continue;
                boolean done;
                try { done = a.doFrame(now); } catch (RuntimeException e) { mAnims.remove(a); throw e; }
                if (done) { mAnims.remove(a); a.frameDone(); }
            }
            if (!mAnims.isEmpty()) post();
        }
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public static int getCurrentAnimationsCount() { return 0; }
    public void commitAnimationFrame(long p0) {}
    public boolean doAnimationFrame(long p0) { return false; }
    public void overrideDurationScale(float p0) {}
    public void setAllowRunningAsynchronously(boolean p0) {}
    // ---- end of generated members
}
