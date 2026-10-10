package husk;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.*;
import android.graphics.drawable.Drawable;
import android.util.TypedValue;
import android.view.*;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * A window's decor: the window background, the status and navigation bar areas (painted in their colours unless the app draws there
 * itself), an action bar when the theme has one, and the content frame (android.R.id.content). Its events go to the window's callback.
 */
public final class DecorView extends FrameLayout {
    private final Window mWindow;
    private Drawable mBackground;
    private LinearLayout mStack;
    private FrameLayout mContent;
    private TextView mTitle;
    private View mActionBar;
    private boolean mCloseOnTouchOutside;
    private int mInsetTop, mInsetBottom, mInsetLeft, mInsetRight;
    private final Paint mBarPaint = new Paint();
    private static WindowInsetsController sFallback;
    private TypedValue mMinWidthMajor, mMinWidthMinor;
    private static TypedValue copy(TypedValue v) { if (v == null) return null; TypedValue c = new TypedValue(); c.type = v.type; c.data = v.data; return c; }

    public DecorView(Context c, Window w) {
        super(c);
        mWindow = w;
        setWillNotDraw(false);
        TypedArray a = c.obtainStyledAttributes(S.Window);
        mCloseOnTouchOutside = a.getBoolean(S.Window_windowCloseOnTouchOutside, false);
        mMinWidthMajor = copy(a.peekValue(S.Window_windowMinWidthMajor));
        mMinWidthMinor = copy(a.peekValue(S.Window_windowMinWidthMinor));
        a.recycle();
    }
    public static WindowInsetsController fallbackInsetsController() {
        if (sFallback == null) sFallback = new WindowInsetsController() {
            public void show(int t) {} public void hide(int t) {} public void setSystemBarsAppearance(int a, int m) {} public int getSystemBarsAppearance() { return 0; }
            public void setSystemBarsBehavior(int b) {} public int getSystemBarsBehavior() { return 0; }
        };
        return sFallback;
    }
    public void setCloseOnTouchOutside(boolean c) { mCloseOnTouchOutside = c; }
    public boolean closeOnTouchOutside() { return mCloseOnTouchOutside; }
    /** The window background; its padding (a Material dialog's InsetDrawable: the card inset from the window's edges) keeps the
     *  content inside it, as View.setBackground does on Android. */
    public void setWindowBackground(Drawable d) {
        mBackground = d;
        Rect p = new Rect();
        if (d != null) { d.setCallback(this); if (!d.getPadding(p)) p.setEmpty(); }
        setPadding(p.left, p.top, p.right, p.bottom);
        invalidate();
        requestLayout();
    }
    @Override protected boolean verifyDrawable(Drawable who) { return who == mBackground || super.verifyDrawable(who); }

    /** The views between the decor and the content: an action bar when asked for, then the content frame. */
    public FrameLayout makeContent(boolean actionBar, CharSequence title) {
        Context c = getContext();
        mStack = new LinearLayout(c);
        mStack.setOrientation(LinearLayout.VERTICAL);
        if (actionBar) {
            TypedValue v = new TypedValue();
            int h = c.getTheme().resolveAttribute(android.R.attr.actionBarSize, v, true) ? TypedValue.complexToDimensionPixelSize(v.data, getResources().getDisplayMetrics()) : (int) (56 * Native.density());
            int bg = c.getTheme().resolveAttribute(android.R.attr.colorPrimary, v, true) && v.type >= TypedValue.TYPE_FIRST_INT ? v.data : 0xFF212121;
            mTitle = new TextView(c);
            mTitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 20);
            mTitle.setTextColor(Color.luminance(bg) > 0.5f ? 0xDE000000 : 0xFFFFFFFF);
            mTitle.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
            mTitle.setGravity(Gravity.CENTER_VERTICAL);
            mTitle.setSingleLine(true);
            mTitle.setPadding((int) (16 * Native.density()), 0, (int) (16 * Native.density()), 0);
            mTitle.setText(title != null ? title : "");
            mTitle.setBackgroundColor(bg);
            mActionBar = mTitle;
            mStack.addView(mTitle, new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, h));
        }
        mContent = new FrameLayout(c);
        mContent.setId(android.R.id.content);
        mStack.addView(mContent, new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT, 0, 1f));
        addView(mStack, new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        return mContent;
    }
    public void setTitle(CharSequence t) { if (mTitle != null) mTitle.setText(t); }
    public View actionBarView() { return mActionBar; }
    public void showActionBar(boolean show) { if (mActionBar != null) mActionBar.setVisibility(show ? VISIBLE : GONE); }

    /** A floating window (a dialog) is at least the theme's minimum width: a fraction of the screen's, or a size. */
    @Override protected void onMeasure(int ws, int hs) {
        if (mWindow.isFloating() && MeasureSpec.getMode(ws) == MeasureSpec.AT_MOST) {
            int sw = Native.screenWidth(), sh = Native.screenHeight();
            TypedValue tv = sw < sh ? mMinWidthMinor : mMinWidthMajor;
            int min = 0;
            if (tv != null) {
                if (tv.type == TypedValue.TYPE_DIMENSION) min = (int) tv.getDimension(getResources().getDisplayMetrics());
                else if (tv.type == TypedValue.TYPE_FRACTION) min = (int) tv.getFraction(sw, sw);
            }
            super.onMeasure(ws, hs);
            if (min > 0 && getMeasuredWidth() < min) super.onMeasure(MeasureSpec.makeMeasureSpec(Math.min(min, MeasureSpec.getSize(ws)), MeasureSpec.EXACTLY), hs);
            return;
        }
        super.onMeasure(ws, hs);
    }

    // ---- insets: the decor keeps its content off the bars unless the app lays out under them
    @Override public WindowInsets dispatchApplyWindowInsets(WindowInsets in) {
        boolean fits = mWindow.huskDecorFits() && !mWindow.isFloating();
        mInsetLeft = in.getSystemWindowInsetLeft(); mInsetTop = mWindow.huskFullscreen() ? 0 : in.getSystemWindowInsetTop();
        mInsetRight = in.getSystemWindowInsetRight(); mInsetBottom = in.getSystemWindowInsetBottom();
        if (fits) {
            if (mStack != null) {
                FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams) mStack.getLayoutParams();
                lp.setMargins(mInsetLeft, mInsetTop, mInsetRight, mInsetBottom);
                mStack.setLayoutParams(lp);
            }
            WindowInsets rest = in.consumeSystemWindowInsets();
            super.dispatchApplyWindowInsets(rest);
            return rest;
        }
        if (mStack != null) { FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams) mStack.getLayoutParams(); lp.setMargins(0, 0, 0, 0); mStack.setLayoutParams(lp); }
        mInsetTop = mInsetBottom = mInsetLeft = mInsetRight = 0;
        return super.dispatchApplyWindowInsets(in);
    }
    @Override public void draw(Canvas c) {
        if (mBackground != null) {
            mBackground.setBounds(0, 0, getWidth(), getHeight());
            mBackground.draw(c);
        }
        super.draw(c);
        // the bars' own colours over the areas the content was kept out of
        if (mInsetTop > 0 && !mWindow.isFloating()) { mBarPaint.setColor(mWindow.getStatusBarColor()); c.drawRect(0, 0, getWidth(), mInsetTop, mBarPaint); }
        if (mInsetBottom > 0 && !mWindow.isFloating()) { mBarPaint.setColor(mWindow.getNavigationBarColor()); c.drawRect(0, getHeight() - mInsetBottom, getWidth(), getHeight(), mBarPaint); }
    }

    // ---- events
    @Override public boolean dispatchTouchEvent(MotionEvent e) {
        Window.Callback cb = mWindow.getCallback();
        if (e.getActionMasked() == MotionEvent.ACTION_OUTSIDE || (mCloseOnTouchOutside && e.getActionMasked() == MotionEvent.ACTION_DOWN && outside(e))) {
            if (cb != null && !mWindow.isDestroyed()) cb.dispatchTouchEvent(e);
            return true;
        }
        return cb != null && !mWindow.isDestroyed() ? cb.dispatchTouchEvent(e) : super.dispatchTouchEvent(e);
    }
    private boolean outside(MotionEvent e) { return e.getX() < 0 || e.getY() < 0 || e.getX() >= getWidth() || e.getY() >= getHeight(); }
    @Override public boolean dispatchKeyEvent(KeyEvent e) {
        Window.Callback cb = mWindow.getCallback();
        return cb != null && !mWindow.isDestroyed() ? cb.dispatchKeyEvent(e) : super.dispatchKeyEvent(e);
    }
    @Override public boolean dispatchGenericMotionEvent(MotionEvent e) {
        Window.Callback cb = mWindow.getCallback();
        return cb != null && !mWindow.isDestroyed() ? cb.dispatchGenericMotionEvent(e) : super.dispatchGenericMotionEvent(e);
    }
    public boolean superDispatchTouchEvent(MotionEvent e) { return super.dispatchTouchEvent(e); }
    public boolean superDispatchKeyEvent(KeyEvent e) {
        if (super.dispatchKeyEvent(e)) return true;
        return false;
    }
    public boolean superDispatchGenericMotionEvent(MotionEvent e) { return super.dispatchGenericMotionEvent(e); }
    @Override public void onWindowFocusChanged(boolean f) { super.onWindowFocusChanged(f); Window.Callback cb = mWindow.getCallback(); if (cb != null && !mWindow.isDestroyed()) cb.onWindowFocusChanged(f); }
    @Override protected void onAttachedToWindow() { super.onAttachedToWindow(); Window.Callback cb = mWindow.getCallback(); if (cb != null && !mWindow.isDestroyed()) cb.onAttachedToWindow(); }
    @Override protected void onDetachedFromWindow() { super.onDetachedFromWindow(); Window.Callback cb = mWindow.getCallback(); if (cb != null) cb.onDetachedFromWindow(); }
    public Window getWindowHusk() { return mWindow; }
}
