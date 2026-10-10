package husk;

import android.graphics.*;
import android.os.Handler;
import android.os.Looper;
import android.view.*;
import java.util.ArrayList;

/**
 * A window's root, as ViewRootImpl: it lays out its view tree to the window's size, and with the other windows draws it into the
 * screen's one bitmap, which the render thread puts over the app's GL (husk.Gfx.present). Touches go to the topmost window that takes
 * them, keys to the topmost focusable one. Everything here runs on the main thread.
 */
public final class ViewRoot implements ViewParent {
    private static final ArrayList<ViewRoot> sRoots = new ArrayList<>();
    private static Bitmap sScreen;
    private static Canvas sCanvas;
    private static boolean sScheduled, sPresentedAny;
    private static Handler sMain;
    private static int sNextViewId = 0x00100000;
    private static int[] sInsets = { 0, 0, 0, 0 };     /* left, top, right, bottom: the notch / status bar and home indicator areas */
    private static int sImeBottom;
    private static ViewRoot sTouchTarget;

    final View mView;
    WindowManager.LayoutParams mLp;
    private int mX, mY, mW, mH;
    private boolean mDumpTimer;
    private boolean mLayoutRequested = true, mFirst = true, mRemoved, mHasFocus = true, mInLayout, mInsetsDirty = true;
    private View mFocused;
    private final ViewTreeObserver mObserver = new ViewTreeObserver();
    private final android.os.IBinder mToken = new android.os.Binder();
    private WindowInsets mWindowInsets;
    private int mSystemUi;
    private final KeyEvent.DispatcherState mKeyState = new KeyEvent.DispatcherState();
    private WindowInsetsController mInsetsController;

    private ViewRoot(View v, WindowManager.LayoutParams lp) { mView = v; mLp = new WindowManager.LayoutParams(); mLp.copyFrom(lp); }

    public static Handler mainHandler() { if (sMain == null) sMain = new Handler(Looper.getMainLooper()); return sMain; }
    public static synchronized int nextViewId() { return sNextViewId++; }
    public static ViewRoot of(View v) { for (ViewRoot r : sRoots) if (r.mView == v) return r; return null; }
    public static ArrayList<ViewRoot> roots() { return sRoots; }
    public View getView() { return mView; }

    /** A new window on top of the others. */
    public static ViewRoot add(View v, WindowManager.LayoutParams lp) {
        ViewRoot r = new ViewRoot(v, lp);
        // An activity's own window goes under the windows it already opened (a dialog shown from onCreate is added before the
        // activity's window, which comes after onResume): on Android a dialog stays above the activity it belongs to.
        int at = sRoots.size();
        android.app.Activity owner = lp != null && lp.type == WindowManager.LayoutParams.TYPE_BASE_APPLICATION ? activityOf(v.getContext()) : null;
        if (owner != null) for (int i = 0; i < sRoots.size(); i++) if (activityOf(sRoots.get(i).mView.getContext()) == owner) { at = i; break; }
        boolean under = at < sRoots.size();
        sRoots.add(at, r);
        if (under) for (int i = at + 1; i < sRoots.size(); i++) if (sRoots.get(i).focusable()) { r.mHasFocus = false; break; }
        if (v.getLayoutParams() == null) v.setLayoutParams(r.mLp);
        r.attach();
        android.webkit.WebView.huskWindowsChanged();
        if (r.mHasFocus) for (ViewRoot o : sRoots) if (o != r && o.mHasFocus && r.focusable()) { o.mHasFocus = false; o.mView.dispatchWindowFocusChanged(false); o.mObserver.huskWindowFocus(false); }
        scheduleFrame();
        return r;
    }
    private void attach() {
        mView.huskAttach(this);
        mObserver.huskAttached(true);
    }
    public void remove() {
        if (mRemoved) return;
        mRemoved = true;
        boolean hadFocus = mHasFocus;
        sRoots.remove(this);
        if (sTouchTarget == this) sTouchTarget = null;
        mView.huskDetach();
        android.webkit.WebView.huskWindowsChanged();
        mObserver.huskAttached(false);
        if (hadFocus) {
            for (int i = sRoots.size() - 1; i >= 0; i--) {
                ViewRoot o = sRoots.get(i);
                if (o.focusable()) { if (!o.mHasFocus) { o.mHasFocus = true; o.mView.dispatchWindowFocusChanged(true); o.mObserver.huskWindowFocus(true); } break; }
            }
        }
        scheduleFrame();
    }
    public void setLayoutParams(WindowManager.LayoutParams lp) { mLp.copyFrom(lp); mView.setLayoutParams(mLp); mLayoutRequested = true; scheduleFrame(); }
    public WindowManager.LayoutParams layoutParams() { return mLp; }
    private boolean focusable() { return (mLp.flags & WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE) == 0; }

    // ---- what views ask of their root
    public ViewTreeObserver observer() { return mObserver; }
    public Handler handler() { return mainHandler(); }
    public android.os.IBinder token() { return mToken; }
    public View focused() { return mFocused; }
    public void setFocused(View v) {
        View old = mFocused;
        mFocused = v;
        if (old != v) InputMethods.focusChanged(old, v);
    }
    public boolean hasWindowFocus() { return mHasFocus; }
    public boolean isInLayout() { return mInLayout; }
    public int width() { return mW; }
    public int height() { return mH; }
    public int windowX() { return mX; }
    public int windowY() { return mY; }
    public KeyEvent.DispatcherState keyDispatcherState() { return mKeyState; }
    public void keepScreenOn(boolean on) {}
    public void invalidate() { scheduleFrame(); }
    public void invalidateFromAnyThread() { if (Looper.myLooper() == Looper.getMainLooper()) scheduleFrame(); else mainHandler().post(ViewRoot::scheduleFrame); }
    public void requestLayout() { mLayoutRequested = true; scheduleFrame(); }
    public void requestApplyInsets() { mInsetsDirty = true; mLayoutRequested = true; scheduleFrame(); }
    public WindowInsets insets() { return mWindowInsets; }
    public int systemUi() { return mSystemUi; }
    public void systemUiChanged(int v) { mSystemUi = v; requestApplyInsets(); }
    public void visibleDisplayFrame(Rect r) { r.set(sInsets[0], sInsets[1], screenW() - sInsets[2], screenH() - Math.max(sInsets[3], sImeBottom)); }
    public WindowInsetsController insetsController() {
        if (mInsetsController == null) mInsetsController = new WindowInsetsController() {
            int appearance, behavior;
            public void show(int types) { if ((types & WindowInsets.Type.ime()) != 0 && mFocused != null) InputMethods.show(mFocused); }
            public void hide(int types) { if ((types & WindowInsets.Type.ime()) != 0) InputMethods.hide(); }
            public void setSystemBarsAppearance(int a, int mask) { appearance = (appearance & ~mask) | (a & mask); }
            public int getSystemBarsAppearance() { return appearance; }
            public void setSystemBarsBehavior(int b) { behavior = b; }
            public int getSystemBarsBehavior() { return behavior; }
        };
        return mInsetsController;
    }

    // ---- insets from the host
    /** The screen's insets in pixels (Husk's host tells them), and the keyboard's height. */
    public static void setInsets(int l, int t, int r, int b) { sInsets = new int[] { l, t, r, b }; for (ViewRoot v : sRoots) v.requestApplyInsets(); }
    public static void setImeHeight(int h) { if (h != sImeBottom) { sImeBottom = h; for (ViewRoot v : sRoots) v.requestApplyInsets(); } }
    public static int imeHeight() { return sImeBottom; }
    public static int[] screenInsetsHusk() { return sInsets; }
    public static WindowInsets screenInsets() { return WindowInsets.huskOf(sInsets[0], sInsets[1], sInsets[2], sInsets[3], sImeBottom, cutout()); }
    public static DisplayCutout cutout() { return sInsets[1] > 0 ? new DisplayCutout(new Rect(0, sInsets[1], 0, 0), null) : null; }
    static int screenW() { return Native.screenWidth(); }
    static int screenH() { return Native.screenHeight(); }

    // ---- the frame
    public static void scheduleFrame() {
        if (sScheduled) return;
        sScheduled = true;
        Choreographer.getMainThreadInstance().postCallback(Choreographer.CALLBACK_TRAVERSAL, ViewRoot::doFrame, null);
    }
    private static void doFrame() {
        sScheduled = false;
        ArrayList<ViewRoot> roots = new ArrayList<>(sRoots);
        for (ViewRoot r : roots) if (!r.mRemoved) r.performLayout();
        for (ViewRoot r : roots) if (!r.mRemoved && r.mObserver.dispatchOnPreDraw()) { scheduleFrame(); }
        drawAll(roots);
        for (ViewRoot r : roots) if (!r.mRemoved) r.mObserver.dispatchOnDraw();
    }
    private static Boolean sDump;
    private long mLastDump;
    /** TL_VIEW_DUMP: the window's views, with their frames, to the log (every 3 s while it lays out). */
    private static void dump(View v, int depth) {
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < depth; i++) b.append("  ");
        b.append(v.getClass().getName()).append(' ').append(v.getLeft()).append(',').append(v.getTop()).append(' ').append(v.getWidth()).append('x').append(v.getHeight());
        if (v.getVisibility() != View.VISIBLE) b.append(v.getVisibility() == View.GONE ? " GONE" : " INVISIBLE");
        if (v.getId() != View.NO_ID) b.append(" #").append(Integer.toHexString(v.getId()));
        if (v instanceof android.widget.TextView) {
            CharSequence tx = ((android.widget.TextView) v).getText();
            StringBuilder e = new StringBuilder();
            for (int i = 0; tx != null && i < tx.length(); i++) { char ch = tx.charAt(i); if (ch < 32 || ch > 126) e.append(String.format("\\u%04x", (int) ch)); else e.append(ch); }
            b.append(" \"").append(e).append('"');
        }
        android.view.ViewGroup.LayoutParams lp = v.getLayoutParams();
        if (lp != null) {
            b.append(" lp=").append(lp.getClass().getSimpleName()).append('(').append(lp.width).append(',').append(lp.height).append(')');
            for (String f : new String[] { "topToTop", "topToBottom", "bottomToBottom", "leftToLeft", "startToStart", "endToEnd", "rightToRight", "horizontalBias", "dimensionRatio" }) {
                try { java.lang.reflect.Field fl = lp.getClass().getField(f); Object o = fl.get(lp); if (o != null && !"-1".equals(String.valueOf(o)) && !"0.5".equals(String.valueOf(o))) b.append(' ').append(f).append('=').append(o); } catch (Exception e) {}
            }
        }
        android.util.Log.d("ViewDump", b.toString());
        if (v instanceof android.view.ViewGroup) { android.view.ViewGroup g = (android.view.ViewGroup) v; for (int i = 0; i < g.getChildCount(); i++) dump(g.getChildAt(i), depth + 1); }
    }
    private void performLayout() {
        int sw = screenW(), sh = screenH();
        if (mInsetsDirty || mFirst) {
            mInsetsDirty = false;
            boolean full = mLp.type == WindowManager.LayoutParams.TYPE_BASE_APPLICATION || mLp.type == WindowManager.LayoutParams.TYPE_APPLICATION;
            mWindowInsets = full ? screenInsets() : WindowInsets.huskOf(0, 0, 0, 0, Math.max(0, sImeBottom - (sh - mY - mH)), null);
            mView.dispatchApplyWindowInsets(mWindowInsets);
            mLayoutRequested = true;
        }
        if (!mLayoutRequested && !mView.isLayoutRequested()) return;
        mLayoutRequested = false;
        mInLayout = true;
        try {
            int ws, hs;
            int lw = mLp.width, lh = mLp.height;
            int maxW = sw, maxH = sh;
            if (lw == ViewGroup.LayoutParams.MATCH_PARENT) ws = View.MeasureSpec.makeMeasureSpec(maxW, View.MeasureSpec.EXACTLY);
            else if (lw == ViewGroup.LayoutParams.WRAP_CONTENT) ws = View.MeasureSpec.makeMeasureSpec(maxW, View.MeasureSpec.AT_MOST);
            else ws = View.MeasureSpec.makeMeasureSpec(lw, View.MeasureSpec.EXACTLY);
            if (lh == ViewGroup.LayoutParams.MATCH_PARENT) hs = View.MeasureSpec.makeMeasureSpec(maxH, View.MeasureSpec.EXACTLY);
            else if (lh == ViewGroup.LayoutParams.WRAP_CONTENT) hs = View.MeasureSpec.makeMeasureSpec(maxH, View.MeasureSpec.AT_MOST);
            else hs = View.MeasureSpec.makeMeasureSpec(lh, View.MeasureSpec.EXACTLY);
            mView.measure(ws, hs);
            int w = mView.getMeasuredWidth(), h = mView.getMeasuredHeight();
            // where the window goes: gravity and offsets for windows that do not fill the screen (dialogs, popups, toasts)
            int g = mLp.gravity;
            Rect out = new Rect();
            if (g == 0 && (lw != ViewGroup.LayoutParams.MATCH_PARENT || lh != ViewGroup.LayoutParams.MATCH_PARENT)) g = Gravity.TOP | Gravity.LEFT;
            if (g == 0) out.set(0, 0, w, h);
            else {
                Rect area = new Rect(0, 0, sw, sh);
                if (mLp.type >= 1000 || (mLp.flags & WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS) == 0 && mLp.type != WindowManager.LayoutParams.TYPE_BASE_APPLICATION)
                    area.set(sInsets[0], sInsets[1], sw - sInsets[2], sh - Math.max(sInsets[3], sImeBottom));
                Gravity.apply(g, w, h, area, mLp.x, mLp.y, out);
            }
            mX = out.left; mY = out.top; mW = w; mH = h;
            mView.layout(0, 0, w, h);
        } finally { mInLayout = false; }
        mObserver.dispatchOnGlobalLayout();
        if (sDump == null) sDump = System.getenv("TL_VIEW_DUMP") != null;
        if (sDump) {
            long now = android.os.SystemClock.uptimeMillis();
            if (now - mLastDump > 3000) { mLastDump = now; dump(mView, 0); }
            if (!mDumpTimer) {                  /* and again every 4 s, laid out again or not: a window that never relayouts shows why */
                mDumpTimer = true;
                final Runnable[] again = new Runnable[1];
                again[0] = () -> { if (mRemoved) return; android.util.Log.d("ViewDump", "---- " + mView.getClass().getName() + " (timer; layout requested " + mView.isLayoutRequested() + ")"); dump(mView, 0); mainHandler().postDelayed(again[0], 4000); };
                mainHandler().postDelayed(again[0], 4000);
            }
        }
        if (mFirst) {
            mFirst = false;
            if (mHasFocus) { mView.dispatchWindowFocusChanged(true); mObserver.huskWindowFocus(true); }
            if (mView.findFocus() == null && mView instanceof ViewGroup) { /* the first focusable view takes focus when it asks; nothing forced */ }
        }
    }
    private static void drawAll(ArrayList<ViewRoot> roots) {
        int sw = screenW(), sh = screenH();
        if (sScreen == null || sScreen.getWidth() != sw || sScreen.getHeight() != sh) {
            if (sCanvas != null) sCanvas.huskRelease();
            sScreen = Bitmap.createBitmap(sw, sh, Bitmap.Config.ARGB_8888);
            sCanvas = new Canvas(sScreen);
        }
        sScreen.eraseColor(0);
        int gen = sScreen.getGenerationId();
        Paint dim = null;
        for (ViewRoot r : roots) {
            if (r.mRemoved || r.mView.getVisibility() != View.VISIBLE) continue;
            if ((r.mLp.flags & WindowManager.LayoutParams.FLAG_DIM_BEHIND) != 0 && r.mLp.dimAmount > 0) {
                if (dim == null) dim = new Paint();
                dim.setColor(Color.argb(Math.round(Math.min(1, r.mLp.dimAmount) * 0.6f * 255), 0, 0, 0));
                sCanvas.drawRect(0, 0, sw, sh, dim);
            }
            int save = sCanvas.save();
            sCanvas.translate(r.mX, r.mY);
            sCanvas.clipRect(0, 0, r.mW, r.mH);
            if (r.mLp.alpha < 1f) sCanvas.saveLayerAlpha(0, 0, r.mW, r.mH, Math.round(r.mLp.alpha * 255));
            try {
                r.mView.draw(sCanvas);
            } catch (RuntimeException e) {
                android.util.Log.e("ViewRoot", "drawing the window threw", e);
            }
            sCanvas.restoreToCount(save);
        }
        boolean any = sScreen.getGenerationId() != gen;
        if (any || sPresentedAny) Gfx.present(any ? sScreen.huskNative() : 0);
        sPresentedAny = any;
    }

    // ---- input
    /** A touch in screen coordinates, to the window it is for. */
    public static boolean dispatchTouch(MotionEvent e) {
        int am = e.getActionMasked();
        if (am == MotionEvent.ACTION_DOWN || sTouchTarget == null || sTouchTarget.mRemoved) {
            sTouchTarget = null;
            float x = e.getRawX(), y = e.getRawY();
            for (int i = sRoots.size() - 1; i >= 0; i--) {
                ViewRoot r = sRoots.get(i);
                if (r.mView.getVisibility() != View.VISIBLE || (r.mLp.flags & WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE) != 0) continue;
                boolean inside = x >= r.mX && y >= r.mY && x < r.mX + r.mW && y < r.mY + r.mH;
                boolean modal = (r.mLp.flags & WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL) == 0;
                if (inside || modal) {
                    if (!inside && (r.mLp.flags & WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH) != 0) {
                        MotionEvent o = MotionEvent.obtain(e); o.setAction(MotionEvent.ACTION_OUTSIDE); o.offsetLocation(-r.mX, -r.mY);
                        r.mView.dispatchTouchEvent(o);
                    }
                    sTouchTarget = r;
                    break;
                }
                if ((r.mLp.flags & WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH) != 0 && am == MotionEvent.ACTION_DOWN) {
                    MotionEvent o = MotionEvent.obtain(e); o.setAction(MotionEvent.ACTION_OUTSIDE); o.offsetLocation(-r.mX, -r.mY);
                    r.mView.dispatchTouchEvent(o);
                }
            }
            if (am != MotionEvent.ACTION_DOWN && sTouchTarget == null) return false;
        }
        ViewRoot r = sTouchTarget;
        if (r == null) return false;
        MotionEvent l = MotionEvent.obtain(e);
        l.offsetLocation(-r.mX, -r.mY);
        boolean h = r.mView.dispatchTouchEvent(l);
        if (TOUCH_LOG && am != MotionEvent.ACTION_MOVE)
            android.util.Log.d("Touch", MotionEvent.actionToString(am) + " " + l.getX() + "," + l.getY() + " -> window " + r.mView.getClass().getName() + " at " + r.mX + "," + r.mY + " " + r.mW + "x" + r.mH + ": " + (h ? "handled" : "not handled") + ", under it " + deepest(r.mView, l.getX(), l.getY()));
        if (am == MotionEvent.ACTION_UP || am == MotionEvent.ACTION_CANCEL) sTouchTarget = null;
        return h;
    }
    public static boolean dispatchKey(KeyEvent e) {
        for (int i = sRoots.size() - 1; i >= 0; i--) {
            ViewRoot r = sRoots.get(i);
            if (!r.focusable() || r.mView.getVisibility() != View.VISIBLE) continue;
            if (r.mView.dispatchKeyEventPreIme(e)) return true;
            return r.mView.dispatchKeyEvent(e);
        }
        return false;
    }
    public static boolean dispatchGeneric(MotionEvent e) {
        for (int i = sRoots.size() - 1; i >= 0; i--) {
            ViewRoot r = sRoots.get(i);
            if (r.mView.getVisibility() != View.VISIBLE) continue;
            MotionEvent l = MotionEvent.obtain(e);
            l.offsetLocation(-r.mX, -r.mY);
            return r.mView.dispatchGenericMotionEvent(l);
        }
        return false;
    }
    private static android.app.Activity activityOf(android.content.Context c) {
        while (c instanceof android.content.ContextWrapper) {
            if (c instanceof android.app.Activity) return (android.app.Activity) c;
            c = ((android.content.ContextWrapper) c).getBaseContext();
        }
        return null;
    }
    private static final boolean TOUCH_LOG = System.getenv("TL_TOUCH_LOG") != null;
    private static String deepest(View v, float x, float y) {
        String path = v.getClass().getSimpleName();
        while (v instanceof android.view.ViewGroup) {
            android.view.ViewGroup g = (android.view.ViewGroup) v;
            View hit = null;
            for (int i = g.getChildCount() - 1; i >= 0; i--) {
                View c = g.getChildAt(i);
                if (c.getVisibility() != View.VISIBLE) continue;
                float cx = x + g.getScrollX() - c.getLeft(), cy = y + g.getScrollY() - c.getTop();
                if (cx >= 0 && cy >= 0 && cx < c.getWidth() && cy < c.getHeight()) { hit = c; x = cx; y = cy; break; }
            }
            if (hit == null) break;
            v = hit;
            path += " > " + v.getClass().getSimpleName() + (v.isClickable() ? "*" : "");
        }
        return path;
    }

    /** The window touches go to: the top one that takes them (a toast does not). */
    public static ViewRoot topTouchable() {
        for (int i = sRoots.size() - 1; i >= 0; i--) if ((sRoots.get(i).mLp.flags & WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE) == 0) return sRoots.get(i);
        return null;
    }
    public static ViewRoot top() { return sRoots.isEmpty() ? null : sRoots.get(sRoots.size() - 1); }

    // ---- ViewParent (the root of a window has no parent view)
    public boolean isLayoutRequested() { return mLayoutRequested; }
    public void requestTransparentRegion(View child) {}
    public void invalidateChild(View child, Rect r) { scheduleFrame(); }
    public ViewParent invalidateChildInParent(int[] loc, Rect r) { scheduleFrame(); return null; }
    public ViewParent getParent() { return null; }
    public void requestChildFocus(View child, View focused) { setFocused(focused); }
    public void recomputeViewAttributes(View child) {}
    public void clearChildFocus(View child) { setFocused(null); }
    public boolean getChildVisibleRect(View child, Rect r, Point offset) { return r.intersect(0, 0, mW, mH); }
    public View focusSearch(View v, int dir) {
        ArrayList<View> all = new ArrayList<>();
        mView.addFocusables(all, dir);
        if (all.isEmpty()) return null;
        int i = all.indexOf(v);
        if (dir == View.FOCUS_FORWARD || dir == View.FOCUS_DOWN || dir == View.FOCUS_RIGHT) return i + 1 < all.size() ? all.get(i + 1) : null;
        return i > 0 ? all.get(i - 1) : null;
    }
    public void bringChildToFront(View child) {}
    public void focusableViewAvailable(View v) {}
    public boolean showContextMenuForChild(View original) { return false; }
    public void createContextMenu(ContextMenu menu) {}
    public ActionMode startActionModeForChild(View original, ActionMode.Callback cb) { return null; }
    public void childDrawableStateChanged(View child) {}
    public void requestDisallowInterceptTouchEvent(boolean d) {}
    public boolean requestChildRectangleOnScreen(View child, Rect r, boolean immediate) { return false; }
    public boolean requestSendAccessibilityEvent(View child, android.view.accessibility.AccessibilityEvent e) { return false; }
    public void childHasTransientStateChanged(View child, boolean has) {}
    public void requestFitSystemWindows() { requestApplyInsets(); }
    public ViewParent getParentForAccessibility() { return null; }
    public void notifySubtreeAccessibilityStateChanged(View child, View source, int type) {}
    public boolean canResolveLayoutDirection() { return true; }
    public boolean isLayoutDirectionResolved() { return true; }
    public int getLayoutDirection() { return View.LAYOUT_DIRECTION_LTR; }
    public boolean canResolveTextDirection() { return true; }
    public boolean isTextDirectionResolved() { return true; }
    public int getTextDirection() { return View.TEXT_DIRECTION_FIRST_STRONG; }
    public boolean canResolveTextAlignment() { return true; }
    public boolean isTextAlignmentResolved() { return true; }
    public int getTextAlignment() { return View.TEXT_ALIGNMENT_GRAVITY; }
    public boolean onStartNestedScroll(View child, View target, int axes) { return false; }
    public void onNestedScrollAccepted(View child, View target, int axes) {}
    public void onStopNestedScroll(View target) {}
    public void onNestedScroll(View target, int dxc, int dyc, int dxu, int dyu) {}
    public void onNestedPreScroll(View target, int dx, int dy, int[] consumed) {}
    public boolean onNestedFling(View target, float vx, float vy, boolean consumed) { return false; }
    public boolean onNestedPreFling(View target, float vx, float vy) { return false; }
    public boolean onNestedPrePerformAccessibilityAction(View target, int action, android.os.Bundle args) { return false; }
}
