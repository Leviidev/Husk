package android.widget;

import android.content.Context;
import android.view.animation.AnimationUtils;
import android.view.animation.Interpolator;

/** Scroll and fling positions over time; flings decelerate along Android's spline, springs bring overscroll back. */
public class OverScroller {
    private final SplineOverScroller mX, mY;
    private Interpolator mInterpolator;
    private int mMode;
    private static final int SCROLL_MODE = 0, FLING_MODE = 1;
    public OverScroller(Context c) { this(c, null); }
    public OverScroller(Context c, Interpolator i) { this(c, i, true); }
    public OverScroller(Context c, Interpolator i, boolean flywheel) { mInterpolator = i; float ppi = c != null ? c.getResources().getDisplayMetrics().density * 160f : 320f; mX = new SplineOverScroller(ppi); mY = new SplineOverScroller(ppi); }
    public OverScroller(Context c, Interpolator i, float bx, float by) { this(c, i, true); }
    public OverScroller(Context c, Interpolator i, float bx, float by, boolean fw) { this(c, i, fw); }
    void setInterpolator(Interpolator i) { mInterpolator = i; }
    public final void setFriction(float f) { mX.mFlingFriction = f; mY.mFlingFriction = f; }
    public final boolean isFinished() { return mX.mFinished && mY.mFinished; }
    public final void forceFinished(boolean f) { mX.mFinished = mY.mFinished = f; }
    public final int getCurrX() { return mX.mCurrentPosition; }
    public final int getCurrY() { return mY.mCurrentPosition; }
    public float getCurrVelocity() { return (float) Math.hypot(mX.mCurrVelocity, mY.mCurrVelocity); }
    public final int getStartX() { return mX.mStart; }
    public final int getStartY() { return mY.mStart; }
    public final int getFinalX() { return mX.mFinal; }
    public final int getFinalY() { return mY.mFinal; }
    public final int getDuration() { return Math.max(mX.mDuration, mY.mDuration); }
    public void extendDuration(int e) { mX.mDuration += e; mY.mDuration += e; }
    public void setFinalX(int x) { mX.mFinal = x; mX.mDistance = x - mX.mStart; mX.mFinished = false; }
    public void setFinalY(int y) { mY.mFinal = y; mY.mDistance = y - mY.mStart; mY.mFinished = false; }
    public int timePassed() { long t = AnimationUtils.currentAnimationTimeMillis(); return (int) (t - Math.min(mX.mStartTime, mY.mStartTime)); }
    public boolean computeScrollOffset() {
        if (isFinished()) return false;
        if (mMode == SCROLL_MODE) {
            long elapsed = AnimationUtils.currentAnimationTimeMillis() - mX.mStartTime;
            int duration = mX.mDuration;
            if (elapsed < duration) {
                float q = (float) elapsed / duration;
                q = mInterpolator == null ? viscousFluid(q) : mInterpolator.getInterpolation(q);
                mX.updateScroll(q); mY.updateScroll(q);
            } else abortAnimation();
        } else {
            if (!mX.mFinished && !mX.update() && !mX.continueWhenFinished()) mX.finish();
            if (!mY.mFinished && !mY.update() && !mY.continueWhenFinished()) mY.finish();
        }
        return true;
    }
    public void startScroll(int sx, int sy, int dx, int dy) { startScroll(sx, sy, dx, dy, 250); }
    public void startScroll(int sx, int sy, int dx, int dy, int duration) { mMode = SCROLL_MODE; mX.startScroll(sx, dx, duration); mY.startScroll(sy, dy, duration); }
    public boolean springBack(int sx, int sy, int minX, int maxX, int minY, int maxY) { mMode = FLING_MODE; boolean a = mX.springback(sx, minX, maxX), b = mY.springback(sy, minY, maxY); return a || b; }
    public void fling(int sx, int sy, int vx, int vy, int minX, int maxX, int minY, int maxY) { fling(sx, sy, vx, vy, minX, maxX, minY, maxY, 0, 0); }
    public void fling(int sx, int sy, int vx, int vy, int minX, int maxX, int minY, int maxY, int ox, int oy) {
        mMode = FLING_MODE;
        mX.fling(sx, vx, minX, maxX, ox); mY.fling(sy, vy, minY, maxY, oy);
    }
    public void notifyHorizontalEdgeReached(int s, int f, int o) { mX.notifyEdgeReached(s, f, o); }
    public void notifyVerticalEdgeReached(int s, int f, int o) { mY.notifyEdgeReached(s, f, o); }
    public boolean isOverScrolled() { return (!mX.mFinished && mX.mState != SplineOverScroller.SPLINE) || (!mY.mFinished && mY.mState != SplineOverScroller.SPLINE); }
    public void abortAnimation() { mX.finish(); mY.finish(); }
    public boolean isScrollingInDirection(float xvel, float yvel) { int dx = mX.mFinal - mX.mStart, dy = mY.mFinal - mY.mStart; return !isFinished() && Math.signum(xvel) == Math.signum(dx) && Math.signum(yvel) == Math.signum(dy); }

    private static final float VISCOUS_FLUID_SCALE = 8.0f, VISCOUS_FLUID_NORMALIZE, VISCOUS_FLUID_OFFSET;
    static { VISCOUS_FLUID_NORMALIZE = 1.0f / viscousFluidRaw(1.0f, 1.0f); VISCOUS_FLUID_OFFSET = 1.0f - VISCOUS_FLUID_NORMALIZE * viscousFluidRaw(1.0f, 1.0f); }
    private static float viscousFluidRaw(float x, float norm) { x *= VISCOUS_FLUID_SCALE; if (x < 1.0f) x -= 1.0f - (float) Math.exp(-x); else { float start = 0.36787944117f; x = 1.0f - (float) Math.exp(1.0f - x); x = start + x * (1.0f - start); } return x; }
    static float viscousFluid(float x) { float i = VISCOUS_FLUID_NORMALIZE * viscousFluidRaw(x, 1); return i > 0 ? i + VISCOUS_FLUID_OFFSET : i; }

    static class SplineOverScroller {
        int mStart, mCurrentPosition, mFinal, mVelocity, mOver, mDuration, mSplineDuration, mSplineDistance, mState = SPLINE;
        float mCurrVelocity, mDeceleration, mFlingFriction = android.view.ViewConfiguration.getScrollFriction();
        long mStartTime;
        int mDistance;
        boolean mFinished = true;
        private final float mPhysicalCoeff;
        static final int SPLINE = 0, CUBIC = 1, BALLISTIC = 2;
        private static final float GRAVITY = 2000.0f;
        private static final float DECELERATION_RATE = (float) (Math.log(0.78) / Math.log(0.9));
        private static final float INFLEXION = 0.35f, START_TENSION = 0.5f, END_TENSION = 1.0f, P1 = START_TENSION * INFLEXION, P2 = 1.0f - END_TENSION * (1.0f - INFLEXION);
        private static final int NB_SAMPLES = 100;
        private static final float[] SPLINE_POSITION = new float[NB_SAMPLES + 1], SPLINE_TIME = new float[NB_SAMPLES + 1];
        static {
            float x_min = 0.0f, y_min = 0.0f;
            for (int i = 0; i < NB_SAMPLES; i++) {
                final float alpha = (float) i / NB_SAMPLES;
                float x_max = 1.0f, x, tx, coef;
                while (true) { x = x_min + (x_max - x_min) / 2.0f; coef = 3.0f * x * (1.0f - x); tx = coef * ((1.0f - x) * P1 + x * P2) + x * x * x; if (Math.abs(tx - alpha) < 1E-5) break; if (tx > alpha) x_max = x; else x_min = x; }
                SPLINE_POSITION[i] = coef * ((1.0f - x) * START_TENSION + x) + x * x * x;
                float y_max = 1.0f, y, dy;
                while (true) { y = y_min + (y_max - y_min) / 2.0f; coef = 3.0f * y * (1.0f - y); dy = coef * ((1.0f - y) * START_TENSION + y) + y * y * y; if (Math.abs(dy - alpha) < 1E-5) break; if (dy > alpha) y_max = y; else y_min = y; }
                SPLINE_TIME[i] = coef * ((1.0f - y) * P1 + y * P2) + y * y * y;
            }
            SPLINE_POSITION[NB_SAMPLES] = SPLINE_TIME[NB_SAMPLES] = 1.0f;
        }
        SplineOverScroller(float ppi) { mPhysicalCoeff = 9.80665f * 39.37f * ppi * 0.84f; }
        void updateScroll(float q) { mCurrentPosition = mStart + Math.round(q * (mFinal - mStart)); }
        private static float getDeceleration(int v) { return v > 0 ? -GRAVITY : GRAVITY; }
        private void adjustDuration(int start, int oldFinal, int newFinal) {
            int oldDistance = oldFinal - start, newDistance = newFinal - start;
            float x = Math.abs((float) newDistance / oldDistance);
            int index = (int) (NB_SAMPLES * x);
            if (index < NB_SAMPLES) { float x_inf = (float) index / NB_SAMPLES, x_sup = (float) (index + 1) / NB_SAMPLES, t_inf = SPLINE_TIME[index], t_sup = SPLINE_TIME[index + 1]; float timeCoef = t_inf + (x - x_inf) / (x_sup - x_inf) * (t_sup - t_inf); mDuration *= timeCoef; }
        }
        void startScroll(int start, int distance, int duration) { mFinished = false; mCurrentPosition = mStart = start; mFinal = start + distance; mDistance = distance; mStartTime = AnimationUtils.currentAnimationTimeMillis(); mDuration = duration; mDeceleration = 0.0f; mVelocity = 0; }
        void finish() { mCurrentPosition = mFinal; mFinished = true; }
        boolean springback(int start, int min, int max) {
            mFinished = true; mCurrentPosition = mStart = mFinal = start; mVelocity = 0; mStartTime = AnimationUtils.currentAnimationTimeMillis(); mDuration = 0;
            if (start < min) startSpringback(start, min, 0); else if (start > max) startSpringback(start, max, 0);
            return !mFinished;
        }
        private void startSpringback(int start, int end, int velocity) {
            mFinished = false; mState = CUBIC; mCurrentPosition = mStart = start; mFinal = end;
            final int delta = start - end;
            mDeceleration = getDeceleration(delta);
            mVelocity = -delta;
            mOver = Math.abs(delta);
            mDuration = (int) (1000.0 * Math.sqrt(-2.0 * delta / mDeceleration));
        }
        void fling(int start, int velocity, int min, int max, int over) {
            mOver = over; mFinished = false; mCurrVelocity = mVelocity = velocity; mDuration = mSplineDuration = 0;
            mStartTime = AnimationUtils.currentAnimationTimeMillis();
            mCurrentPosition = mStart = start;
            if (start > max || start < min) { startAfterEdge(start, min, max, velocity); return; }
            mState = SPLINE;
            double totalDistance = 0.0;
            if (velocity != 0) { mDuration = mSplineDuration = getSplineFlingDuration(velocity); totalDistance = getSplineFlingDistance(velocity); }
            mSplineDistance = (int) (totalDistance * Math.signum(velocity));
            mFinal = start + mSplineDistance;
            if (mFinal < min) { adjustDuration(mStart, mFinal, min); mFinal = min; }
            if (mFinal > max) { adjustDuration(mStart, mFinal, max); mFinal = max; }
        }
        private double getSplineDeceleration(int velocity) { return Math.log(INFLEXION * Math.abs(velocity) / (mFlingFriction * mPhysicalCoeff)); }
        private double getSplineFlingDistance(int velocity) { final double l = getSplineDeceleration(velocity); final double decelMinusOne = DECELERATION_RATE - 1.0; return mFlingFriction * mPhysicalCoeff * Math.exp(DECELERATION_RATE / decelMinusOne * l); }
        private int getSplineFlingDuration(int velocity) { final double l = getSplineDeceleration(velocity); final double decelMinusOne = DECELERATION_RATE - 1.0; return (int) (1000.0 * Math.exp(l / decelMinusOne)); }
        private void fitOnBounceCurve(int start, int end, int velocity) {
            final float durationToApex = -velocity / mDeceleration;
            final float velocitySquared = (float) velocity * velocity;
            final float distanceToApex = velocitySquared / 2.0f / Math.abs(mDeceleration);
            final float distanceToEdge = Math.abs(end - start);
            final float totalDuration = (float) Math.sqrt(2.0 * (distanceToApex + distanceToEdge) / Math.abs(mDeceleration));
            mStartTime -= (int) (1000.0f * (totalDuration - durationToApex));
            mCurrentPosition = mStart = end;
            mVelocity = (int) (-mDeceleration * totalDuration);
        }
        private void startBounceAfterEdge(int start, int end, int velocity) { mDeceleration = getDeceleration(velocity == 0 ? start - end : velocity); fitOnBounceCurve(start, end, velocity); onEdgeReached(); }
        private void startAfterEdge(int start, int min, int max, int velocity) {
            if (start > min && start < max) { mFinished = true; return; }
            final boolean positive = start > max;
            final int edge = positive ? max : min;
            final int overDistance = start - edge;
            boolean keepIncreasing = overDistance * velocity >= 0;
            if (keepIncreasing) startBounceAfterEdge(start, edge, velocity);
            else {
                final double totalDistance = getSplineFlingDistance(velocity);
                if (totalDistance > Math.abs(overDistance)) fling(start, velocity, positive ? min : start, positive ? start : max, mOver);
                else startSpringback(start, edge, velocity);
            }
        }
        void notifyEdgeReached(int start, int end, int over) { if (mState == SPLINE) { mOver = over; mStartTime = AnimationUtils.currentAnimationTimeMillis(); startAfterEdge(start, end, end, (int) mCurrVelocity); } }
        private void onEdgeReached() {
            final float velocitySquared = (float) mVelocity * mVelocity;
            float distance = velocitySquared / (2.0f * Math.abs(mDeceleration));
            final float sign = Math.signum(mVelocity);
            if (distance > mOver) { mDeceleration = -sign * velocitySquared / (2.0f * mOver); distance = mOver; }
            mOver = (int) distance;
            mState = BALLISTIC;
            mFinal = mStart + (int) (mVelocity > 0 ? distance : -distance);
            mDuration = -(int) (1000.0f * mVelocity / mDeceleration);
        }
        boolean continueWhenFinished() {
            switch (mState) {
            case SPLINE:
                if (mDuration < mSplineDuration) { mCurrentPosition = mStart = mFinal; mVelocity = (int) mCurrVelocity; mDeceleration = getDeceleration(mVelocity); mStartTime += mDuration; onEdgeReached(); } else return false;
                break;
            case BALLISTIC: mStartTime += mDuration; startSpringback(mFinal, mStart, 0); break;
            case CUBIC: return false;
            }
            update();
            return true;
        }
        boolean update() {
            final long time = AnimationUtils.currentAnimationTimeMillis();
            final long currentTime = time - mStartTime;
            if (currentTime == 0) return mDuration > 0;
            if (currentTime > mDuration) return false;
            double distance = 0.0;
            switch (mState) {
            case SPLINE: {
                final float t = (float) currentTime / mSplineDuration;
                final int index = (int) (NB_SAMPLES * t);
                float distanceCoef = 1.f, velocityCoef = 0.f;
                if (index < NB_SAMPLES) { final float t_inf = (float) index / NB_SAMPLES, t_sup = (float) (index + 1) / NB_SAMPLES, d_inf = SPLINE_POSITION[index], d_sup = SPLINE_POSITION[index + 1]; velocityCoef = (d_sup - d_inf) / (t_sup - t_inf); distanceCoef = d_inf + (t - t_inf) * velocityCoef; }
                distance = distanceCoef * mSplineDistance;
                mCurrVelocity = velocityCoef * mSplineDistance / mSplineDuration * 1000.0f;
                break;
            }
            case BALLISTIC: { final float t = currentTime / 1000.0f; mCurrVelocity = mVelocity + mDeceleration * t; distance = mVelocity * t + mDeceleration * t * t / 2.0f; break; }
            case CUBIC: { final float t = (float) currentTime / mDuration, t2 = t * t, sign = Math.signum(mVelocity); distance = sign * mOver * (3.0f * t2 - 2.0f * t * t2); mCurrVelocity = sign * mOver * 6.0f * (-t + t2); break; }
            }
            mCurrentPosition = mStart + (int) Math.round(distance);
            return true;
        }
    }
}
