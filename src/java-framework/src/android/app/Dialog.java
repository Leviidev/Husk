package android.app;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.DialogInterface;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.TypedValue;
import android.view.*;

/** A window over the activity's: the dialog theme's floating window, shown through the window manager, cancelled by back or an outside touch. */
public class Dialog implements DialogInterface, Window.Callback, KeyEvent.Callback, View.OnCreateContextMenuListener {
    private Activity mOwnerActivity;
    final Context mContext;
    final WindowManager mWindowManager;
    final Window mWindow;
    View mDecor;
    private ActionBar mActionBar;
    protected boolean mCancelable = true;
    private boolean mCreated, mShowing, mCanceled, mCanceledOnTouchOutside = true;
    private OnShowListener mOnShow;
    private OnDismissListener mOnDismiss;
    private OnCancelListener mOnCancel;
    private OnKeyListener mOnKeyListener;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Runnable mDismissAction = this::dismissDialog;
    public Dialog(Context c) { this(c, 0, true); }
    public Dialog(Context c, int themeResId) { this(c, themeResId, true); }
    Dialog(Context c, int themeResId, boolean createContextThemeWrapper) {
        if (createContextThemeWrapper) {
            if (themeResId == 0) { TypedValue v = new TypedValue(); c.getTheme().resolveAttribute(android.R.attr.dialogTheme, v, true); themeResId = v.resourceId; }
            mContext = themeResId != 0 ? new ContextThemeWrapper(c, themeResId) : c;
        } else mContext = c;
        mWindowManager = (WindowManager) c.getSystemService(Context.WINDOW_SERVICE);
        mWindow = new Window(mContext);
        mWindow.setCallback(this);
        mWindow.setWindowManager(mWindowManager, null, null);
        mWindow.setGravity(Gravity.CENTER);
        Activity a = ownerOf(c);
        if (a != null) setOwnerActivity(a);
    }
    protected Dialog(Context c, boolean cancelable, OnCancelListener l) { this(c); mCancelable = cancelable; setOnCancelListener(l); }
    private static Activity ownerOf(Context c) { while (c instanceof ContextWrapper) { if (c instanceof Activity) return (Activity) c; c = ((ContextWrapper) c).getBaseContext(); } return null; }
    public final Context getContext() { return mContext; }
    public ActionBar getActionBar() { return mActionBar; }
    public final void setOwnerActivity(Activity a) { mOwnerActivity = a; }
    public final Activity getOwnerActivity() { return mOwnerActivity; }
    public boolean isShowing() { return mDecor != null && mDecor.getVisibility() == View.VISIBLE && mShowing; }
    public void create() { if (!mCreated) dispatchOnCreate(null); }
    public void show() {
        if (mShowing) { if (mDecor != null) mDecor.setVisibility(View.VISIBLE); return; }
        mCanceled = false;
        if (!mCreated) dispatchOnCreate(null);
        onStart();
        mDecor = mWindow.getDecorView();
        WindowManager.LayoutParams l = mWindow.getAttributes();
        if (l.type == 0 || l.type == WindowManager.LayoutParams.TYPE_BASE_APPLICATION) l.type = WindowManager.LayoutParams.TYPE_APPLICATION;
        try { mWindowManager.addView(mDecor, l); } catch (RuntimeException e) { android.util.Log.w("Dialog", "could not show", e); return; }
        mShowing = true;
        if (mOnShow != null) mOnShow.onShow(this);
    }
    public void hide() { if (mDecor != null) mDecor.setVisibility(View.GONE); }
    public void dismiss() { if (Looper.myLooper() == Looper.getMainLooper()) dismissDialog(); else mHandler.post(mDismissAction); }
    void dismissDialog() {
        if (mDecor == null || !mShowing) return;
        if (mWindow.isDestroyed()) return;
        try { mWindowManager.removeViewImmediate(mDecor); } catch (RuntimeException e) {}
        mDecor = null;
        mShowing = false;
        onStop();
        if (mOnDismiss != null) mOnDismiss.onDismiss(this);
    }
    void dispatchOnCreate(Bundle s) { if (!mCreated) { onCreate(s); mCreated = true; } }
    protected void onCreate(Bundle s) {}
    protected void onStart() { if (mActionBar != null) mActionBar.setShowHideAnimationEnabled(true); }
    protected void onStop() {}
    public Bundle onSaveInstanceState() { return new Bundle(); }
    public void onRestoreInstanceState(Bundle s) {}
    public Window getWindow() { return mWindow; }
    public View getCurrentFocus() { return mWindow.getCurrentFocus(); }
    public <T extends View> T findViewById(int id) { return mWindow.findViewById(id); }
    public final <T extends View> T requireViewById(int id) { T v = findViewById(id); if (v == null) throw new IllegalArgumentException("ID does not reference a View inside this Dialog"); return v; }
    public void setContentView(int layoutResID) { mWindow.setContentView(layoutResID); }
    public void setContentView(View v) { mWindow.setContentView(v); }
    public void setContentView(View v, ViewGroup.LayoutParams p) { mWindow.setContentView(v, p); }
    public void addContentView(View v, ViewGroup.LayoutParams p) { mWindow.addContentView(v, p); }
    public void setTitle(CharSequence t) { mWindow.setTitle(t); mWindow.getAttributes().setTitle(t); }
    public void setTitle(int r) { setTitle(mContext.getText(r)); }
    public boolean onKeyDown(int keyCode, KeyEvent e) { if (keyCode == KeyEvent.KEYCODE_BACK || keyCode == KeyEvent.KEYCODE_ESCAPE) { e.startTracking(); return true; } return false; }
    public boolean onKeyLongPress(int k, KeyEvent e) { return false; }
    public boolean onKeyUp(int keyCode, KeyEvent e) { if ((keyCode == KeyEvent.KEYCODE_BACK || keyCode == KeyEvent.KEYCODE_ESCAPE) && e.isTracking() && !e.isCanceled()) { onBackPressed(); return true; } return false; }
    public boolean onKeyMultiple(int k, int n, KeyEvent e) { return false; }
    public void onBackPressed() { if (mWindow.huskDispatchBack()) return; if (mCancelable) cancel(); }
    public boolean onKeyShortcut(int k, KeyEvent e) { return false; }
    public boolean onTouchEvent(MotionEvent e) {
        if (mCancelable && mShowing && mCanceledOnTouchOutside && e.getAction() == MotionEvent.ACTION_DOWN && isOutOfBounds(e)) { cancel(); return true; }
        if (mCancelable && mShowing && mCanceledOnTouchOutside && e.getAction() == MotionEvent.ACTION_OUTSIDE) { cancel(); return true; }
        return false;
    }
    private boolean isOutOfBounds(MotionEvent e) { final int x = (int) e.getX(), y = (int) e.getY(); final View d = mDecor; return d != null && (x < 0 || y < 0 || x > d.getWidth() || y > d.getHeight()); }
    public boolean onTrackballEvent(MotionEvent e) { return false; }
    public boolean onGenericMotionEvent(MotionEvent e) { return false; }
    public void onWindowAttributesChanged(WindowManager.LayoutParams p) { if (mDecor != null && mShowing) mWindowManager.updateViewLayout(mDecor, p); }
    public void onContentChanged() {}
    public void onWindowFocusChanged(boolean f) {}
    public void onAttachedToWindow() {}
    public void onDetachedFromWindow() {}
    public boolean dispatchKeyEvent(KeyEvent e) {
        if (mOnKeyListener != null && mOnKeyListener.onKey(this, e.getKeyCode(), e)) return true;
        if (mWindow.superDispatchKeyEvent(e)) return true;
        return e.dispatch(this, mDecor != null ? mDecor.getKeyDispatcherState() : null, this);
    }
    public boolean dispatchKeyShortcutEvent(KeyEvent e) { return onKeyShortcut(e.getKeyCode(), e); }
    public boolean dispatchTouchEvent(MotionEvent e) { if (mWindow.superDispatchTouchEvent(e)) return true; return onTouchEvent(e); }
    public boolean dispatchTrackballEvent(MotionEvent e) { return onTrackballEvent(e); }
    public boolean dispatchGenericMotionEvent(MotionEvent e) { if (mWindow.superDispatchGenericMotionEvent(e)) return true; return onGenericMotionEvent(e); }
    public boolean dispatchPopulateAccessibilityEvent(android.view.accessibility.AccessibilityEvent e) { return false; }
    public View onCreatePanelView(int f) { return null; }
    public boolean onCreatePanelMenu(int f, Menu m) { return f == Window.FEATURE_OPTIONS_PANEL && onCreateOptionsMenu(m); }
    public boolean onPreparePanel(int f, View v, Menu m) { return f == Window.FEATURE_OPTIONS_PANEL && onPrepareOptionsMenu(m) && m.hasVisibleItems(); }
    public boolean onMenuOpened(int f, Menu m) { return true; }
    public boolean onMenuItemSelected(int f, MenuItem i) { return false; }
    public void onPanelClosed(int f, Menu m) {}
    public boolean onCreateOptionsMenu(Menu m) { return true; }
    public boolean onPrepareOptionsMenu(Menu m) { return true; }
    public boolean onOptionsItemSelected(MenuItem i) { return false; }
    public void onOptionsMenuClosed(Menu m) {}
    public void openOptionsMenu() {} public void closeOptionsMenu() {} public void invalidateOptionsMenu() {}
    public void onCreateContextMenu(ContextMenu m, View v, ContextMenu.ContextMenuInfo i) {}
    public void registerForContextMenu(View v) { v.setOnCreateContextMenuListener(this); }
    public void unregisterForContextMenu(View v) { v.setOnCreateContextMenuListener(null); }
    public void openContextMenu(View v) { v.showContextMenu(); }
    public boolean onContextItemSelected(MenuItem i) { return false; }
    public void onContextMenuClosed(Menu m) {}
    public boolean onSearchRequested(SearchEvent e) { return false; }
    public boolean onSearchRequested() { return false; }
    public final SearchEvent getSearchEvent() { return null; }
    public ActionMode onWindowStartingActionMode(ActionMode.Callback cb) { return null; }
    public ActionMode onWindowStartingActionMode(ActionMode.Callback cb, int type) { return null; }
    public void onActionModeStarted(ActionMode m) {}
    public void onActionModeFinished(ActionMode m) {}
    public void takeKeyEvents(boolean get) { mWindow.takeKeyEvents(get); }
    public final boolean requestWindowFeature(int f) { return mWindow.requestFeature(f); }
    public final void setFeatureDrawableResource(int f, int r) {} public final void setFeatureDrawableUri(int f, android.net.Uri u) {} public final void setFeatureDrawable(int f, Drawable d) {} public final void setFeatureDrawableAlpha(int f, int a) {}
    public LayoutInflater getLayoutInflater() { return mWindow.getLayoutInflater(); }
    public void setCancelable(boolean c) { mCancelable = c; }
    public void setCanceledOnTouchOutside(boolean c) { if (c && !mCancelable) mCancelable = true; mCanceledOnTouchOutside = c; }
    public void cancel() { if (!mCanceled && mOnCancel != null) { mCanceled = true; mOnCancel.onCancel(this); } dismiss(); }
    public void setOnCancelListener(OnCancelListener l) { mOnCancel = l; }
    public void setCancelMessage(android.os.Message m) {}
    public void setOnDismissListener(OnDismissListener l) { mOnDismiss = l; }
    public void setOnShowListener(OnShowListener l) { mOnShow = l; }
    public void setDismissMessage(android.os.Message m) {}
    public final void setVolumeControlStream(int s) {}
    public final int getVolumeControlStream() { return 3; }
    public void setOnKeyListener(OnKeyListener l) { mOnKeyListener = l; }
    public void onProvideKeyboardShortcuts(java.util.List<KeyboardShortcutGroup> d, Menu m, int id) {}
    public android.window.OnBackInvokedDispatcher getOnBackInvokedDispatcher() { return mWindow.getOnBackInvokedDispatcher(); }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    protected boolean allowsRegisterDefaultOnBackInvokedCallback() { return false; }
    public void onWindowDismissed(boolean p0, boolean p1) {}
    public void setDismissOverride(java.lang.Runnable p0) {}
    public boolean takeCancelAndDismissListeners(java.lang.String p0, android.content.DialogInterface.OnCancelListener p1, android.content.DialogInterface.OnDismissListener p2) { return false; }
    // ---- end of generated members
}
