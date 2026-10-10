package android.view;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.os.Bundle;

/** A window, as PhoneWindow is: the decor (husk.DecorView) holding the content, its look from the theme, its events to the callback. */
public class Window {
    public static final int FEATURE_OPTIONS_PANEL = 0, FEATURE_NO_TITLE = 1, FEATURE_PROGRESS = 2, FEATURE_LEFT_ICON = 3, FEATURE_RIGHT_ICON = 4,
        FEATURE_INDETERMINATE_PROGRESS = 5, FEATURE_CONTEXT_MENU = 6, FEATURE_CUSTOM_TITLE = 7, FEATURE_ACTION_BAR = 8, FEATURE_ACTION_BAR_OVERLAY = 9,
        FEATURE_ACTION_MODE_OVERLAY = 10, FEATURE_SWIPE_TO_DISMISS = 11, FEATURE_CONTENT_TRANSITIONS = 12, FEATURE_ACTIVITY_TRANSITIONS = 13;
    public static final int ID_ANDROID_CONTENT = android.R.id.content, PROGRESS_VISIBILITY_ON = -1, PROGRESS_VISIBILITY_OFF = -2,
        DECOR_CAPTION_SHADE_AUTO = 0, DECOR_CAPTION_SHADE_LIGHT = 1, DECOR_CAPTION_SHADE_DARK = 2;
    public static final String NAVIGATION_BAR_BACKGROUND_TRANSITION_NAME = "android:navigation:background", STATUS_BAR_BACKGROUND_TRANSITION_NAME = "android:status:background";
    public interface Callback {
        boolean dispatchKeyEvent(KeyEvent e);
        boolean dispatchKeyShortcutEvent(KeyEvent e);
        boolean dispatchTouchEvent(MotionEvent e);
        boolean dispatchTrackballEvent(MotionEvent e);
        boolean dispatchGenericMotionEvent(MotionEvent e);
        boolean dispatchPopulateAccessibilityEvent(android.view.accessibility.AccessibilityEvent e);
        View onCreatePanelView(int featureId);
        boolean onCreatePanelMenu(int featureId, Menu menu);
        boolean onPreparePanel(int featureId, View view, Menu menu);
        boolean onMenuOpened(int featureId, Menu menu);
        boolean onMenuItemSelected(int featureId, MenuItem item);
        void onWindowAttributesChanged(WindowManager.LayoutParams attrs);
        void onContentChanged();
        void onWindowFocusChanged(boolean hasFocus);
        void onAttachedToWindow();
        void onDetachedFromWindow();
        void onPanelClosed(int featureId, Menu menu);
        boolean onSearchRequested();
        boolean onSearchRequested(android.view.SearchEvent e);
        ActionMode onWindowStartingActionMode(ActionMode.Callback cb);
        ActionMode onWindowStartingActionMode(ActionMode.Callback cb, int type);
        void onActionModeStarted(ActionMode mode);
        void onActionModeFinished(ActionMode mode);
        default void onProvideKeyboardShortcuts(java.util.List<KeyboardShortcutGroup> data, Menu menu, int deviceId) {}
        default void onPointerCaptureChanged(boolean hasCapture) {}
    }
    public interface OnFrameMetricsAvailableListener { void onFrameMetricsAvailable(Window w, FrameMetrics m, int dropCount); }
    public interface OnRestrictedCaptionAreaChangedListener { void onRestrictedCaptionAreaChanged(android.graphics.Rect r); }

    private final Context mContext;
    private Callback mCallback;
    private final WindowManager.LayoutParams mAttrs = new WindowManager.LayoutParams();
    private husk.DecorView mDecor;
    private ViewGroup mContentParent;
    private int mFeatures, mLocalFeatures;
    private WindowManager mWindowManager;
    private boolean mFloating, mDecorFits = true, mActive, mDestroyed, mIsActivityWindow;
    private int mStatusBarColor, mNavigationBarColor;
    private CharSequence mTitle;
    private Drawable mBackground;
    private int mDefaultSoftInput;
    private Callback mFallbackCallback;

    public Window(Context c) {
        mContext = c;
        TypedArray a = c.obtainStyledAttributes(husk.S.Window);
        mFloating = a.getBoolean(husk.S.Window_windowIsFloating, false);
        if (a.getBoolean(husk.S.Window_windowNoTitle, false)) mFeatures |= 1 << FEATURE_NO_TITLE;
        else if (a.getBoolean(husk.S.Window_windowActionBar, false)) mFeatures |= 1 << FEATURE_ACTION_BAR;
        if (a.getBoolean(husk.S.Window_windowFullscreen, false)) mAttrs.flags |= WindowManager.LayoutParams.FLAG_FULLSCREEN;
        if (a.getBoolean(husk.S.Window_windowTranslucentStatus, false)) mAttrs.flags |= WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS;
        if (a.getBoolean(husk.S.Window_windowTranslucentNavigation, false)) mAttrs.flags |= WindowManager.LayoutParams.FLAG_TRANSLUCENT_NAVIGATION;
        if (a.getBoolean(husk.S.Window_windowDrawsSystemBarBackgrounds, false)) mAttrs.flags |= WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS;
        mStatusBarColor = a.getColor(husk.S.Window_statusBarColor, 0xFF000000);
        mNavigationBarColor = a.getColor(husk.S.Window_navigationBarColor, 0xFF000000);
        mBackground = a.getDrawable(husk.S.Window_windowBackground);
        mDefaultSoftInput = a.getInt(husk.S.Window_windowSoftInputMode, 0);
        mAttrs.softInputMode = mDefaultSoftInput;
        mAttrs.layoutInDisplayCutoutMode = a.getInt(husk.S.Window_windowLayoutInDisplayCutoutMode, 0);
        if (mFloating) {
            mAttrs.width = WindowManager.LayoutParams.WRAP_CONTENT; mAttrs.height = WindowManager.LayoutParams.WRAP_CONTENT;
            mAttrs.gravity = Gravity.CENTER;
            if (a.getBoolean(husk.S.Window_backgroundDimEnabled, true)) { mAttrs.flags |= WindowManager.LayoutParams.FLAG_DIM_BEHIND; mAttrs.dimAmount = a.getFloat(husk.S.Window_backgroundDimAmount, 0.6f); }
        }
        a.recycle();
    }
    public final Context getContext() { return mContext; }
    public final TypedArray getWindowStyle() { return mContext.obtainStyledAttributes(husk.S.Window); }
    public void setCallback(Callback cb) { mCallback = cb; }
    public final Callback getCallback() { return mCallback; }
    public void huskSetActivityWindow(boolean a) { mIsActivityWindow = a; if (a) mAttrs.type = WindowManager.LayoutParams.TYPE_BASE_APPLICATION; }
    public boolean huskIsActivityWindow() { return mIsActivityWindow; }
    public void setWindowManager(WindowManager wm, android.os.IBinder token, String name) { setWindowManager(wm, token, name, false); }
    public void setWindowManager(WindowManager wm, android.os.IBinder token, String name, boolean hw) { mWindowManager = wm; mAttrs.token = token; }
    public WindowManager getWindowManager() { if (mWindowManager == null) mWindowManager = (WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE); return mWindowManager; }
    public final WindowManager.LayoutParams getAttributes() { return mAttrs; }
    public void setAttributes(WindowManager.LayoutParams a) { mAttrs.copyFrom(a); dispatchAttrs(); }
    private void dispatchAttrs() { if (mCallback != null) mCallback.onWindowAttributesChanged(mAttrs); if (mDecor != null) { husk.ViewRoot r = husk.ViewRoot.of(mDecor); if (r != null) r.setLayoutParams(mAttrs); mDecor.requestApplyInsets(); } }
    public void addFlags(int f) { setFlags(f, f); }
    public void clearFlags(int f) { setFlags(0, f); }
    public void setFlags(int f, int mask) { mAttrs.flags = (mAttrs.flags & ~mask) | (f & mask); dispatchAttrs(); }
    public void addPrivateFlags(int f) {}
    public void setLayout(int w, int h) { mAttrs.width = w; mAttrs.height = h; dispatchAttrs(); }
    public void setGravity(int g) { mAttrs.gravity = g; dispatchAttrs(); }
    public void setType(int t) { mAttrs.type = t; }
    public void setFormat(int f) { mAttrs.format = f; }
    public void setWindowAnimations(int a) { mAttrs.windowAnimations = a; }
    public void setSoftInputMode(int m) { mAttrs.softInputMode = m; }
    public void setDimAmount(float a) { mAttrs.dimAmount = a; dispatchAttrs(); }
    public void setBackgroundBlurRadius(int r) {}
    public void setElevation(float e) {}
    public float getElevation() { return 0; }
    public void setClipToOutline(boolean c) {}
    public void setColorMode(int m) {}
    public void setSustainedPerformanceMode(boolean e) {}
    public void setPreferMinimalPostProcessing(boolean b) {}
    public void setHideOverlayWindows(boolean b) {}
    public void setFrameRateBoostOnTouchEnabled(boolean b) {}
    public boolean requestFeature(int f) {
        if (mContentParent != null && f != FEATURE_NO_TITLE) throw new AndroidRuntimeExceptionHusk("requestFeature() must be called before adding content");
        if (f == FEATURE_NO_TITLE) mFeatures &= ~(1 << FEATURE_ACTION_BAR);
        if (f == FEATURE_ACTION_BAR) mFeatures &= ~(1 << FEATURE_NO_TITLE);
        mFeatures |= 1 << f;
        return true;
    }
    static final class AndroidRuntimeExceptionHusk extends android.util.AndroidRuntimeException { AndroidRuntimeExceptionHusk(String s) { super(s); } }
    public boolean hasFeature(int f) { return (mFeatures & (1 << f)) != 0; }
    protected final int getFeatures() { return mFeatures; }
    public final int getLocalFeatures() { return mFeatures; }
    public void setFeatureDrawableResource(int f, int id) {}
    public void setFeatureDrawable(int f, Drawable d) {}
    public void setFeatureInt(int f, int v) {}
    public boolean isFloating() { return mFloating; }
    public boolean isActive() { return mActive; }
    public void makeActive() { mActive = true; }
    public final boolean isDestroyed() { return mDestroyed; }
    public void huskDestroy() { mDestroyed = true; }
    public boolean isWideColorGamut() { return false; }

    // ---- the decor
    public View getDecorView() { if (mDecor == null) installDecor(); return mDecor; }
    public View peekDecorView() { return mDecor; }
    private void installDecor() {
        mDecor = new husk.DecorView(mContext, this);
        mDecor.setId(View.NO_ID);
        if (mBackground != null) mDecor.setWindowBackground(mBackground);
        mContentParent = mDecor.makeContent(hasFeature(FEATURE_ACTION_BAR) && !mFloating, mTitle != null ? mTitle : mContext instanceof android.app.Activity ? ((android.app.Activity) mContext).getTitle() : null);
    }
    public <T extends View> T findViewById(int id) { return getDecorView().findViewById(id); }
    public final <T extends View> T requireViewById(int id) { T v = getDecorView().findViewById(id); if (v == null) throw new IllegalArgumentException("ID does not reference a View inside this Window"); return v; }
    public void setContentView(int layout) {
        if (mContentParent == null) installDecor(); else mContentParent.removeAllViews();
        LayoutInflater.from(mContext).inflate(layout, mContentParent);
        if (mCallback != null && !mDestroyed) mCallback.onContentChanged();
    }
    public void setContentView(View v) { setContentView(v, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)); }
    public void setContentView(View v, ViewGroup.LayoutParams p) {
        if (mContentParent == null) installDecor(); else mContentParent.removeAllViews();
        mContentParent.addView(v, p);
        if (mCallback != null && !mDestroyed) mCallback.onContentChanged();
    }
    public void addContentView(View v, ViewGroup.LayoutParams p) {
        if (mContentParent == null) installDecor();
        mContentParent.addView(v, p);
        if (mCallback != null && !mDestroyed) mCallback.onContentChanged();
    }
    public void clearContentView() { if (mContentParent != null) mContentParent.removeAllViews(); }
    public View getCurrentFocus() { return mDecor != null ? mDecor.findFocus() : null; }
    public LayoutInflater getLayoutInflater() { return LayoutInflater.from(mContext); }
    public void setTitle(CharSequence t) { mTitle = t; if (mDecor != null) mDecor.setTitle(t); }
    public void setTitleColor(int c) {}
    public void setBackgroundDrawable(Drawable d) { mBackground = d; if (mDecor != null) mDecor.setWindowBackground(d); }
    public void setBackgroundDrawableResource(int id) { setBackgroundDrawable(mContext.getDrawable(id)); }
    public void setStatusBarColor(int c) { mStatusBarColor = c; if (mDecor != null) mDecor.invalidate(); }
    public int getStatusBarColor() { return mStatusBarColor; }
    public void setNavigationBarColor(int c) { mNavigationBarColor = c; if (mDecor != null) mDecor.invalidate(); }
    public int getNavigationBarColor() { return mNavigationBarColor; }
    public void setNavigationBarDividerColor(int c) {}
    public void setStatusBarContrastEnforced(boolean e) {}
    public void setNavigationBarContrastEnforced(boolean e) {}
    public boolean isStatusBarContrastEnforced() { return false; }
    public boolean isNavigationBarContrastEnforced() { return false; }
    public void setDecorFitsSystemWindows(boolean f) { mDecorFits = f; if (mDecor != null) mDecor.requestApplyInsets(); }
    public boolean huskDecorFits() {
        if (!mDecorFits) return false;
        int f = mAttrs.flags;
        if ((f & (WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS)) != 0) return false;
        int sys = mDecor != null ? mDecor.getSystemUiVisibility() | mAttrs.systemUiVisibility : mAttrs.systemUiVisibility;
        if ((sys & (View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION)) != 0) return false;
        return true;
    }
    public boolean huskFullscreen() { return (mAttrs.flags & WindowManager.LayoutParams.FLAG_FULLSCREEN) != 0; }
    public WindowInsetsController getInsetsController() { View d = getDecorView(); WindowInsetsController c = d.getWindowInsetsController(); return c != null ? c : husk.DecorView.fallbackInsetsController(); }
    public void setLocalFocus(boolean f, boolean inTouch) {}
    public void injectInputEvent(InputEvent e) {}
    public void takeKeyEvents(boolean b) {}
    public void takeSurface(SurfaceHolder.Callback2 cb) {}
    public void takeInputQueue(InputQueue.Callback cb) {}
    public void openPanel(int f, KeyEvent e) {}
    public void closePanel(int f) {}
    public void togglePanel(int f, KeyEvent e) {}
    public void invalidatePanelMenu(int f) {}
    public boolean performPanelShortcut(int f, int code, KeyEvent e, int flags) { return false; }
    public boolean performPanelIdentifierAction(int f, int id, int flags) { return false; }
    public void closeAllPanels() {}
    public boolean performContextMenuIdentifierAction(int id, int flags) { return false; }
    public void onConfigurationChanged(android.content.res.Configuration c) {}
    public void setChildDrawable(int f, Drawable d) {}
    public void setChildInt(int f, int v) {}
    public boolean isShortcutKey(int code, KeyEvent e) { return false; }
    public void setVolumeControlStream(int s) {}
    public int getVolumeControlStream() { return 3; }
    public void setMediaController(android.media.session.MediaController c) {}
    public void setUiOptions(int o) {}
    public void setIcon(int r) {}
    public void setDefaultIcon(int r) {}
    public void setLogo(int r) {}
    public void setDefaultLogo(int r) {}
    public void setCloseOnTouchOutside(boolean c) { if (mDecor != null) mDecor.setCloseOnTouchOutside(c); }
    public boolean shouldCloseOnTouchHusk() { return mDecor != null && mDecor.closeOnTouchOutside(); }
    public void setAllowEnterTransitionOverlap(boolean b) {}
    public void setAllowReturnTransitionOverlap(boolean b) {}
    public void setEnterTransition(android.transition.Transition t) {}
    public void setExitTransition(android.transition.Transition t) {}
    public void setReturnTransition(android.transition.Transition t) {}
    public void setReenterTransition(android.transition.Transition t) {}
    public void setSharedElementEnterTransition(android.transition.Transition t) {}
    public void setSharedElementExitTransition(android.transition.Transition t) {}
    public void setSharedElementsUseOverlay(boolean b) {}
    public void setTransitionBackgroundFadeDuration(long d) {}
    public android.transition.Transition getEnterTransition() { return null; } public android.transition.Transition getExitTransition() { return null; }
    public android.transition.Transition getReturnTransition() { return null; } public android.transition.Transition getReenterTransition() { return null; }
    public android.transition.Transition getSharedElementEnterTransition() { return null; } public android.transition.Transition getSharedElementExitTransition() { return null; }
    public android.transition.Transition getSharedElementReturnTransition() { return null; } public android.transition.Transition getSharedElementReenterTransition() { return null; }
    public void setSharedElementReturnTransition(android.transition.Transition t) {} public void setSharedElementReenterTransition(android.transition.Transition t) {}
    public boolean getAllowEnterTransitionOverlap() { return true; } public boolean getAllowReturnTransitionOverlap() { return true; }
    public boolean getSharedElementsUseOverlay() { return true; } public long getTransitionBackgroundFadeDuration() { return 0; }
    public void setTransitionManager(android.transition.TransitionManager m) {} public android.transition.TransitionManager getTransitionManager() { return null; }
    public android.transition.Scene getContentScene() { return null; }
    public void addOnFrameMetricsAvailableListener(OnFrameMetricsAvailableListener l, android.os.Handler h) {}
    public void removeOnFrameMetricsAvailableListener(OnFrameMetricsAvailableListener l) {}
    private husk.BackDispatcher mBackDispatcher;
    public android.window.OnBackInvokedDispatcher getOnBackInvokedDispatcher() { if (mBackDispatcher == null) mBackDispatcher = new husk.BackDispatcher(); return mBackDispatcher; }
    public boolean huskDispatchBack() { return mBackDispatcher != null && mBackDispatcher.dispatch(); }
    public void setRestrictedCaptionAreaListener(OnRestrictedCaptionAreaChangedListener l) {}
    public void setSystemGestureExclusionRects(java.util.List<android.graphics.Rect> r) {}
    public android.view.WindowInsets getRootWindowInsetsHusk() { return mDecor == null ? null : mDecor.getRootWindowInsets(); }
    public void restoreHierarchyState(Bundle b) {}
    public Bundle saveHierarchyState() { return new Bundle(); }
    public android.media.session.MediaController getMediaController() { return null; }

    // ---- the decor's events: through the callback (an activity or dialog), which hands back to these
    public boolean superDispatchKeyEvent(KeyEvent e) { return mDecor != null && mDecor.superDispatchKeyEvent(e); }
    public boolean superDispatchKeyShortcutEvent(KeyEvent e) { return false; }
    public boolean superDispatchTouchEvent(MotionEvent e) { return mDecor != null && mDecor.superDispatchTouchEvent(e); }
    public boolean superDispatchTrackballEvent(MotionEvent e) { return false; }
    public boolean superDispatchGenericMotionEvent(MotionEvent e) { return mDecor != null && mDecor.superDispatchGenericMotionEvent(e); }
}
