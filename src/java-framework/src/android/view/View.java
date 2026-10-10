package android.view;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;

public class View {
    public static final int VISIBLE = 0, INVISIBLE = 4, GONE = 8, NO_ID = -1, FOCUSABLE = 1;
    public static final int SYSTEM_UI_FLAG_VISIBLE = 0, SYSTEM_UI_FLAG_LOW_PROFILE = 1, SYSTEM_UI_FLAG_HIDE_NAVIGATION = 2, SYSTEM_UI_FLAG_FULLSCREEN = 4,
        SYSTEM_UI_FLAG_LAYOUT_STABLE = 256, SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION = 512, SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN = 1024,
        SYSTEM_UI_FLAG_IMMERSIVE = 2048, SYSTEM_UI_FLAG_IMMERSIVE_STICKY = 4096, STATUS_BAR_HIDDEN = 1;
    public static final int LAYER_TYPE_NONE = 0, LAYER_TYPE_SOFTWARE = 1, LAYER_TYPE_HARDWARE = 2, HAPTIC_FEEDBACK_ENABLED = 1;
    public static final int MEASURED_STATE_MASK = 0xff000000;

    public interface OnTouchListener { boolean onTouch(View v, MotionEvent e); }
    public interface OnKeyListener { boolean onKey(View v, int code, KeyEvent e); }
    public interface OnClickListener { void onClick(View v); }
    public interface OnLongClickListener { boolean onLongClick(View v); }
    public interface OnGenericMotionListener { boolean onGenericMotion(View v, MotionEvent e); }
    public interface OnFocusChangeListener { void onFocusChange(View v, boolean focus); }
    public interface OnSystemUiVisibilityChangeListener { void onSystemUiVisibilityChange(int v); }
    public interface OnLayoutChangeListener { void onLayoutChange(View v, int l, int t, int r, int b, int ol, int ot, int or, int ob); }
    public interface OnApplyWindowInsetsListener { WindowInsets onApplyWindowInsets(View v, WindowInsets i); }
    public interface OnAttachStateChangeListener { void onViewAttachedToWindow(View v); void onViewDetachedFromWindow(View v); }
    public static class MeasureSpec {
        public static final int UNSPECIFIED = 0, EXACTLY = 0x40000000, AT_MOST = 0x80000000;
        public static int makeMeasureSpec(int size, int mode) { return (size & 0x3fffffff) | mode; }
        public static int getMode(int spec) { return spec & 0xc0000000; }
        public static int getSize(int spec) { return spec & 0x3fffffff; }
    }

    protected final Context mContext;
    private ViewGroup.LayoutParams mLayout;
    private OnTouchListener mTouch;
    private OnKeyListener mKey;
    private OnClickListener mClick;
    private OnGenericMotionListener mGeneric;
    private ViewTreeObserver mObserver;
    private int mLeft, mTop, mRight, mBottom, mVisibility, mId = NO_ID, mSystemUi;
    private boolean mFocusable, mAttached, mKeepScreenOn;
    ViewGroup mParent;
    private Object mTag;
    private int mMeasuredW, mMeasuredH;

    public View(Context c) { mContext = c; }
    public View(Context c, android.util.AttributeSet a) { this(c); }
    public View(Context c, android.util.AttributeSet a, int style) { this(c); }

    /** Husk: the view is on screen at this size. */
    public void huskAttach(int w, int h) {
        boolean first = !mAttached;
        mAttached = true;
        mMeasuredW = w; mMeasuredH = h;
        int ow = getWidth(), oh = getHeight();
        mLeft = 0; mTop = 0; mRight = w; mBottom = h;
        if (first) onAttachedToWindow();
        onMeasure(MeasureSpec.makeMeasureSpec(w, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(h, MeasureSpec.EXACTLY));
        if (ow != w || oh != h) onSizeChanged(w, h, ow, oh);
        onLayout(true, 0, 0, w, h);
        if (mObserver != null) mObserver.dispatchGlobalLayout();
    }

    public Context getContext() { return mContext; }
    public Handler getHandler() { return new Handler(Looper.getMainLooper()); }
    public boolean post(Runnable r) { return getHandler().post(r); }
    public boolean postDelayed(Runnable r, long ms) { return getHandler().postDelayed(r, ms); }
    public boolean removeCallbacks(Runnable r) { return true; }
    public void postInvalidate() {}
    public void invalidate() {}
    public void requestLayout() {}
    public void forceLayout() {}
    public final int getWidth() { return mRight - mLeft; }
    public final int getHeight() { return mBottom - mTop; }
    public final int getLeft() { return mLeft; }
    public final int getTop() { return mTop; }
    public final int getRight() { return mRight; }
    public final int getBottom() { return mBottom; }
    public final int getMeasuredWidth() { return mMeasuredW; }
    public final int getMeasuredHeight() { return mMeasuredH; }
    protected final void setMeasuredDimension(int w, int h) { mMeasuredW = w; mMeasuredH = h; }
    protected void onMeasure(int ws, int hs) { setMeasuredDimension(MeasureSpec.getSize(ws), MeasureSpec.getSize(hs)); }
    protected void onLayout(boolean changed, int l, int t, int r, int b) {}
    protected void onSizeChanged(int w, int h, int ow, int oh) {}
    protected void onAttachedToWindow() {}
    protected void onDetachedFromWindow() {}
    public void onWindowFocusChanged(boolean f) {}
    protected void onFocusChanged(boolean f, int dir, android.graphics.Rect r) {}
    public void measure(int ws, int hs) { onMeasure(ws, hs); }
    public void layout(int l, int t, int r, int b) { mLeft = l; mTop = t; mRight = r; mBottom = b; onLayout(true, l, t, r, b); }
    public int getId() { return mId; }
    public void setId(int id) { mId = id; }
    public View findViewById(int id) { return id == mId ? this : null; }
    public Object getTag() { return mTag; }
    public void setTag(Object t) { mTag = t; }
    public ViewGroup.LayoutParams getLayoutParams() { return mLayout; }
    public void setLayoutParams(ViewGroup.LayoutParams p) { mLayout = p; }
    public ViewParent getParent() { return mParent; }
    public View getRootView() { View v = this; while (v.mParent != null) v = v.mParent; return v; }
    public void setVisibility(int v) { mVisibility = v; }
    public int getVisibility() { return mVisibility; }
    public boolean isShown() { return mVisibility == VISIBLE && mAttached; }
    public boolean isAttachedToWindow() { return mAttached; }
    public boolean isInEditMode() { return false; }
    public boolean isHardwareAccelerated() { return true; }
    public void setLayerType(int t, Object paint) {}
    public void setFocusable(boolean f) { mFocusable = f; }
    public void setFocusable(int f) { mFocusable = f != 0; }
    public void setFocusableInTouchMode(boolean f) { mFocusable |= f; }
    public boolean isFocusable() { return mFocusable; }
    public boolean requestFocus() { return true; }
    public boolean requestFocus(int dir) { return true; }
    public boolean hasFocus() { return true; }
    public boolean isFocused() { return true; }
    public void clearFocus() {}
    public void setKeepScreenOn(boolean b) { mKeepScreenOn = b; }
    public boolean getKeepScreenOn() { return mKeepScreenOn; }
    public void setSystemUiVisibility(int v) { mSystemUi = v; }
    public int getSystemUiVisibility() { return mSystemUi; }
    public int getWindowSystemUiVisibility() { return mSystemUi; }
    public void setOnSystemUiVisibilityChangeListener(OnSystemUiVisibilityChangeListener l) {}
    public void setOnApplyWindowInsetsListener(OnApplyWindowInsetsListener l) {}
    public WindowInsets getRootWindowInsets() { return new WindowInsets(); }
    public void addOnLayoutChangeListener(OnLayoutChangeListener l) {}
    public void removeOnLayoutChangeListener(OnLayoutChangeListener l) {}
    public void addOnAttachStateChangeListener(OnAttachStateChangeListener l) {}
    public void setOnTouchListener(OnTouchListener l) { mTouch = l; }
    public void setOnKeyListener(OnKeyListener l) { mKey = l; }
    public void setOnClickListener(OnClickListener l) { mClick = l; }
    public void setOnLongClickListener(OnLongClickListener l) {}
    public void setOnGenericMotionListener(OnGenericMotionListener l) { mGeneric = l; }
    public void setOnFocusChangeListener(OnFocusChangeListener l) {}
    public void setOnHoverListener(Object l) {}
    public void setOnCapturedPointerListener(Object l) {}
    public void setClickable(boolean b) {}
    public void setLongClickable(boolean b) {}
    public void setHapticFeedbackEnabled(boolean b) {}
    public boolean performHapticFeedback(int c) { return true; }
    public boolean performClick() { if (mClick != null) { mClick.onClick(this); return true; } return false; }
    public void setBackgroundColor(int c) {}
    public void setBackground(Object d) {}
    public void setBackgroundResource(int r) {}
    public void setAlpha(float a) {}
    public void setPadding(int l, int t, int r, int b) {}
    public void setContentDescription(CharSequence c) {}
    public void setDefaultFocusHighlightEnabled(boolean b) {}
    public void setPointerIcon(Object p) {}
    public void releasePointerCapture() {}
    public void requestPointerCapture() {}
    public IBinderToken getWindowToken() { return new IBinderToken(); }
    public static final class IBinderToken implements android.os.IBinder {}
    public Display getDisplay() { return new Display(); }
    public ViewTreeObserver getViewTreeObserver() { if (mObserver == null) mObserver = new ViewTreeObserver(); return mObserver; }
    public void getLocationOnScreen(int[] out) { out[0] = mLeft; out[1] = mTop; }
    public void getLocationInWindow(int[] out) { out[0] = mLeft; out[1] = mTop; }
    public void getWindowVisibleDisplayFrame(android.graphics.Rect r) { r.set(0, 0, husk.Native.screenWidth(), husk.Native.screenHeight()); }
    public android.view.inputmethod.InputConnection onCreateInputConnection(android.view.inputmethod.EditorInfo o) { return null; }
    public boolean onCheckIsTextEditor() { return false; }
    public boolean onTouchEvent(MotionEvent e) { return false; }
    public boolean onGenericMotionEvent(MotionEvent e) { return false; }
    public boolean onKeyDown(int code, KeyEvent e) { return false; }
    public boolean onKeyUp(int code, KeyEvent e) { return false; }
    public boolean onKeyMultiple(int code, int n, KeyEvent e) { return false; }
    public boolean onKeyPreIme(int code, KeyEvent e) { return false; }
    public boolean dispatchTouchEvent(MotionEvent e) { if (mTouch != null && mTouch.onTouch(this, e)) return true; return onTouchEvent(e); }
    public boolean dispatchGenericMotionEvent(MotionEvent e) { if (mGeneric != null && mGeneric.onGenericMotion(this, e)) return true; return onGenericMotionEvent(e); }
    public boolean dispatchKeyEvent(KeyEvent e) {
        if (mKey != null && mKey.onKey(this, e.getKeyCode(), e)) return true;
        return e.getAction() == KeyEvent.ACTION_DOWN ? onKeyDown(e.getKeyCode(), e) : e.getAction() == KeyEvent.ACTION_UP ? onKeyUp(e.getKeyCode(), e) : onKeyMultiple(e.getKeyCode(), e.getRepeatCount(), e);
    }
}
