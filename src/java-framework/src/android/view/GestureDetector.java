package android.view;

import android.content.Context;
import android.os.Handler;
import android.os.Message;

public class GestureDetector {
    public interface OnGestureListener {
        boolean onDown(MotionEvent e); void onShowPress(MotionEvent e); boolean onSingleTapUp(MotionEvent e);
        boolean onScroll(MotionEvent e1, MotionEvent e2, float dx, float dy); void onLongPress(MotionEvent e); boolean onFling(MotionEvent e1, MotionEvent e2, float vx, float vy);
    }
    public interface OnDoubleTapListener { boolean onSingleTapConfirmed(MotionEvent e); boolean onDoubleTap(MotionEvent e); boolean onDoubleTapEvent(MotionEvent e); }
    public interface OnContextClickListener { boolean onContextClick(MotionEvent e); }
    public static class SimpleOnGestureListener implements OnGestureListener, OnDoubleTapListener, OnContextClickListener {
        public boolean onSingleTapUp(MotionEvent e) { return false; } public void onLongPress(MotionEvent e) {}
        public boolean onScroll(MotionEvent e1, MotionEvent e2, float dx, float dy) { return false; }
        public boolean onFling(MotionEvent e1, MotionEvent e2, float vx, float vy) { return false; }
        public void onShowPress(MotionEvent e) {} public boolean onDown(MotionEvent e) { return false; }
        public boolean onDoubleTap(MotionEvent e) { return false; } public boolean onDoubleTapEvent(MotionEvent e) { return false; }
        public boolean onSingleTapConfirmed(MotionEvent e) { return false; } public boolean onContextClick(MotionEvent e) { return false; }
    }
    private static final int SHOW_PRESS = 1, LONG_PRESS = 2, TAP = 3;
    private final OnGestureListener mListener;
    private OnDoubleTapListener mDoubleTap;
    private final Handler mHandler;
    private final int mTouchSlopSq, mDoubleTapSlopSq, mMinFling, mMaxFling;
    private boolean mStillDown, mDeferConfirm, mInLongPress, mAlwaysInTapRegion, mAlwaysInBiggerTapRegion, mIsDoubleTapping, mLongpressEnabled = true;
    private MotionEvent mCurrentDown, mPreviousUp;
    private float mLastX, mLastY, mDownX, mDownY;
    private VelocityTracker mVelocity;

    public GestureDetector(Context c, OnGestureListener l) { this(c, l, null); }
    public GestureDetector(OnGestureListener l) { this(null, l, null); }
    public GestureDetector(OnGestureListener l, Handler h) { this(null, l, h); }
    public GestureDetector(Context c, OnGestureListener l, Handler h) { this(c, l, h, false); }
    public GestureDetector(Context c, OnGestureListener l, Handler h, boolean unused) {
        mListener = l;
        if (l instanceof OnDoubleTapListener) mDoubleTap = (OnDoubleTapListener) l;
        mHandler = new Handler(h != null ? h.getLooper() : android.os.Looper.myLooper() != null ? android.os.Looper.myLooper() : android.os.Looper.getMainLooper()) {
            @Override public void handleMessage(Message m) {
                if (m.what == SHOW_PRESS) mListener.onShowPress(mCurrentDown);
                else if (m.what == LONG_PRESS) { mDeferConfirm = false; mInLongPress = true; mListener.onLongPress(mCurrentDown); }
                else if (m.what == TAP && mDoubleTap != null) { if (!mStillDown) mDoubleTap.onSingleTapConfirmed(mCurrentDown); else mDeferConfirm = true; }
            }
        };
        ViewConfiguration vc = ViewConfiguration.get(c);
        int s = vc.getScaledTouchSlop(), ds = vc.getScaledDoubleTapSlop();
        mTouchSlopSq = s * s; mDoubleTapSlopSq = ds * ds; mMinFling = vc.getScaledMinimumFlingVelocity(); mMaxFling = vc.getScaledMaximumFlingVelocity();
    }
    public void setOnDoubleTapListener(OnDoubleTapListener l) { mDoubleTap = l; }
    public void setContextClickListener(OnContextClickListener l) {}
    public void setIsLongpressEnabled(boolean e) { mLongpressEnabled = e; }
    public boolean isLongpressEnabled() { return mLongpressEnabled; }

    public boolean onTouchEvent(MotionEvent ev) {
        int a = ev.getActionMasked();
        if (mVelocity == null) mVelocity = VelocityTracker.obtain();
        mVelocity.addMovement(ev);
        boolean up = a == MotionEvent.ACTION_POINTER_UP;
        int skip = up ? ev.getActionIndex() : -1;
        float sx = 0, sy = 0; int n = 0;
        for (int i = 0; i < ev.getPointerCount(); i++) { if (i == skip) continue; sx += ev.getX(i); sy += ev.getY(i); n++; }
        float fx = n > 0 ? sx / n : ev.getX(), fy = n > 0 ? sy / n : ev.getY();
        boolean handled = false;
        switch (a) {
        case MotionEvent.ACTION_POINTER_DOWN: case MotionEvent.ACTION_POINTER_UP:
            mDownX = mLastX = fx; mDownY = mLastY = fy; cancelTaps(); break;
        case MotionEvent.ACTION_DOWN:
            if (mDoubleTap != null) {
                boolean had = mHandler.hasMessages(TAP);
                if (had) mHandler.removeMessages(TAP);
                if (mCurrentDown != null && mPreviousUp != null && had && isDoubleTap(mCurrentDown, mPreviousUp, ev)) {
                    mIsDoubleTapping = true;
                    handled |= mDoubleTap.onDoubleTap(mCurrentDown);
                    handled |= mDoubleTap.onDoubleTapEvent(ev);
                } else mHandler.sendEmptyMessageDelayed(TAP, ViewConfiguration.getDoubleTapTimeout());
            }
            mDownX = mLastX = fx; mDownY = mLastY = fy;
            mCurrentDown = MotionEvent.obtain(ev);
            mAlwaysInTapRegion = mAlwaysInBiggerTapRegion = true; mStillDown = true; mInLongPress = false; mDeferConfirm = false;
            if (mLongpressEnabled) { mHandler.removeMessages(LONG_PRESS); mHandler.sendEmptyMessageDelayed(LONG_PRESS, ViewConfiguration.getLongPressTimeout()); }
            mHandler.sendEmptyMessageDelayed(SHOW_PRESS, ViewConfiguration.getTapTimeout());
            handled |= mListener.onDown(ev);
            break;
        case MotionEvent.ACTION_MOVE:
            if (mInLongPress) break;
            float dx = mLastX - fx, dy = mLastY - fy;
            if (mIsDoubleTapping) handled |= mDoubleTap.onDoubleTapEvent(ev);
            else if (mAlwaysInTapRegion) {
                int ddx = (int) (fx - mDownX), ddy = (int) (fy - mDownY), dist = ddx * ddx + ddy * ddy;
                if (dist > mTouchSlopSq) {
                    handled = mListener.onScroll(mCurrentDown, ev, dx, dy);
                    mLastX = fx; mLastY = fy; mAlwaysInTapRegion = false;
                    mHandler.removeMessages(TAP); mHandler.removeMessages(SHOW_PRESS); mHandler.removeMessages(LONG_PRESS);
                }
                if (dist > mDoubleTapSlopSq) mAlwaysInBiggerTapRegion = false;
            } else if (Math.abs(dx) >= 1 || Math.abs(dy) >= 1) { handled = mListener.onScroll(mCurrentDown, ev, dx, dy); mLastX = fx; mLastY = fy; }
            break;
        case MotionEvent.ACTION_UP:
            mStillDown = false;
            MotionEvent cur = MotionEvent.obtain(ev);
            if (mIsDoubleTapping) handled |= mDoubleTap.onDoubleTapEvent(ev);
            else if (mInLongPress) { mHandler.removeMessages(TAP); mInLongPress = false; }
            else if (mAlwaysInTapRegion) { handled = mListener.onSingleTapUp(ev); if (mDeferConfirm && mDoubleTap != null) mDoubleTap.onSingleTapConfirmed(ev); }
            else {
                int id = ev.getPointerId(0);
                mVelocity.computeCurrentVelocity(1000, mMaxFling);
                float vy = mVelocity.getYVelocity(id), vx = mVelocity.getXVelocity(id);
                if (Math.abs(vy) > mMinFling || Math.abs(vx) > mMinFling) handled = mListener.onFling(mCurrentDown, ev, vx, vy);
            }
            mPreviousUp = cur;
            mVelocity.clear();
            mIsDoubleTapping = false; mDeferConfirm = false;
            mHandler.removeMessages(SHOW_PRESS); mHandler.removeMessages(LONG_PRESS);
            break;
        case MotionEvent.ACTION_CANCEL:
            mHandler.removeMessages(SHOW_PRESS); mHandler.removeMessages(LONG_PRESS); mHandler.removeMessages(TAP);
            mVelocity.clear(); mIsDoubleTapping = false; mStillDown = false; mAlwaysInTapRegion = false; mAlwaysInBiggerTapRegion = false; mDeferConfirm = false; mInLongPress = false;
            break;
        }
        return handled;
    }
    public boolean onGenericMotionEvent(MotionEvent e) { return false; }
    private void cancelTaps() { mHandler.removeMessages(SHOW_PRESS); mHandler.removeMessages(LONG_PRESS); mHandler.removeMessages(TAP); mIsDoubleTapping = false; mAlwaysInTapRegion = false; mAlwaysInBiggerTapRegion = false; mDeferConfirm = false; mInLongPress = false; }
    private boolean isDoubleTap(MotionEvent firstDown, MotionEvent firstUp, MotionEvent second) {
        if (!mAlwaysInBiggerTapRegion) return false;
        long dt = second.getEventTime() - firstUp.getEventTime();
        if (dt > ViewConfiguration.getDoubleTapTimeout() || dt < ViewConfiguration.getDoubleTapMinTime()) return false;
        int dx = (int) firstDown.getX() - (int) second.getX(), dy = (int) firstDown.getY() - (int) second.getY();
        return dx * dx + dy * dy < mDoubleTapSlopSq;
    }
}
