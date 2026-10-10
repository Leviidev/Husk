package android.animation;
public class TimeAnimator extends ValueAnimator {
    public interface TimeListener { void onTimeUpdate(TimeAnimator a, long total, long delta); }
    private TimeListener mListener; private long mStart = -1, mPrev = -1;
    public TimeAnimator() { setFloatValues(0f, 1f); setDuration(Long.MAX_VALUE / 4); setInterpolator(null); }
    public void setTimeListener(TimeListener l) { mListener = l; }
    @Override void animateValue(float f) {
        long now = android.view.animation.AnimationUtils.currentAnimationTimeMillis();
        if (mStart < 0) { mStart = now; mPrev = now; }
        if (mListener != null) mListener.onTimeUpdate(this, now - mStart, now - mPrev);
        mPrev = now;
    }
    @Override public void start() { mStart = -1; super.start(); }
}
