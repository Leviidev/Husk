package android.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.*;

/** A vertical scroller for one child: drag, fling, overscroll glow, focus and child-rectangle scrolling, smoothScrollTo. */
public class ScrollView extends FrameLayout {
    static final int ANIMATED_SCROLL_GAP = 250;
    private long mLastScroll;
    private final Rect mTempRect = new Rect();
    private OverScroller mScroller;
    private EdgeEffect mEdgeGlowTop, mEdgeGlowBottom;
    private int mLastMotionY, mActivePointerId = -1, mTouchSlop, mMinimumVelocity, mMaximumVelocity, mOverscrollDistance, mOverflingDistance;
    private boolean mIsLayoutDirty = true, mIsBeingDragged, mFillViewport, mSmoothScrollingEnabled = true;
    private View mChildToScrollTo;
    private VelocityTracker mVelocityTracker;
    public ScrollView(Context c) { this(c, null); }
    public ScrollView(Context c, AttributeSet a) { this(c, a, android.R.attr.scrollViewStyle); }
    public ScrollView(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public ScrollView(Context c, AttributeSet attrs, int s, int r) {
        super(c, attrs, s, r);
        mScroller = new OverScroller(c);
        setFocusable(true); setDescendantFocusability(FOCUS_AFTER_DESCENDANTS); setWillNotDraw(false);
        ViewConfiguration vc = ViewConfiguration.get(c);
        mTouchSlop = vc.getScaledTouchSlop(); mMinimumVelocity = vc.getScaledMinimumFlingVelocity(); mMaximumVelocity = vc.getScaledMaximumFlingVelocity();
        mOverscrollDistance = vc.getScaledOverscrollDistance(); mOverflingDistance = vc.getScaledOverflingDistance();
        mEdgeGlowTop = new EdgeEffect(c); mEdgeGlowBottom = new EdgeEffect(c);
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.ScrollView, s, r);
        setFillViewport(a.getBoolean(husk.S.ScrollView_fillViewport, false));
        a.recycle();
    }
    @Override public boolean shouldDelayChildPressedState() { return true; }
    public int getMaxScrollAmount() { return (int) (0.5f * (mBottom - mTop)); }
    @Override public void addView(View child) { if (getChildCount() > 0) throw new IllegalStateException("ScrollView can host only one direct child"); super.addView(child); }
    @Override public void addView(View child, int index) { if (getChildCount() > 0) throw new IllegalStateException("ScrollView can host only one direct child"); super.addView(child, index); }
    @Override public void addView(View child, ViewGroup.LayoutParams p) { if (getChildCount() > 0) throw new IllegalStateException("ScrollView can host only one direct child"); super.addView(child, p); }
    @Override public void addView(View child, int index, ViewGroup.LayoutParams p) { if (getChildCount() > 0) throw new IllegalStateException("ScrollView can host only one direct child"); super.addView(child, index, p); }
    private boolean canScroll() { View child = getChildAt(0); if (child != null) { int ch = child.getHeight(); return getHeight() < ch + mPaddingTop + mPaddingBottom; } return false; }
    public boolean isFillViewport() { return mFillViewport; }
    public void setFillViewport(boolean f) { if (f != mFillViewport) { mFillViewport = f; requestLayout(); } }
    public boolean isSmoothScrollingEnabled() { return mSmoothScrollingEnabled; }
    public void setSmoothScrollingEnabled(boolean e) { mSmoothScrollingEnabled = e; }
    public void setEdgeEffectColor(int c) { mEdgeGlowTop.setColor(c); mEdgeGlowBottom.setColor(c); }
    public void setTopEdgeEffectColor(int c) { mEdgeGlowTop.setColor(c); }
    public void setBottomEdgeEffectColor(int c) { mEdgeGlowBottom.setColor(c); }
    @Override protected void onMeasure(int ws, int hs) {
        super.onMeasure(ws, hs);
        if (!mFillViewport) return;
        if (MeasureSpec.getMode(hs) == MeasureSpec.UNSPECIFIED) return;
        if (getChildCount() > 0) {
            final View child = getChildAt(0);
            final FrameLayout.LayoutParams lp = (LayoutParams) child.getLayoutParams();
            int widthPadding = mPaddingLeft + mPaddingRight + lp.leftMargin + lp.rightMargin, heightPadding = mPaddingTop + mPaddingBottom + lp.topMargin + lp.bottomMargin;
            final int desiredHeight = getMeasuredHeight() - heightPadding;
            if (child.getMeasuredHeight() < desiredHeight) {
                child.measure(getChildMeasureSpec(ws, widthPadding, lp.width), MeasureSpec.makeMeasureSpec(desiredHeight, MeasureSpec.EXACTLY));
            }
        }
    }
    @Override protected void measureChild(View child, int ws, int hs) {
        ViewGroup.LayoutParams lp = child.getLayoutParams();
        child.measure(getChildMeasureSpec(ws, mPaddingLeft + mPaddingRight, lp.width), MeasureSpec.makeSafeMeasureSpec(Math.max(0, MeasureSpec.getSize(hs) - mPaddingTop - mPaddingBottom), MeasureSpec.UNSPECIFIED));
    }
    @Override protected void measureChildWithMargins(View child, int ws, int wUsed, int hs, int hUsed) {
        final MarginLayoutParams lp = (MarginLayoutParams) child.getLayoutParams();
        final int cw = getChildMeasureSpec(ws, mPaddingLeft + mPaddingRight + lp.leftMargin + lp.rightMargin + wUsed, lp.width);
        final int usedTotal = mPaddingTop + mPaddingBottom + lp.topMargin + lp.bottomMargin + hUsed;
        child.measure(cw, MeasureSpec.makeSafeMeasureSpec(Math.max(0, MeasureSpec.getSize(hs) - usedTotal), MeasureSpec.UNSPECIFIED));
    }
    @Override public boolean dispatchKeyEvent(KeyEvent e) { return super.dispatchKeyEvent(e) || executeKeyEvent(e); }
    public boolean executeKeyEvent(KeyEvent e) {
        if (!canScroll() || e.getAction() != KeyEvent.ACTION_DOWN) return false;
        switch (e.getKeyCode()) {
        case KeyEvent.KEYCODE_DPAD_UP: return e.isAltPressed() ? fullScroll(View.FOCUS_UP) : arrowScroll(View.FOCUS_UP);
        case KeyEvent.KEYCODE_DPAD_DOWN: return e.isAltPressed() ? fullScroll(View.FOCUS_DOWN) : arrowScroll(View.FOCUS_DOWN);
        case KeyEvent.KEYCODE_SPACE: pageScroll(e.isShiftPressed() ? View.FOCUS_UP : View.FOCUS_DOWN); return true;
        case KeyEvent.KEYCODE_PAGE_UP: return pageScroll(View.FOCUS_UP);
        case KeyEvent.KEYCODE_PAGE_DOWN: return pageScroll(View.FOCUS_DOWN);
        }
        return false;
    }
    private boolean inChild(int x, int y) { if (getChildCount() > 0) { final int sy = mScrollY; final View child = getChildAt(0); return !(y < child.getTop() - sy || y >= child.getBottom() - sy || x < child.getLeft() || x >= child.getRight()); } return false; }
    private void initOrResetVelocityTracker() { if (mVelocityTracker == null) mVelocityTracker = VelocityTracker.obtain(); else mVelocityTracker.clear(); }
    private void recycleVelocityTracker() { if (mVelocityTracker != null) { mVelocityTracker.recycle(); mVelocityTracker = null; } }
    @Override public void requestDisallowInterceptTouchEvent(boolean d) { if (d) recycleVelocityTracker(); super.requestDisallowInterceptTouchEvent(d); }
    @Override public boolean onInterceptTouchEvent(MotionEvent ev) {
        final int action = ev.getActionMasked();
        if (action == MotionEvent.ACTION_MOVE && mIsBeingDragged) return true;
        if (super.onInterceptTouchEvent(ev)) return true;
        if (getScrollY() == 0 && !canScrollVertically(1)) return false;
        switch (action) {
        case MotionEvent.ACTION_MOVE: {
            final int activePointerId = mActivePointerId;
            if (activePointerId == -1) break;
            final int pointerIndex = ev.findPointerIndex(activePointerId);
            if (pointerIndex == -1) break;
            final int y = (int) ev.getY(pointerIndex);
            final int yDiff = Math.abs(y - mLastMotionY);
            if (yDiff > mTouchSlop) {
                mIsBeingDragged = true; mLastMotionY = y;
                if (mVelocityTracker == null) mVelocityTracker = VelocityTracker.obtain();
                mVelocityTracker.addMovement(ev);
                final ViewParent parent = getParent();
                if (parent != null) parent.requestDisallowInterceptTouchEvent(true);
            }
            break;
        }
        case MotionEvent.ACTION_DOWN: {
            final int y = (int) ev.getY();
            if (!inChild((int) ev.getX(), y)) { mIsBeingDragged = false; recycleVelocityTracker(); break; }
            mLastMotionY = y; mActivePointerId = ev.getPointerId(0);
            initOrResetVelocityTracker(); mVelocityTracker.addMovement(ev);
            mScroller.computeScrollOffset();
            mIsBeingDragged = !mScroller.isFinished();
            break;
        }
        case MotionEvent.ACTION_CANCEL: case MotionEvent.ACTION_UP:
            mIsBeingDragged = false; mActivePointerId = -1; recycleVelocityTracker();
            if (mScroller.springBack(mScrollX, mScrollY, 0, 0, 0, getScrollRange())) postInvalidateOnAnimation();
            break;
        case MotionEvent.ACTION_POINTER_UP: onSecondaryPointerUp(ev); break;
        }
        return mIsBeingDragged;
    }
    @Override public boolean onTouchEvent(MotionEvent ev) {
        if (mVelocityTracker == null) mVelocityTracker = VelocityTracker.obtain();
        MotionEvent vtev = MotionEvent.obtain(ev);
        final int action = ev.getActionMasked();
        switch (action) {
        case MotionEvent.ACTION_DOWN: {
            if (getChildCount() == 0) return false;
            if (mIsBeingDragged = !mScroller.isFinished()) { final ViewParent parent = getParent(); if (parent != null) parent.requestDisallowInterceptTouchEvent(true); }
            if (!mScroller.isFinished()) mScroller.abortAnimation();
            mLastMotionY = (int) ev.getY(); mActivePointerId = ev.getPointerId(0);
            break;
        }
        case MotionEvent.ACTION_MOVE: {
            final int activePointerIndex = ev.findPointerIndex(mActivePointerId);
            if (activePointerIndex == -1) break;
            final int y = (int) ev.getY(activePointerIndex);
            int deltaY = mLastMotionY - y;
            if (!mIsBeingDragged && Math.abs(deltaY) > mTouchSlop) {
                final ViewParent parent = getParent();
                if (parent != null) parent.requestDisallowInterceptTouchEvent(true);
                mIsBeingDragged = true;
                if (deltaY > 0) deltaY -= mTouchSlop; else deltaY += mTouchSlop;
            }
            if (mIsBeingDragged) {
                mLastMotionY = y;
                final int oldX = mScrollX, oldY = mScrollY, range = getScrollRange();
                if (overScrollBy(0, deltaY, 0, mScrollY, 0, range, 0, mOverscrollDistance, true)) mVelocityTracker.clear();
                final int pulledToY = oldY + deltaY;
                if (range > 0) {
                    if (pulledToY < 0) { mEdgeGlowTop.onPull((float) deltaY / getHeight(), ev.getX(activePointerIndex) / getWidth()); if (!mEdgeGlowBottom.isFinished()) mEdgeGlowBottom.onRelease(); }
                    else if (pulledToY > range) { mEdgeGlowBottom.onPull((float) deltaY / getHeight(), 1.f - ev.getX(activePointerIndex) / getWidth()); if (!mEdgeGlowTop.isFinished()) mEdgeGlowTop.onRelease(); }
                    if (!mEdgeGlowTop.isFinished() || !mEdgeGlowBottom.isFinished()) postInvalidateOnAnimation();
                }
                if (oldX != mScrollX || oldY != mScrollY) onScrollChanged(mScrollX, mScrollY, oldX, oldY);
            }
            break;
        }
        case MotionEvent.ACTION_UP:
            if (mIsBeingDragged) {
                final VelocityTracker vt = mVelocityTracker;
                vt.addMovement(vtev);
                vt.computeCurrentVelocity(1000, mMaximumVelocity);
                int initialVelocity = (int) vt.getYVelocity(mActivePointerId);
                if (Math.abs(initialVelocity) > mMinimumVelocity) flingWithNestedDispatch(-initialVelocity);
                else if (mScroller.springBack(mScrollX, mScrollY, 0, 0, 0, getScrollRange())) postInvalidateOnAnimation();
                mActivePointerId = -1; endDrag();
            }
            break;
        case MotionEvent.ACTION_CANCEL:
            if (mIsBeingDragged && getChildCount() > 0) { if (mScroller.springBack(mScrollX, mScrollY, 0, 0, 0, getScrollRange())) postInvalidateOnAnimation(); mActivePointerId = -1; endDrag(); }
            break;
        case MotionEvent.ACTION_POINTER_DOWN: { final int index = ev.getActionIndex(); mLastMotionY = (int) ev.getY(index); mActivePointerId = ev.getPointerId(index); break; }
        case MotionEvent.ACTION_POINTER_UP: onSecondaryPointerUp(ev); mLastMotionY = (int) ev.getY(ev.findPointerIndex(mActivePointerId)); break;
        }
        if (mVelocityTracker != null && action != MotionEvent.ACTION_UP) mVelocityTracker.addMovement(vtev);
        vtev.recycle();
        return true;
    }
    private void onSecondaryPointerUp(MotionEvent ev) {
        final int pointerIndex = ev.getActionIndex(), pointerId = ev.getPointerId(pointerIndex);
        if (pointerId == mActivePointerId) { final int newPointerIndex = pointerIndex == 0 ? 1 : 0; mLastMotionY = (int) ev.getY(newPointerIndex); mActivePointerId = ev.getPointerId(newPointerIndex); if (mVelocityTracker != null) mVelocityTracker.clear(); }
    }
    @Override protected void onOverScrolled(int scrollX, int scrollY, boolean clampedX, boolean clampedY) {
        if (!mScroller.isFinished()) {
            final int oldX = mScrollX, oldY = mScrollY;
            mScrollX = scrollX; mScrollY = scrollY;
            onScrollChanged(mScrollX, mScrollY, oldX, oldY);
            if (clampedY) mScroller.springBack(mScrollX, mScrollY, 0, 0, 0, getScrollRange());
        } else super.scrollTo(scrollX, scrollY);
        awakenScrollBars();
    }
    private int getScrollRange() { int scrollRange = 0; if (getChildCount() > 0) { View child = getChildAt(0); scrollRange = Math.max(0, child.getHeight() - (getHeight() - mPaddingBottom - mPaddingTop)); } return scrollRange; }
    public boolean pageScroll(int direction) {
        boolean down = direction == View.FOCUS_DOWN;
        int height = getHeight();
        if (down) { mTempRect.top = getScrollY() + height; int count = getChildCount(); if (count > 0) { View view = getChildAt(count - 1); if (mTempRect.top + height > view.getBottom()) mTempRect.top = view.getBottom() - height; } }
        else { mTempRect.top = getScrollY() - height; if (mTempRect.top < 0) mTempRect.top = 0; }
        mTempRect.bottom = mTempRect.top + height;
        return scrollAndFocus(direction, mTempRect.top, mTempRect.bottom);
    }
    public boolean fullScroll(int direction) {
        boolean down = direction == View.FOCUS_DOWN;
        int height = getHeight();
        mTempRect.top = 0; mTempRect.bottom = height;
        if (down) { int count = getChildCount(); if (count > 0) { View view = getChildAt(count - 1); mTempRect.bottom = view.getBottom() + mPaddingBottom; mTempRect.top = mTempRect.bottom - height; } }
        return scrollAndFocus(direction, mTempRect.top, mTempRect.bottom);
    }
    private boolean scrollAndFocus(int direction, int top, int bottom) {
        int height = getHeight(), containerTop = getScrollY(), containerBottom = containerTop + height;
        boolean up = direction == View.FOCUS_UP;
        if (top >= containerTop && bottom <= containerBottom) return false;
        int delta = up ? (top - containerTop) : (bottom - containerBottom);
        doScrollY(delta);
        return true;
    }
    public boolean arrowScroll(int direction) {
        int scrollDelta = getMaxScrollAmount();
        if (direction == View.FOCUS_UP && getScrollY() < scrollDelta) scrollDelta = getScrollY();
        else if (direction == View.FOCUS_DOWN && getChildCount() > 0) { int daBottom = getChildAt(0).getBottom(), screenBottom = getScrollY() + getHeight() - mPaddingBottom; if (daBottom - screenBottom < scrollDelta) scrollDelta = daBottom - screenBottom; }
        if (scrollDelta == 0) return false;
        doScrollY(direction == View.FOCUS_DOWN ? scrollDelta : -scrollDelta);
        return true;
    }
    private void doScrollY(int delta) { if (delta != 0) { if (mSmoothScrollingEnabled) smoothScrollBy(0, delta); else scrollBy(0, delta); } }
    public final void smoothScrollBy(int dx, int dy) {
        if (getChildCount() == 0) return;
        long duration = android.view.animation.AnimationUtils.currentAnimationTimeMillis() - mLastScroll;
        if (duration > ANIMATED_SCROLL_GAP) {
            final int height = getHeight() - mPaddingBottom - mPaddingTop, bottom = getChildAt(0).getHeight(), maxY = Math.max(0, bottom - height), scrollY = mScrollY;
            dy = Math.max(0, Math.min(scrollY + dy, maxY)) - scrollY;
            mScroller.startScroll(mScrollX, scrollY, 0, dy);
            postInvalidateOnAnimation();
        } else { if (!mScroller.isFinished()) mScroller.abortAnimation(); scrollBy(dx, dy); }
        mLastScroll = android.view.animation.AnimationUtils.currentAnimationTimeMillis();
    }
    public final void smoothScrollTo(int x, int y) { smoothScrollBy(x - mScrollX, y - mScrollY); }
    @Override protected int computeVerticalScrollRange() {
        final int count = getChildCount(), contentHeight = getHeight() - mPaddingBottom - mPaddingTop;
        if (count == 0) return contentHeight;
        int scrollRange = getChildAt(0).getBottom();
        final int scrollY = mScrollY, overscrollBottom = Math.max(0, scrollRange - contentHeight);
        if (scrollY < 0) scrollRange -= scrollY; else if (scrollY > overscrollBottom) scrollRange += scrollY - overscrollBottom;
        return scrollRange;
    }
    @Override protected int computeVerticalScrollOffset() { return Math.max(0, super.computeVerticalScrollOffset()); }
    @Override public void computeScroll() {
        if (mScroller.computeScrollOffset()) {
            int oldX = mScrollX, oldY = mScrollY, x = mScroller.getCurrX(), y = mScroller.getCurrY();
            if (oldX != x || oldY != y) {
                final int range = getScrollRange();
                overScrollBy(x - oldX, y - oldY, oldX, oldY, 0, range, 0, mOverflingDistance, false);
                onScrollChanged(mScrollX, mScrollY, oldX, oldY);
                if (range > 0) { if (y < 0 && oldY >= 0) mEdgeGlowTop.onAbsorb((int) mScroller.getCurrVelocity()); else if (y > range && oldY <= range) mEdgeGlowBottom.onAbsorb((int) mScroller.getCurrVelocity()); }
            }
            if (!awakenScrollBars()) postInvalidateOnAnimation();
        }
    }
    public void scrollToDescendant(View child) { if (!mIsLayoutDirty) { child.getDrawingRect(mTempRect); offsetDescendantRectToMyCoords(child, mTempRect); int d = computeScrollDeltaToGetChildRectOnScreen(mTempRect); if (d != 0) scrollBy(0, d); } else mChildToScrollTo = child; }
    private boolean scrollToChildRect(Rect rect, boolean immediate) { final int delta = computeScrollDeltaToGetChildRectOnScreen(rect); final boolean scroll = delta != 0; if (scroll) { if (immediate) scrollBy(0, delta); else smoothScrollBy(0, delta); } return scroll; }
    protected int computeScrollDeltaToGetChildRectOnScreen(Rect rect) {
        if (getChildCount() == 0) return 0;
        int height = getHeight(), screenTop = getScrollY(), screenBottom = screenTop + height, fadingEdge = getVerticalFadingEdgeLength();
        if (rect.top > 0) screenTop += fadingEdge;
        if (rect.bottom < getChildAt(0).getHeight()) screenBottom -= fadingEdge;
        int scrollYDelta = 0;
        if (rect.bottom > screenBottom && rect.top > screenTop) {
            if (rect.height() > height) scrollYDelta += (rect.top - screenTop); else scrollYDelta += (rect.bottom - screenBottom);
            int bottom = getChildAt(0).getBottom(), distanceToBottom = bottom - screenBottom;
            scrollYDelta = Math.min(scrollYDelta, distanceToBottom);
        } else if (rect.top < screenTop && rect.bottom < screenBottom) {
            if (rect.height() > height) scrollYDelta -= (screenBottom - rect.bottom); else scrollYDelta -= (screenTop - rect.top);
            scrollYDelta = Math.max(scrollYDelta, -getScrollY());
        }
        return scrollYDelta;
    }
    @Override public void requestChildFocus(View child, View focused) { if (focused != null && focused.getRevealOnFocusHint()) { if (!mIsLayoutDirty) scrollToDescendant(focused); else mChildToScrollTo = focused; } super.requestChildFocus(child, focused); }
    @Override public boolean requestChildRectangleOnScreen(View child, Rect rectangle, boolean immediate) { rectangle.offset(child.getLeft() - child.getScrollX(), child.getTop() - child.getScrollY()); return scrollToChildRect(rectangle, immediate); }
    @Override public void requestLayout() { mIsLayoutDirty = true; super.requestLayout(); }
    @Override protected void onLayout(boolean changed, int l, int t, int r, int b) {
        super.onLayout(changed, l, t, r, b);
        mIsLayoutDirty = false;
        if (mChildToScrollTo != null && isViewDescendantOf(mChildToScrollTo, this)) scrollToDescendant(mChildToScrollTo);
        mChildToScrollTo = null;
        if (!isLaidOut()) {
            final int childHeight = getChildCount() > 0 ? getChildAt(0).getMeasuredHeight() : 0;
            final int scrollRange = Math.max(0, childHeight - (b - t - mPaddingBottom - mPaddingTop));
            if (mScrollY > scrollRange) mScrollY = scrollRange; else if (mScrollY < 0) mScrollY = 0;
        }
        scrollTo(mScrollX, mScrollY);
    }
    @Override protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        View currentFocused = findFocus();
        if (null == currentFocused || this == currentFocused) return;
        if (isWithinDeltaOfScreen(currentFocused, 0, oldh)) { currentFocused.getDrawingRect(mTempRect); offsetDescendantRectToMyCoords(currentFocused, mTempRect); doScrollY(computeScrollDeltaToGetChildRectOnScreen(mTempRect)); }
    }
    private boolean isWithinDeltaOfScreen(View descendant, int delta, int height) { descendant.getDrawingRect(mTempRect); offsetDescendantRectToMyCoords(descendant, mTempRect); return (mTempRect.bottom + delta) >= getScrollY() && (mTempRect.top - delta) <= (getScrollY() + height); }
    private static boolean isViewDescendantOf(View child, View parent) { if (child == parent) return true; final ViewParent theParent = child.getParent(); return (theParent instanceof ViewGroup) && isViewDescendantOf((View) theParent, parent); }
    public void fling(int velocityY) {
        if (getChildCount() > 0) {
            int height = getHeight() - mPaddingBottom - mPaddingTop, bottom = getChildAt(0).getHeight();
            mScroller.fling(mScrollX, mScrollY, 0, velocityY, 0, 0, 0, Math.max(0, bottom - height), 0, height / 2);
            postInvalidateOnAnimation();
        }
    }
    private void flingWithNestedDispatch(int velocityY) { final boolean canFling = (mScrollY > 0 || velocityY > 0) && (mScrollY < getScrollRange() || velocityY < 0); if (canFling) fling(velocityY); }
    private void endDrag() { mIsBeingDragged = false; recycleVelocityTracker(); mEdgeGlowTop.onRelease(); mEdgeGlowBottom.onRelease(); }
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
        final int scrollY = mScrollY;
        if (!mEdgeGlowTop.isFinished()) { final int restoreCount = canvas.save(); int width = getWidth() - mPaddingLeft - mPaddingRight; canvas.translate(mPaddingLeft, Math.min(0, scrollY)); mEdgeGlowTop.setSize(width, getHeight()); if (mEdgeGlowTop.draw(canvas)) postInvalidateOnAnimation(); canvas.restoreToCount(restoreCount); }
        if (!mEdgeGlowBottom.isFinished()) { final int restoreCount = canvas.save(); int width = getWidth() - mPaddingLeft - mPaddingRight, height = getHeight(); canvas.translate(-width + mPaddingLeft, Math.max(getScrollRange(), scrollY) + height); canvas.rotate(180, width, 0); mEdgeGlowBottom.setSize(width, height); if (mEdgeGlowBottom.draw(canvas)) postInvalidateOnAnimation(); canvas.restoreToCount(restoreCount); }
    }
    private static int clamp(int n, int my, int child) { if (my >= child || n < 0) return 0; if ((my + n) > child) return child - my; return n; }
    @Override public CharSequence getAccessibilityClassName() { return ScrollView.class.getName(); }
    // ---- generated by tools/compat/genstubs.py: the platform's nested classes this class does not write
    public static final class InspectionCompanion implements android.view.inspector.InspectionCompanion {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public InspectionCompanion() {}
        public void mapProperties(android.view.inspector.PropertyMapper p0) {}
        public void readProperties(android.widget.ScrollView p0, android.view.inspector.PropertyReader p1) {}
        public void readProperties(java.lang.Object p0, android.view.inspector.PropertyReader p1) {}
    }
    // ---- end of generated nested classes
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    protected void encodeProperties(android.view.ViewHierarchyEncoder p0) {}
    public int getBottomEdgeEffectColor() { return 0; }
    public int getTopEdgeEffectColor() { return 0; }
    public void onInitializeAccessibilityEventInternal(android.view.accessibility.AccessibilityEvent p0) {}
    public void onInitializeAccessibilityNodeInfoInternal(android.view.accessibility.AccessibilityNodeInfo p0) {}
    public boolean performAccessibilityActionInternal(int p0, android.os.Bundle p1) { return false; }
    // ---- end of generated members
}
