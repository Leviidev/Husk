package android.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.*;

/** A horizontal scroller for one child: drag, fling, overscroll glow, child-rectangle scrolling, smoothScrollTo. */
public class HorizontalScrollView extends FrameLayout {
    private long mLastScroll;
    private final Rect mTempRect = new Rect();
    private final OverScroller mScroller;
    private final EdgeEffect mEdgeGlowLeft, mEdgeGlowRight;
    private int mLastMotionX, mActivePointerId = -1, mTouchSlop, mMinimumVelocity, mMaximumVelocity, mOverscrollDistance, mOverflingDistance;
    private boolean mIsLayoutDirty = true, mIsBeingDragged, mFillViewport, mSmoothScrollingEnabled = true;
    private View mChildToScrollTo;
    private VelocityTracker mVelocityTracker;
    public HorizontalScrollView(Context c) { this(c, null); }
    public HorizontalScrollView(Context c, AttributeSet a) { this(c, a, android.R.attr.horizontalScrollViewStyle); }
    public HorizontalScrollView(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public HorizontalScrollView(Context c, AttributeSet attrs, int s, int r) {
        super(c, attrs, s, r);
        mScroller = new OverScroller(c);
        setFocusable(true); setDescendantFocusability(FOCUS_AFTER_DESCENDANTS); setWillNotDraw(false);
        ViewConfiguration vc = ViewConfiguration.get(c);
        mTouchSlop = vc.getScaledTouchSlop(); mMinimumVelocity = vc.getScaledMinimumFlingVelocity(); mMaximumVelocity = vc.getScaledMaximumFlingVelocity();
        mOverscrollDistance = vc.getScaledOverscrollDistance(); mOverflingDistance = vc.getScaledOverflingDistance();
        mEdgeGlowLeft = new EdgeEffect(c); mEdgeGlowRight = new EdgeEffect(c);
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.HorizontalScrollView, s, r);
        setFillViewport(a.getBoolean(husk.S.HorizontalScrollView_fillViewport, false));
        a.recycle();
    }
    @Override public boolean shouldDelayChildPressedState() { return true; }
    public int getMaxScrollAmount() { return (int) (0.5f * (mRight - mLeft)); }
    private void one() { if (getChildCount() > 0) throw new IllegalStateException("HorizontalScrollView can host only one direct child"); }
    @Override public void addView(View child) { one(); super.addView(child); }
    @Override public void addView(View child, int index) { one(); super.addView(child, index); }
    @Override public void addView(View child, ViewGroup.LayoutParams p) { one(); super.addView(child, p); }
    @Override public void addView(View child, int index, ViewGroup.LayoutParams p) { one(); super.addView(child, index, p); }
    public boolean isFillViewport() { return mFillViewport; }
    public void setFillViewport(boolean f) { if (f != mFillViewport) { mFillViewport = f; requestLayout(); } }
    public boolean isSmoothScrollingEnabled() { return mSmoothScrollingEnabled; }
    public void setSmoothScrollingEnabled(boolean e) { mSmoothScrollingEnabled = e; }
    public void setEdgeEffectColor(int c) { mEdgeGlowLeft.setColor(c); mEdgeGlowRight.setColor(c); }
    @Override protected void onMeasure(int ws, int hs) {
        super.onMeasure(ws, hs);
        if (!mFillViewport || MeasureSpec.getMode(ws) == MeasureSpec.UNSPECIFIED) return;
        if (getChildCount() > 0) {
            final View child = getChildAt(0);
            final FrameLayout.LayoutParams lp = (LayoutParams) child.getLayoutParams();
            int widthPadding = mPaddingLeft + mPaddingRight + lp.leftMargin + lp.rightMargin, heightPadding = mPaddingTop + mPaddingBottom + lp.topMargin + lp.bottomMargin;
            final int desiredWidth = getMeasuredWidth() - widthPadding;
            if (child.getMeasuredWidth() < desiredWidth) child.measure(MeasureSpec.makeMeasureSpec(desiredWidth, MeasureSpec.EXACTLY), getChildMeasureSpec(hs, heightPadding, lp.height));
        }
    }
    @Override protected void measureChild(View child, int ws, int hs) {
        ViewGroup.LayoutParams lp = child.getLayoutParams();
        child.measure(MeasureSpec.makeSafeMeasureSpec(Math.max(0, MeasureSpec.getSize(ws) - mPaddingLeft - mPaddingRight), MeasureSpec.UNSPECIFIED), getChildMeasureSpec(hs, mPaddingTop + mPaddingBottom, lp.height));
    }
    @Override protected void measureChildWithMargins(View child, int ws, int wUsed, int hs, int hUsed) {
        final MarginLayoutParams lp = (MarginLayoutParams) child.getLayoutParams();
        final int ch = getChildMeasureSpec(hs, mPaddingTop + mPaddingBottom + lp.topMargin + lp.bottomMargin + hUsed, lp.height);
        final int usedTotal = mPaddingLeft + mPaddingRight + lp.leftMargin + lp.rightMargin + wUsed;
        child.measure(MeasureSpec.makeSafeMeasureSpec(Math.max(0, MeasureSpec.getSize(ws) - usedTotal), MeasureSpec.UNSPECIFIED), ch);
    }
    private boolean inChild(int x, int y) { if (getChildCount() > 0) { final int sx = mScrollX; final View child = getChildAt(0); return !(y < child.getTop() || y >= child.getBottom() || x < child.getLeft() - sx || x >= child.getRight() - sx); } return false; }
    private void recycleVelocityTracker() { if (mVelocityTracker != null) { mVelocityTracker.recycle(); mVelocityTracker = null; } }
    @Override public void requestDisallowInterceptTouchEvent(boolean d) { if (d) recycleVelocityTracker(); super.requestDisallowInterceptTouchEvent(d); }
    @Override public boolean onInterceptTouchEvent(MotionEvent ev) {
        final int action = ev.getActionMasked();
        if (action == MotionEvent.ACTION_MOVE && mIsBeingDragged) return true;
        if (super.onInterceptTouchEvent(ev)) return true;
        switch (action) {
        case MotionEvent.ACTION_MOVE: {
            if (mActivePointerId == -1) break;
            final int pointerIndex = ev.findPointerIndex(mActivePointerId);
            if (pointerIndex == -1) break;
            final int x = (int) ev.getX(pointerIndex);
            if (Math.abs(x - mLastMotionX) > mTouchSlop) {
                mIsBeingDragged = true; mLastMotionX = x;
                if (mVelocityTracker == null) mVelocityTracker = VelocityTracker.obtain();
                mVelocityTracker.addMovement(ev);
                if (getParent() != null) getParent().requestDisallowInterceptTouchEvent(true);
            }
            break;
        }
        case MotionEvent.ACTION_DOWN: {
            final int x = (int) ev.getX();
            if (!inChild(x, (int) ev.getY())) { mIsBeingDragged = false; recycleVelocityTracker(); break; }
            mLastMotionX = x; mActivePointerId = ev.getPointerId(0);
            if (mVelocityTracker == null) mVelocityTracker = VelocityTracker.obtain(); else mVelocityTracker.clear();
            mVelocityTracker.addMovement(ev);
            mIsBeingDragged = !mScroller.isFinished();
            break;
        }
        case MotionEvent.ACTION_CANCEL: case MotionEvent.ACTION_UP:
            mIsBeingDragged = false; mActivePointerId = -1;
            if (mScroller.springBack(mScrollX, mScrollY, 0, getScrollRange(), 0, 0)) postInvalidateOnAnimation();
            break;
        case MotionEvent.ACTION_POINTER_DOWN: { final int index = ev.getActionIndex(); mLastMotionX = (int) ev.getX(index); mActivePointerId = ev.getPointerId(index); break; }
        case MotionEvent.ACTION_POINTER_UP: onSecondaryPointerUp(ev); mLastMotionX = (int) ev.getX(ev.findPointerIndex(mActivePointerId)); break;
        }
        return mIsBeingDragged;
    }
    @Override public boolean onTouchEvent(MotionEvent ev) {
        if (mVelocityTracker == null) mVelocityTracker = VelocityTracker.obtain();
        mVelocityTracker.addMovement(ev);
        switch (ev.getActionMasked()) {
        case MotionEvent.ACTION_DOWN:
            if (getChildCount() == 0) return false;
            if (mIsBeingDragged = !mScroller.isFinished()) { if (getParent() != null) getParent().requestDisallowInterceptTouchEvent(true); }
            if (!mScroller.isFinished()) mScroller.abortAnimation();
            mLastMotionX = (int) ev.getX(); mActivePointerId = ev.getPointerId(0);
            break;
        case MotionEvent.ACTION_MOVE: {
            final int activePointerIndex = ev.findPointerIndex(mActivePointerId);
            if (activePointerIndex == -1) break;
            final int x = (int) ev.getX(activePointerIndex);
            int deltaX = mLastMotionX - x;
            if (!mIsBeingDragged && Math.abs(deltaX) > mTouchSlop) { if (getParent() != null) getParent().requestDisallowInterceptTouchEvent(true); mIsBeingDragged = true; deltaX += deltaX > 0 ? -mTouchSlop : mTouchSlop; }
            if (mIsBeingDragged) {
                mLastMotionX = x;
                final int oldX = mScrollX, oldY = mScrollY, range = getScrollRange();
                if (overScrollBy(deltaX, 0, mScrollX, 0, range, 0, mOverscrollDistance, 0, true)) mVelocityTracker.clear();
                final int pulledToX = oldX + deltaX;
                if (range > 0) {
                    if (pulledToX < 0) { mEdgeGlowLeft.onPull((float) deltaX / getWidth(), 1.f - ev.getY(activePointerIndex) / getHeight()); if (!mEdgeGlowRight.isFinished()) mEdgeGlowRight.onRelease(); }
                    else if (pulledToX > range) { mEdgeGlowRight.onPull((float) deltaX / getWidth(), ev.getY(activePointerIndex) / getHeight()); if (!mEdgeGlowLeft.isFinished()) mEdgeGlowLeft.onRelease(); }
                    if (!mEdgeGlowLeft.isFinished() || !mEdgeGlowRight.isFinished()) postInvalidateOnAnimation();
                }
                if (oldX != mScrollX || oldY != mScrollY) onScrollChanged(mScrollX, mScrollY, oldX, oldY);
            }
            break;
        }
        case MotionEvent.ACTION_UP:
            if (mIsBeingDragged) {
                mVelocityTracker.computeCurrentVelocity(1000, mMaximumVelocity);
                int initialVelocity = (int) mVelocityTracker.getXVelocity(mActivePointerId);
                if (getChildCount() > 0) {
                    if (Math.abs(initialVelocity) > mMinimumVelocity) fling(-initialVelocity);
                    else if (mScroller.springBack(mScrollX, mScrollY, 0, getScrollRange(), 0, 0)) postInvalidateOnAnimation();
                }
                mActivePointerId = -1; mIsBeingDragged = false; recycleVelocityTracker(); mEdgeGlowLeft.onRelease(); mEdgeGlowRight.onRelease();
            }
            break;
        case MotionEvent.ACTION_CANCEL:
            if (mIsBeingDragged && getChildCount() > 0) { if (mScroller.springBack(mScrollX, mScrollY, 0, getScrollRange(), 0, 0)) postInvalidateOnAnimation(); mActivePointerId = -1; mIsBeingDragged = false; recycleVelocityTracker(); mEdgeGlowLeft.onRelease(); mEdgeGlowRight.onRelease(); }
            break;
        case MotionEvent.ACTION_POINTER_UP: onSecondaryPointerUp(ev); break;
        }
        return true;
    }
    private void onSecondaryPointerUp(MotionEvent ev) {
        final int pointerIndex = ev.getActionIndex(), pointerId = ev.getPointerId(pointerIndex);
        if (pointerId == mActivePointerId) { final int n = pointerIndex == 0 ? 1 : 0; mLastMotionX = (int) ev.getX(n); mActivePointerId = ev.getPointerId(n); if (mVelocityTracker != null) mVelocityTracker.clear(); }
    }
    @Override protected void onOverScrolled(int scrollX, int scrollY, boolean clampedX, boolean clampedY) {
        if (!mScroller.isFinished()) { final int oldX = mScrollX, oldY = mScrollY; mScrollX = scrollX; mScrollY = scrollY; onScrollChanged(mScrollX, mScrollY, oldX, oldY); if (clampedX) mScroller.springBack(mScrollX, mScrollY, 0, getScrollRange(), 0, 0); }
        else super.scrollTo(scrollX, scrollY);
        awakenScrollBars();
    }
    private int getScrollRange() { int r = 0; if (getChildCount() > 0) r = Math.max(0, getChildAt(0).getWidth() - (getWidth() - mPaddingLeft - mPaddingRight)); return r; }
    public boolean pageScroll(int direction) { int width = getWidth(); doScrollX(direction == View.FOCUS_RIGHT ? Math.min(width, getScrollRange() - mScrollX) : -Math.min(width, mScrollX)); return true; }
    public boolean fullScroll(int direction) { doScrollX(direction == View.FOCUS_RIGHT ? getScrollRange() - mScrollX : -mScrollX); return true; }
    public boolean arrowScroll(int direction) { int d = getMaxScrollAmount(); doScrollX(direction == View.FOCUS_RIGHT ? Math.min(d, getScrollRange() - mScrollX) : -Math.min(d, mScrollX)); return true; }
    private void doScrollX(int delta) { if (delta != 0) { if (mSmoothScrollingEnabled) smoothScrollBy(delta, 0); else scrollBy(delta, 0); } }
    public final void smoothScrollBy(int dx, int dy) {
        if (getChildCount() == 0) return;
        long duration = android.view.animation.AnimationUtils.currentAnimationTimeMillis() - mLastScroll;
        if (duration > ScrollView.ANIMATED_SCROLL_GAP) {
            final int maxX = getScrollRange(), scrollX = mScrollX;
            dx = Math.max(0, Math.min(scrollX + dx, maxX)) - scrollX;
            mScroller.startScroll(scrollX, mScrollY, dx, 0);
            postInvalidateOnAnimation();
        } else { if (!mScroller.isFinished()) mScroller.abortAnimation(); scrollBy(dx, dy); }
        mLastScroll = android.view.animation.AnimationUtils.currentAnimationTimeMillis();
    }
    public final void smoothScrollTo(int x, int y) { smoothScrollBy(x - mScrollX, y - mScrollY); }
    @Override protected int computeHorizontalScrollRange() {
        final int count = getChildCount(), contentWidth = getWidth() - mPaddingLeft - mPaddingRight;
        if (count == 0) return contentWidth;
        int scrollRange = getChildAt(0).getRight();
        final int scrollX = mScrollX, overscrollRight = Math.max(0, scrollRange - contentWidth);
        if (scrollX < 0) scrollRange -= scrollX; else if (scrollX > overscrollRight) scrollRange += scrollX - overscrollRight;
        return scrollRange;
    }
    @Override protected int computeHorizontalScrollOffset() { return Math.max(0, super.computeHorizontalScrollOffset()); }
    @Override public void computeScroll() {
        if (mScroller.computeScrollOffset()) {
            int oldX = mScrollX, oldY = mScrollY, x = mScroller.getCurrX(), y = mScroller.getCurrY();
            if (oldX != x || oldY != y) {
                final int range = getScrollRange();
                overScrollBy(x - oldX, y - oldY, oldX, oldY, range, 0, mOverflingDistance, 0, false);
                onScrollChanged(mScrollX, mScrollY, oldX, oldY);
                if (range > 0) { if (x < 0 && oldX >= 0) mEdgeGlowLeft.onAbsorb((int) mScroller.getCurrVelocity()); else if (x > range && oldX <= range) mEdgeGlowRight.onAbsorb((int) mScroller.getCurrVelocity()); }
            }
            if (!awakenScrollBars()) postInvalidateOnAnimation();
        }
    }
    protected int computeScrollDeltaToGetChildRectOnScreen(Rect rect) {
        if (getChildCount() == 0) return 0;
        int width = getWidth(), screenLeft = getScrollX(), screenRight = screenLeft + width, delta = 0;
        if (rect.right > screenRight && rect.left > screenLeft) { delta += rect.width() > width ? rect.left - screenLeft : rect.right - screenRight; delta = Math.min(delta, getChildAt(0).getRight() - screenRight); }
        else if (rect.left < screenLeft && rect.right < screenRight) { delta -= rect.width() > width ? screenRight - rect.right : screenLeft - rect.left; delta = Math.max(delta, -getScrollX()); }
        return delta;
    }
    private void scrollToChild(View child) { child.getDrawingRect(mTempRect); offsetDescendantRectToMyCoords(child, mTempRect); int d = computeScrollDeltaToGetChildRectOnScreen(mTempRect); if (d != 0) scrollBy(d, 0); }
    @Override public void requestChildFocus(View child, View focused) { if (focused != null && focused.getRevealOnFocusHint()) { if (!mIsLayoutDirty) scrollToChild(focused); else mChildToScrollTo = focused; } super.requestChildFocus(child, focused); }
    @Override public boolean requestChildRectangleOnScreen(View child, Rect rect, boolean immediate) { rect.offset(child.getLeft() - child.getScrollX(), child.getTop() - child.getScrollY()); int d = computeScrollDeltaToGetChildRectOnScreen(rect); if (d != 0) { if (immediate) scrollBy(d, 0); else smoothScrollBy(d, 0); } return d != 0; }
    @Override public void requestLayout() { mIsLayoutDirty = true; super.requestLayout(); }
    @Override protected void onLayout(boolean changed, int l, int t, int r, int b) {
        super.onLayout(changed, l, t, r, b);
        mIsLayoutDirty = false;
        if (mChildToScrollTo != null && mChildToScrollTo.isAttachedToWindow()) scrollToChild(mChildToScrollTo);
        mChildToScrollTo = null;
        if (!isLaidOut()) { final int childWidth = getChildCount() > 0 ? getChildAt(0).getMeasuredWidth() : 0; final int range = Math.max(0, childWidth - (r - l - mPaddingLeft - mPaddingRight)); if (mScrollX > range) mScrollX = range; else if (mScrollX < 0) mScrollX = 0; }
        scrollTo(mScrollX, mScrollY);
    }
    public void fling(int velocityX) {
        if (getChildCount() > 0) {
            int width = getWidth() - mPaddingRight - mPaddingLeft, right = getChildAt(0).getWidth();
            mScroller.fling(mScrollX, mScrollY, velocityX, 0, 0, Math.max(0, right - width), 0, 0, width / 2, 0);
            postInvalidateOnAnimation();
        }
    }
    @Override public void scrollTo(int x, int y) {
        if (getChildCount() > 0) {
            View child = getChildAt(0);
            x = clamp(x, getWidth() - mPaddingRight - mPaddingLeft, child.getWidth());
            y = clamp(y, getHeight() - mPaddingBottom - mPaddingTop, child.getHeight());
            if (x != mScrollX || y != mScrollY) super.scrollTo(x, y);
        }
    }
    @Override public void draw(Canvas canvas) {
        super.draw(canvas);
        final int scrollX = mScrollX;
        if (!mEdgeGlowLeft.isFinished()) { final int rc = canvas.save(); final int height = getHeight() - mPaddingTop - mPaddingBottom; canvas.rotate(270); canvas.translate(-height + mPaddingTop, Math.min(0, scrollX)); mEdgeGlowLeft.setSize(height, getWidth()); if (mEdgeGlowLeft.draw(canvas)) postInvalidateOnAnimation(); canvas.restoreToCount(rc); }
        if (!mEdgeGlowRight.isFinished()) { final int rc = canvas.save(); final int width = getWidth(), height = getHeight() - mPaddingTop - mPaddingBottom; canvas.rotate(90); canvas.translate(-mPaddingTop, -(Math.max(getScrollRange(), scrollX) + width)); mEdgeGlowRight.setSize(height, width); if (mEdgeGlowRight.draw(canvas)) postInvalidateOnAnimation(); canvas.restoreToCount(rc); }
    }
    private static int clamp(int n, int my, int child) { if (my >= child || n < 0) return 0; if ((my + n) > child) return child - my; return n; }
    @Override public CharSequence getAccessibilityClassName() { return HorizontalScrollView.class.getName(); }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
    protected void encodeProperties(android.view.ViewHierarchyEncoder p0) {}
    public boolean executeKeyEvent(android.view.KeyEvent p0) { return false; }
    public int getLeftEdgeEffectColor() { return (huskFill.get("LeftEdgeEffectColor") instanceof Integer ? (Integer) huskFill.get("LeftEdgeEffectColor") : 0); }
    protected float getLeftFadingEdgeStrength() { return 0f; }
    public int getRightEdgeEffectColor() { return (huskFill.get("RightEdgeEffectColor") instanceof Integer ? (Integer) huskFill.get("RightEdgeEffectColor") : 0); }
    protected float getRightFadingEdgeStrength() { return 0f; }
    public void onInitializeAccessibilityEventInternal(android.view.accessibility.AccessibilityEvent p0) {}
    public void onInitializeAccessibilityNodeInfoInternal(android.view.accessibility.AccessibilityNodeInfo p0) {}
    public boolean performAccessibilityActionInternal(int p0, android.os.Bundle p1) { return false; }
    public void setLeftEdgeEffectColor(int p0) { huskFill.put("LeftEdgeEffectColor", Integer.valueOf(p0)); }
    public void setRightEdgeEffectColor(int p0) { huskFill.put("RightEdgeEffectColor", Integer.valueOf(p0)); }
    // ---- end of generated members
}
