package android.view;

import android.animation.Animator;
import android.animation.TimeInterpolator;
import android.animation.ValueAnimator;
import java.util.ArrayList;

/** view.animate(): the properties asked for, animated together by one ValueAnimator started on the next frame. */
public class ViewPropertyAnimator {
    private static final int TX = 0, TY = 1, TZ = 2, SX = 3, SY = 4, R = 5, RX = 6, RY = 7, X = 8, Y = 9, Z = 10, A = 11;
    private final View mView;
    private long mDuration = -1, mStartDelay;
    private TimeInterpolator mInterpolator;
    private Animator.AnimatorListener mListener;
    private ValueAnimator.AnimatorUpdateListener mUpdateListener;
    private final ArrayList<float[]> mPending = new ArrayList<>();   /* {property, to, by?} */
    private Runnable mWithStart, mWithEnd;
    private ValueAnimator mRunning;
    private boolean mScheduled;
    ViewPropertyAnimator(View v) { mView = v; }
    public ViewPropertyAnimator setDuration(long d) { if (d < 0) throw new IllegalArgumentException("Animators cannot have negative duration: " + d); mDuration = d; return this; }
    public long getDuration() { return mDuration >= 0 ? mDuration : 300; }
    public long getStartDelay() { return mStartDelay; }
    public ViewPropertyAnimator setStartDelay(long d) { mStartDelay = d; return this; }
    public ViewPropertyAnimator setInterpolator(TimeInterpolator i) { mInterpolator = i; return this; }
    public TimeInterpolator getInterpolator() { return mInterpolator; }
    public ViewPropertyAnimator setListener(Animator.AnimatorListener l) { mListener = l; return this; }
    public ViewPropertyAnimator setUpdateListener(ValueAnimator.AnimatorUpdateListener l) { mUpdateListener = l; return this; }
    public ViewPropertyAnimator withLayer() { return this; }
    public ViewPropertyAnimator withStartAction(Runnable r) { mWithStart = r; return this; }
    public ViewPropertyAnimator withEndAction(Runnable r) { mWithEnd = r; return this; }
    private ViewPropertyAnimator to(int p, float v) { mPending.add(new float[] { p, v, 0 }); schedule(); return this; }
    private ViewPropertyAnimator by(int p, float v) { mPending.add(new float[] { p, v, 1 }); schedule(); return this; }
    public ViewPropertyAnimator x(float v) { return to(X, v); } public ViewPropertyAnimator xBy(float v) { return by(X, v); }
    public ViewPropertyAnimator y(float v) { return to(Y, v); } public ViewPropertyAnimator yBy(float v) { return by(Y, v); }
    public ViewPropertyAnimator z(float v) { return to(Z, v); } public ViewPropertyAnimator zBy(float v) { return by(Z, v); }
    public ViewPropertyAnimator rotation(float v) { return to(R, v); } public ViewPropertyAnimator rotationBy(float v) { return by(R, v); }
    public ViewPropertyAnimator rotationX(float v) { return to(RX, v); } public ViewPropertyAnimator rotationXBy(float v) { return by(RX, v); }
    public ViewPropertyAnimator rotationY(float v) { return to(RY, v); } public ViewPropertyAnimator rotationYBy(float v) { return by(RY, v); }
    public ViewPropertyAnimator translationX(float v) { return to(TX, v); } public ViewPropertyAnimator translationXBy(float v) { return by(TX, v); }
    public ViewPropertyAnimator translationY(float v) { return to(TY, v); } public ViewPropertyAnimator translationYBy(float v) { return by(TY, v); }
    public ViewPropertyAnimator translationZ(float v) { return to(TZ, v); } public ViewPropertyAnimator translationZBy(float v) { return by(TZ, v); }
    public ViewPropertyAnimator scaleX(float v) { return to(SX, v); } public ViewPropertyAnimator scaleXBy(float v) { return by(SX, v); }
    public ViewPropertyAnimator scaleY(float v) { return to(SY, v); } public ViewPropertyAnimator scaleYBy(float v) { return by(SY, v); }
    public ViewPropertyAnimator alpha(float v) { return to(A, v); } public ViewPropertyAnimator alphaBy(float v) { return by(A, v); }
    private float get(int p) {
        switch (p) { case TX: return mView.getTranslationX(); case TY: return mView.getTranslationY(); case TZ: return mView.getTranslationZ(); case SX: return mView.getScaleX();
        case SY: return mView.getScaleY(); case R: return mView.getRotation(); case RX: return mView.getRotationX(); case RY: return mView.getRotationY();
        case X: return mView.getX(); case Y: return mView.getY(); case Z: return mView.getZ(); default: return mView.getAlpha(); }
    }
    private void set(int p, float v) {
        switch (p) { case TX: mView.setTranslationX(v); break; case TY: mView.setTranslationY(v); break; case TZ: mView.setTranslationZ(v); break; case SX: mView.setScaleX(v); break;
        case SY: mView.setScaleY(v); break; case R: mView.setRotation(v); break; case RX: mView.setRotationX(v); break; case RY: mView.setRotationY(v); break;
        case X: mView.setX(v); break; case Y: mView.setY(v); break; case Z: mView.setZ(v); break; default: mView.setAlpha(v); }
    }
    private final Runnable mStarter = this::startAnimation;
    private void schedule() { if (!mScheduled) { mScheduled = true; mView.postOnAnimation(mStarter); } }
    public void start() { if (mScheduled) { mView.removeCallbacks(mStarter); startAnimation(); } }
    public void cancel() { if (mScheduled) { mView.removeCallbacks(mStarter); mScheduled = false; mPending.clear(); } if (mRunning != null) mRunning.cancel(); }
    private void startAnimation() {
        mScheduled = false;
        if (mPending.isEmpty()) return;
        final ArrayList<float[]> props = new ArrayList<>(mPending);
        mPending.clear();
        final float[] from = new float[props.size()], delta = new float[props.size()];
        for (int i = 0; i < props.size(); i++) {
            float[] p = props.get(i);
            from[i] = get((int) p[0]);
            delta[i] = p[2] != 0 ? p[1] : p[1] - from[i];
        }
        if (mRunning != null && mRunning.isRunning()) mRunning.cancel();
        final ValueAnimator va = ValueAnimator.ofFloat(0f, 1f);
        if (mDuration >= 0) va.setDuration(mDuration);
        va.setStartDelay(mStartDelay);
        if (mInterpolator != null) va.setInterpolator(mInterpolator);
        final Runnable ws = mWithStart, we = mWithEnd;
        final Animator.AnimatorListener listener = mListener;
        final ValueAnimator.AnimatorUpdateListener ul = mUpdateListener;
        mWithStart = null; mWithEnd = null;
        va.addUpdateListener(a -> {
            float f = a.getAnimatedFraction();
            for (int i = 0; i < props.size(); i++) set((int) props.get(i)[0], from[i] + delta[i] * f);
            if (ul != null) ul.onAnimationUpdate(a);
        });
        va.addListener(new Animator.AnimatorListener() {
            public void onAnimationStart(Animator a) { if (ws != null) ws.run(); if (listener != null) listener.onAnimationStart(a); }
            public void onAnimationEnd(Animator a) { if (listener != null) listener.onAnimationEnd(a); if (we != null) we.run(); if (mRunning == va) mRunning = null; }
            public void onAnimationCancel(Animator a) { if (listener != null) listener.onAnimationCancel(a); }
            public void onAnimationRepeat(Animator a) { if (listener != null) listener.onAnimationRepeat(a); }
        });
        mRunning = va;
        va.start();
    }
}
