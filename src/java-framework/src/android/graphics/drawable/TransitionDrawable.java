package android.graphics.drawable;

import android.graphics.Canvas;

public class TransitionDrawable extends LayerDrawable {
    private long mStart; private int mDuration; private boolean mReverse, mCross;
    public TransitionDrawable(Drawable[] layers) { super(layers); }
    public void startTransition(int ms) { mStart = android.os.SystemClock.uptimeMillis(); mDuration = ms; mReverse = false; invalidateSelf(); }
    public void reverseTransition(int ms) { mStart = android.os.SystemClock.uptimeMillis(); mDuration = ms; mReverse = !mReverse; invalidateSelf(); }
    public void resetTransition() { mStart = 0; mReverse = false; invalidateSelf(); }
    public void setCrossFadeEnabled(boolean e) { mCross = e; }
    public boolean isCrossFadeEnabled() { return mCross; }
    @Override public void draw(Canvas c) {
        float t = mStart == 0 ? 0 : Math.min(1, (android.os.SystemClock.uptimeMillis() - mStart) / (float) Math.max(1, mDuration));
        if (mReverse) t = 1 - t;
        Drawable a = getDrawable(0), b = getNumberOfLayers() > 1 ? getDrawable(1) : null;
        if (a != null) { if (mCross) a.setAlpha((int) (255 * (1 - t))); a.draw(c); }
        if (b != null && t > 0) { b.setAlpha((int) (255 * t)); b.draw(c); }
        if (mStart != 0 && android.os.SystemClock.uptimeMillis() - mStart < mDuration) invalidateSelf();
    }
}
