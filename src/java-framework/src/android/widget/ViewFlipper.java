package android.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;

public class ViewFlipper extends ViewAnimator {
    private int mFlipInterval = 3000;
    private boolean mAutoStart, mRunning, mStarted, mVisible, mUserPresent = true;
    public ViewFlipper(Context c) { super(c); }
    public ViewFlipper(Context c, AttributeSet attrs) {
        super(c, attrs);
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.ViewFlipper);
        mFlipInterval = a.getInt(husk.S.ViewFlipper_flipInterval, 3000);
        mAutoStart = a.getBoolean(husk.S.ViewFlipper_autoStart, false);
        a.recycle();
    }
    @Override protected void onAttachedToWindow() { super.onAttachedToWindow(); if (mAutoStart) startFlipping(); }
    @Override protected void onDetachedFromWindow() { super.onDetachedFromWindow(); mVisible = false; updateRunning(); }
    @Override protected void onWindowVisibilityChanged(int v) { super.onWindowVisibilityChanged(v); mVisible = v == VISIBLE; updateRunning(false); }
    public void setFlipInterval(int ms) { mFlipInterval = ms; }
    public int getFlipInterval() { return mFlipInterval; }
    public void startFlipping() { mStarted = true; updateRunning(); }
    public void stopFlipping() { mStarted = false; updateRunning(); }
    private void updateRunning() { updateRunning(true); }
    private void updateRunning(boolean flipNow) {
        boolean running = mVisible && mStarted && mUserPresent;
        if (running != mRunning) {
            if (running) { showOnly(mWhichChild, flipNow); postDelayed(mFlipRunnable, mFlipInterval); }
            else removeCallbacks(mFlipRunnable);
            mRunning = running;
        }
    }
    public boolean isFlipping() { return mStarted; }
    public void setAutoStart(boolean a) { mAutoStart = a; }
    public boolean isAutoStart() { return mAutoStart; }
    private final Runnable mFlipRunnable = new Runnable() { @Override public void run() { if (mRunning) { showNext(); postDelayed(mFlipRunnable, mFlipInterval); } } };
}
