package android.animation;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;

/** Children are started together or after the ones they follow end; durations and interpolators set here pass to them. */
public final class AnimatorSet extends Animator {
    private final ArrayList<Animator> mNodes = new ArrayList<>();
    private final HashMap<Animator, ArrayList<Animator>> mAfter = new HashMap<>();   /* a -> the animators that start when a ends */
    private final HashMap<Animator, ArrayList<Animator>> mWith = new HashMap<>();
    private long mDuration = -1, mStartDelay;
    private TimeInterpolator mInterpolator;
    private boolean mStarted, mRunning;
    private int mRemaining;
    private final android.os.Handler mHandler = new android.os.Handler();

    public AnimatorSet() {}
    private void node(Animator a) { if (!mNodes.contains(a)) mNodes.add(a); }
    public void playTogether(Animator... items) { for (Animator a : items) if (a != null) node(a); }
    public void playTogether(Collection<Animator> items) { for (Animator a : items) if (a != null) node(a); }
    public void playSequentially(Animator... items) { for (int i = 0; i < items.length; i++) { node(items[i]); if (i > 0) link(items[i - 1], items[i]); } }
    public void playSequentially(List<Animator> items) { playSequentially(items.toArray(new Animator[0])); }
    private void link(Animator first, Animator then) { ArrayList<Animator> l = mAfter.get(first); if (l == null) mAfter.put(first, l = new ArrayList<>()); if (!l.contains(then)) l.add(then); }
    public ArrayList<Animator> getChildAnimations() { return new ArrayList<>(mNodes); }
    public Builder play(Animator a) { node(a); return new Builder(a); }
    public final class Builder {
        private final Animator mCurrent;
        Builder(Animator a) { mCurrent = a; }
        public Builder with(Animator a) { node(a); ArrayList<Animator> deps = deps(mCurrent); for (Animator d : deps) link(d, a); ArrayList<Animator> l = mWith.get(mCurrent); if (l == null) mWith.put(mCurrent, l = new ArrayList<>()); l.add(a); return this; }
        public Builder before(Animator a) { node(a); link(mCurrent, a); return this; }
        public Builder after(Animator a) { node(a); link(a, mCurrent); return this; }
        public Builder after(long delay) { ValueAnimator d = ValueAnimator.ofFloat(0, 1); d.setDuration(delay); return after(d); }
    }
    private ArrayList<Animator> deps(Animator a) { ArrayList<Animator> r = new ArrayList<>(); for (java.util.Map.Entry<Animator, ArrayList<Animator>> e : mAfter.entrySet()) if (e.getValue().contains(a)) r.add(e.getKey()); return r; }

    @Override public long getStartDelay() { return mStartDelay; }
    @Override public void setStartDelay(long d) { mStartDelay = d; }
    @Override public AnimatorSet setDuration(long d) { mDuration = d; return this; }
    @Override public long getDuration() { return mDuration; }
    @Override public void setInterpolator(TimeInterpolator i) { mInterpolator = i; }
    @Override public TimeInterpolator getInterpolator() { return mInterpolator; }
    @Override public boolean isRunning() { return mRunning; }
    @Override public boolean isStarted() { return mStarted; }
    @Override public void setTarget(Object t) { for (Animator a : mNodes) if (a instanceof AnimatorSet || a instanceof ObjectAnimator) a.setTarget(t); }
    @Override public long getTotalDuration() { return mStartDelay + 1000; }

    private final AnimatorListener mChildListener = new AnimatorListenerAdapter() {
        @Override public void onAnimationEnd(Animator a) {
            a.removeListener(this);
            if (!mStarted) return;
            ArrayList<Animator> next = mAfter.get(a);
            if (next != null) for (Animator n : next) if (allDepsDone(n)) startChild(n);
            mDone.add(a);
            if (--mRemaining == 0) finish(false);
        }
    };
    private final ArrayList<Animator> mDone = new ArrayList<>();
    private boolean allDepsDone(Animator n) { for (Animator d : deps(n)) if (!mDone.contains(d) && d != null && d.isStarted()) return false; return true; }
    private void startChild(Animator a) {
        if (mDuration >= 0) a.setDuration(mDuration);
        if (mInterpolator != null) a.setInterpolator(mInterpolator);
        a.addListener(mChildListener);
        a.start();
    }
    @Override public void start() {
        if (mStarted) cancel();
        mStarted = true; mRunning = mStartDelay == 0; mDone.clear();
        mRemaining = mNodes.size();
        for (AnimatorListener l : listeners()) l.onAnimationStart(this, false);
        Runnable go = () -> {
            mRunning = true;
            if (mNodes.isEmpty()) { finish(false); return; }
            for (Animator a : new ArrayList<>(mNodes)) if (deps(a).isEmpty()) startChild(a);
        };
        if (mStartDelay > 0) mHandler.postDelayed(go, mStartDelay); else go.run();
    }
    private void finish(boolean cancelled) {
        if (!mStarted) return;
        mStarted = false; mRunning = false;
        if (cancelled) for (AnimatorListener l : listeners()) l.onAnimationCancel(this);
        for (AnimatorListener l : listeners()) l.onAnimationEnd(this, false);
    }
    @Override public void cancel() {
        if (!mStarted) return;
        boolean was = mStarted;
        mStarted = false;
        for (Animator a : mNodes) { a.removeListener(mChildListener); if (a.isStarted()) a.cancel(); }
        mStarted = was;
        finish(true);
    }
    @Override public void end() {
        if (!mStarted) start();
        mStarted = false;
        for (Animator a : mNodes) { a.removeListener(mChildListener); a.end(); }
        mStarted = true;
        finish(false);
    }
    @Override public boolean canReverse() { return false; }
    @Override public AnimatorSet clone() {
        AnimatorSet s = (AnimatorSet) super.clone();
        return s;
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
    public void commitAnimationFrame(long p0) {}
    public boolean doAnimationFrame(long p0) { return false; }
    public int getChangingConfigurations() { return 0; }
    public long getCurrentPlayTime() { return (huskFill.get("CurrentPlayTime") instanceof Long ? (Long) huskFill.get("CurrentPlayTime") : 0L); }
    public void setCurrentPlayTime(long p0) { huskFill.put("CurrentPlayTime", Long.valueOf(p0)); }
    public boolean shouldPlayTogether() { return false; }
    // ---- end of generated members
}
