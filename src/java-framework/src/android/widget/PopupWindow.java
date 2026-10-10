package android.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.*;

/** A floating window over the app's: shown at a location or dropped under an anchor, dismissed by an outside touch or back. */
public class PopupWindow {
    public static final int INPUT_METHOD_FROM_FOCUSABLE = 0, INPUT_METHOD_NEEDED = 1, INPUT_METHOD_NOT_NEEDED = 2;
    public interface OnDismissListener { void onDismiss(); }
    private Context mContext;
    private WindowManager mWindowManager;
    private View mContentView, mDecorView;
    private boolean mIsShowing, mFocusable, mTouchable = true, mOutsideTouchable, mClippingEnabled = true, mOverlapAnchor, mAttachedInDecor = true, mSplitTouchEnabled = true;
    private int mWidth = ViewGroup.LayoutParams.WRAP_CONTENT, mHeight = ViewGroup.LayoutParams.WRAP_CONTENT, mInputMethodMode, mSoftInputMode, mAnimationStyle = -1, mWindowLayoutType = WindowManager.LayoutParams.TYPE_APPLICATION_PANEL;
    private Drawable mBackground;
    private float mElevation;
    private OnDismissListener mOnDismissListener;
    private View.OnTouchListener mTouchInterceptor;
    private View mAnchor;
    private int mAnchorXoff, mAnchorYoff, mAnchorGravity;
    private final int[] mTmp = new int[2];
    public PopupWindow(Context c) { this(c, null); }
    public PopupWindow(Context c, AttributeSet a) { this(c, a, android.R.attr.popupWindowStyle); }
    public PopupWindow(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public PopupWindow(Context c, AttributeSet attrs, int s, int r) {
        mContext = c;
        mWindowManager = (WindowManager) c.getSystemService(Context.WINDOW_SERVICE);
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.PopupWindow, s, r);
        try { mBackground = a.getDrawable(husk.S.PopupWindow_popupBackground); } catch (RuntimeException e) {}
        mElevation = a.getDimension(husk.S.PopupWindow_popupElevation, 0);
        mOverlapAnchor = a.getBoolean(husk.S.PopupWindow_overlapAnchor, false);
        a.recycle();
    }
    public PopupWindow() { this(null, 0, 0); }
    public PopupWindow(View contentView) { this(contentView, 0, 0); }
    public PopupWindow(int width, int height) { this(null, width, height); }
    public PopupWindow(View contentView, int width, int height) { this(contentView, width, height, false); }
    public PopupWindow(View contentView, int width, int height, boolean focusable) {
        if (contentView != null) { mContext = contentView.getContext(); mWindowManager = (WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE); }
        setContentView(contentView); setWidth(width); setHeight(height); setFocusable(focusable);
    }
    public Drawable getBackground() { return mBackground; }
    public void setBackgroundDrawable(Drawable b) { mBackground = b; }
    public float getElevation() { return mElevation; }
    public void setElevation(float e) { mElevation = e; }
    public int getAnimationStyle() { return mAnimationStyle; }
    public void setAnimationStyle(int s) { mAnimationStyle = s; }
    public void setEnterTransition(android.transition.Transition t) {} public void setExitTransition(android.transition.Transition t) {}
    public View getContentView() { return mContentView; }
    public void setContentView(View v) {
        if (isShowing()) return;
        mContentView = v;
        if (mContext == null && v != null) mContext = v.getContext();
        if (mWindowManager == null && v != null) mWindowManager = (WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE);
    }
    public void setTouchInterceptor(View.OnTouchListener l) { mTouchInterceptor = l; }
    public boolean isFocusable() { return mFocusable; }
    public void setFocusable(boolean f) { mFocusable = f; }
    public int getInputMethodMode() { return mInputMethodMode; }
    public void setInputMethodMode(int m) { mInputMethodMode = m; }
    public void setSoftInputMode(int m) { mSoftInputMode = m; }
    public int getSoftInputMode() { return mSoftInputMode; }
    public boolean isTouchable() { return mTouchable; }
    public void setTouchable(boolean t) { mTouchable = t; }
    public boolean isOutsideTouchable() { return mOutsideTouchable; }
    public void setOutsideTouchable(boolean t) { mOutsideTouchable = t; }
    public boolean isClippingEnabled() { return mClippingEnabled; }
    public void setClippingEnabled(boolean e) { mClippingEnabled = e; }
    public void setIsClippedToScreen(boolean e) {}
    public boolean isSplitTouchEnabled() { return mSplitTouchEnabled; }
    public void setSplitTouchEnabled(boolean e) { mSplitTouchEnabled = e; }
    public boolean isAttachedInDecor() { return mAttachedInDecor; }
    public void setAttachedInDecor(boolean e) { mAttachedInDecor = e; }
    public void setOverlapAnchor(boolean o) { mOverlapAnchor = o; }
    public boolean getOverlapAnchor() { return mOverlapAnchor; }
    public void setWindowLayoutType(int t) { mWindowLayoutType = t; }
    public int getWindowLayoutType() { return mWindowLayoutType; }
    public void setTouchModal(boolean m) {}
    public void setEpicenterBounds(Rect r) {}
    public int getHeight() { return mHeight; } public void setHeight(int h) { mHeight = h; }
    public int getWidth() { return mWidth; } public void setWidth(int w) { mWidth = w; }
    public void setWindowLayoutMode(int w, int h) { mWidth = w; mHeight = h; }
    public boolean isShowing() { return mIsShowing; }
    public boolean isAboveAnchor() { return false; }
    public void setOnDismissListener(OnDismissListener l) { mOnDismissListener = l; }
    /** The window's root: the content on the popup background, closing on an outside touch or back. */
    private final class PopupDecor extends FrameLayout {
        PopupDecor(Context c) { super(c); }
        @Override public boolean dispatchKeyEvent(KeyEvent e) {
            if (e.getKeyCode() == KeyEvent.KEYCODE_BACK) {
                if (e.getAction() == KeyEvent.ACTION_DOWN && e.getRepeatCount() == 0) { KeyEvent.DispatcherState s = getKeyDispatcherState(); if (s != null) s.startTracking(e, this); return true; }
                if (e.getAction() == KeyEvent.ACTION_UP) { dismiss(); return true; }
            }
            return super.dispatchKeyEvent(e);
        }
        @Override public boolean dispatchTouchEvent(MotionEvent e) { if (mTouchInterceptor != null && mTouchInterceptor.onTouch(this, e)) return true; return super.dispatchTouchEvent(e); }
        @Override public boolean onTouchEvent(MotionEvent e) {
            final int x = (int) e.getX(), y = (int) e.getY();
            if (e.getAction() == MotionEvent.ACTION_DOWN && (x < 0 || x >= getWidth() || y < 0 || y >= getHeight())) { dismiss(); return true; }
            if (e.getAction() == MotionEvent.ACTION_OUTSIDE) { dismiss(); return true; }
            return super.onTouchEvent(e);
        }
    }
    private WindowManager.LayoutParams createParams() {
        WindowManager.LayoutParams p = new WindowManager.LayoutParams();
        p.gravity = Gravity.START | Gravity.TOP;
        p.width = mWidth; p.height = mHeight;
        p.type = mWindowLayoutType;
        p.format = android.graphics.PixelFormat.TRANSLUCENT;
        int flags = 0;
        if (!mFocusable) flags |= WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
        if (!mTouchable) flags |= WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE;
        if (mOutsideTouchable || mFocusable) flags |= WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH;
        if (!mFocusable || mOutsideTouchable) flags |= WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL;
        if (!mClippingEnabled) flags |= WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS;
        p.flags = flags;
        p.softInputMode = mSoftInputMode;
        p.setTitle("PopupWindow");
        return p;
    }
    private void preparePopup() {
        PopupDecor d = new PopupDecor(mContext);
        if (mContentView.getParent() instanceof ViewGroup) ((ViewGroup) mContentView.getParent()).removeView(mContentView);
        ViewGroup.LayoutParams clp = mContentView.getLayoutParams();
        int cw = mWidth == ViewGroup.LayoutParams.WRAP_CONTENT || clp == null ? ViewGroup.LayoutParams.WRAP_CONTENT : ViewGroup.LayoutParams.MATCH_PARENT;
        int ch = mHeight == ViewGroup.LayoutParams.WRAP_CONTENT || clp == null ? ViewGroup.LayoutParams.WRAP_CONTENT : ViewGroup.LayoutParams.MATCH_PARENT;
        d.addView(mContentView, new FrameLayout.LayoutParams(mWidth == ViewGroup.LayoutParams.WRAP_CONTENT ? cw : ViewGroup.LayoutParams.MATCH_PARENT, mHeight == ViewGroup.LayoutParams.WRAP_CONTENT ? ch : ViewGroup.LayoutParams.MATCH_PARENT));
        if (mBackground != null) d.setBackground(mBackground);
        d.setElevation(mElevation);
        mDecorView = d;
    }
    public void showAtLocation(View parent, int gravity, int x, int y) { showAtLocation(parent != null ? parent.getWindowToken() : null, gravity, x, y); }
    public void showAtLocation(android.os.IBinder token, int gravity, int x, int y) {
        if (isShowing() || mContentView == null) return;
        mIsShowing = true;
        mAnchor = null;
        WindowManager.LayoutParams p = createParams();
        p.token = token;
        preparePopup();
        p.gravity = gravity == Gravity.NO_GRAVITY ? Gravity.TOP | Gravity.START : gravity;
        p.x = x; p.y = y;
        mWindowManager.addView(mDecorView, p);
    }
    public void showAsDropDown(View anchor) { showAsDropDown(anchor, 0, 0); }
    public void showAsDropDown(View anchor, int xoff, int yoff) { showAsDropDown(anchor, xoff, yoff, Gravity.TOP | Gravity.START); }
    public void showAsDropDown(View anchor, int xoff, int yoff, int gravity) {
        if (isShowing() || mContentView == null) return;
        mIsShowing = true;
        mAnchor = anchor; mAnchorXoff = xoff; mAnchorYoff = yoff; mAnchorGravity = gravity;
        WindowManager.LayoutParams p = createParams();
        p.token = anchor.getWindowToken();
        preparePopup();
        findDropDownPosition(anchor, p, xoff, yoff, gravity);
        mWindowManager.addView(mDecorView, p);
    }
    private void findDropDownPosition(View anchor, WindowManager.LayoutParams p, int xoff, int yoff, int gravity) {
        anchor.getLocationOnScreen(mTmp);
        int ax = mTmp[0], ay = mTmp[1];
        int sw = husk.Native.screenWidth(), sh = husk.Native.screenHeight();
        // measure to flip above when it does not fit below, and to keep it on screen
        int w = mWidth, h = mHeight;
        if (w < 0 || h < 0) {
            mDecorView.measure(View.MeasureSpec.makeMeasureSpec(sw, mWidth == ViewGroup.LayoutParams.MATCH_PARENT ? View.MeasureSpec.EXACTLY : View.MeasureSpec.AT_MOST), View.MeasureSpec.makeMeasureSpec(sh, View.MeasureSpec.AT_MOST));
            if (w < 0) w = mDecorView.getMeasuredWidth();
            if (h < 0) h = mDecorView.getMeasuredHeight();
        }
        int x = ax + xoff, y = ay + (mOverlapAnchor ? 0 : anchor.getHeight()) + yoff;
        if ((Gravity.getAbsoluteGravity(gravity, anchor.getLayoutDirection()) & Gravity.HORIZONTAL_GRAVITY_MASK) == Gravity.RIGHT) x = ax + anchor.getWidth() - w + xoff;
        Rect frame = new Rect();
        anchor.getWindowVisibleDisplayFrame(frame);
        if (y + h > frame.bottom) { int above = ay - h - yoff + (mOverlapAnchor ? anchor.getHeight() : 0); if (above >= frame.top) y = above; else y = Math.max(frame.top, frame.bottom - h); }
        if (mClippingEnabled) { if (x + w > frame.right) x = frame.right - w; if (x < frame.left) x = frame.left; }
        p.gravity = Gravity.LEFT | Gravity.TOP;
        p.x = x; p.y = y;
        // ViewRoot places panels inside the screen's insets; these are screen coordinates
        p.x -= insetLeft(); p.y -= insetTop();
    }
    private static int insetLeft() { int[] i = husk.ViewRoot.screenInsetsHusk(); return i[0]; }
    private static int insetTop() { int[] i = husk.ViewRoot.screenInsetsHusk(); return i[1]; }
    public void update() { if (isShowing()) { WindowManager.LayoutParams p = (WindowManager.LayoutParams) mDecorView.getLayoutParams(); p.width = mWidth; p.height = mHeight; mWindowManager.updateViewLayout(mDecorView, p); } }
    public void update(int width, int height) { mWidth = width; mHeight = height; update(); }
    public void update(int x, int y, int width, int height) { update(x, y, width, height, false); }
    public void update(int x, int y, int width, int height, boolean force) {
        if (width >= 0) mWidth = width; if (height >= 0) mHeight = height;
        if (!isShowing()) return;
        WindowManager.LayoutParams p = (WindowManager.LayoutParams) mDecorView.getLayoutParams();
        if (width != -1) p.width = width; if (height != -1) p.height = height;
        p.x = x; p.y = y;
        mWindowManager.updateViewLayout(mDecorView, p);
    }
    public void update(View anchor, int width, int height) { update(anchor, 0, 0, width, height); }
    public void update(View anchor, int xoff, int yoff, int width, int height) {
        if (!isShowing()) return;
        if (width >= 0) mWidth = width; if (height >= 0) mHeight = height;
        WindowManager.LayoutParams p = (WindowManager.LayoutParams) mDecorView.getLayoutParams();
        p.width = mWidth; p.height = mHeight;
        findDropDownPosition(anchor, p, xoff, yoff, mAnchorGravity);
        mWindowManager.updateViewLayout(mDecorView, p);
    }
    public int getMaxAvailableHeight(View anchor) { return getMaxAvailableHeight(anchor, 0); }
    public int getMaxAvailableHeight(View anchor, int yOffset) { return getMaxAvailableHeight(anchor, yOffset, false); }
    public int getMaxAvailableHeight(View anchor, int yOffset, boolean ignoreBottomDecorations) {
        Rect frame = new Rect(); anchor.getWindowVisibleDisplayFrame(frame);
        anchor.getLocationOnScreen(mTmp);
        int distanceToBottom = frame.bottom - (mTmp[1] + anchor.getHeight()) - yOffset, distanceToTop = mTmp[1] - frame.top + yOffset;
        int returnedHeight = Math.max(distanceToBottom, distanceToTop);
        if (mBackground != null) { Rect pad = new Rect(); mBackground.getPadding(pad); returnedHeight -= pad.top + pad.bottom; }
        return returnedHeight;
    }
    public void dismiss() {
        if (!isShowing()) return;
        mIsShowing = false;
        try { mWindowManager.removeViewImmediate(mDecorView); } catch (RuntimeException e) {}
        if (mDecorView instanceof ViewGroup) ((ViewGroup) mDecorView).removeView(mContentView);
        mDecorView = null;
        if (mOnDismissListener != null) mOnDismissListener.onDismiss();
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
    protected void attachToAnchor(android.view.View p0, int p1, int p2, int p3) {}
    protected android.view.WindowManager.LayoutParams createPopupLayoutParams(android.os.IBinder p0) { return null; }
    protected void detachFromAnchor() {}
    protected boolean findDropDownPosition(android.view.View p0, android.view.WindowManager.LayoutParams p1, int p2, int p3, int p4, int p5, int p6, boolean p7) { return false; }
    protected boolean getAllowScrollingAnchorParent() { return false; }
    protected android.view.View getAnchor() { return null; }
    protected android.view.WindowManager.LayoutParams getDecorViewLayoutParams() { return null; }
    public android.transition.Transition getEnterTransition() { return null; }
    public android.graphics.Rect getEpicenterBounds() { return null; }
    public android.transition.Transition getExitTransition() { return null; }
    protected android.widget.PopupWindow.OnDismissListener getOnDismissListener() { return null; }
    protected android.graphics.Rect getTransitionEpicenter() { return null; }
    protected boolean hasContentView() { return false; }
    protected boolean hasDecorView() { return false; }
    public boolean isClipToScreenEnabled() { return (huskFill.get("ClipToScreenEnabled") instanceof Boolean ? (Boolean) huskFill.get("ClipToScreenEnabled") : false); }
    public boolean isClippedToScreen() { return false; }
    public boolean isLaidOutInScreen() { return false; }
    public boolean isLayoutInScreenEnabled() { return (huskFill.get("LayoutInScreenEnabled") instanceof Boolean ? (Boolean) huskFill.get("LayoutInScreenEnabled") : false); }
    protected boolean isLayoutInsetDecor() { return (huskFill.get("LayoutInsetDecor") instanceof Boolean ? (Boolean) huskFill.get("LayoutInsetDecor") : false); }
    public boolean isTouchModal() { return false; }
    protected boolean isTransitioningToDismiss() { return (huskFill.get("TransitioningToDismiss") instanceof Boolean ? (Boolean) huskFill.get("TransitioningToDismiss") : false); }
    public void setClipToScreenEnabled(boolean p0) { huskFill.put("ClipToScreenEnabled", Boolean.valueOf(p0)); }
    protected void setDropDown(boolean p0) {}
    public void setIgnoreCheekPress() {}
    public void setIsLaidOutInScreen(boolean p0) {}
    public void setLayoutInScreenEnabled(boolean p0) { huskFill.put("LayoutInScreenEnabled", Boolean.valueOf(p0)); }
    public void setLayoutInsetDecor(boolean p0) { huskFill.put("LayoutInsetDecor", Boolean.valueOf(p0)); }
    protected void setShowing(boolean p0) {}
    protected void setTransitioningToDismiss(boolean p0) { huskFill.put("TransitioningToDismiss", Boolean.valueOf(p0)); }
    protected void update(android.view.View p0, android.view.WindowManager.LayoutParams p1) {}
    protected void updateAboveAnchor(boolean p0) {}
    // ---- end of generated members
}
