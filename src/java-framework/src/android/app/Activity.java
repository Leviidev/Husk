package android.app;

import android.content.*;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.*;
import android.view.accessibility.AccessibilityEvent;
import java.util.ArrayList;

/** An activity, run by husk.AppRunner as ActivityThread runs one: attached to a base context, its lifecycle in order, its window on screen. */
public class Activity extends ContextThemeWrapper implements Window.Callback, KeyEvent.Callback, LayoutInflater.Factory2, View.OnCreateContextMenuListener,
        ComponentCallbacks2 {
    public static final int RESULT_OK = -1, RESULT_CANCELED = 0, RESULT_FIRST_USER = 1, DEFAULT_KEYS_DISABLE = 0, DEFAULT_KEYS_DIALER = 1,
        DEFAULT_KEYS_SHORTCUT = 2, DEFAULT_KEYS_SEARCH_LOCAL = 3, DEFAULT_KEYS_SEARCH_GLOBAL = 4, OVERRIDE_TRANSITION_OPEN = 0, OVERRIDE_TRANSITION_CLOSE = 1;
    protected static final int[] FOCUSED_STATE_SET = { android.R.attr.state_focused };
    private Window mWindow;
    private Intent mIntent = new Intent(Intent.ACTION_MAIN);
    private Application mApplication;
    private ComponentName mComponent;
    private CharSequence mTitle;
    private int mTitleColor;
    private boolean mFinished, mDestroyed, mResumed, mStarted, mCalled, mVisibleFromClient = true, mChangingConfigurations;
    private int mResultCode = RESULT_CANCELED;
    private Intent mResultData;
    private Activity mParent, mCaller;
    private int mRequestCode = -1;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private ActionBar mActionBar;
    private final ArrayList<Application.ActivityLifecycleCallbacks> mLifecycleCallbacks = new ArrayList<>();
    private FragmentManager mFragments;
    private int mRequestedOrientation = -1;
    private husk.Manifest.Component mInfo;

    public Activity() { super(null, 0); }

    // ---- husk.AppRunner's side
    /** As Activity.attach: the base context, the window, the intent. */
    public final void huskAttach(Context base, Application app, Intent intent, husk.Manifest.Component info, Activity caller, int requestCode) {
        attachBaseContext(base);
        mApplication = app;
        mIntent = intent != null ? intent : new Intent(Intent.ACTION_MAIN);
        mInfo = info;
        mCaller = caller;
        mRequestCode = requestCode;
        mComponent = new ComponentName(getPackageName(), getClass().getName());
        int theme = info != null && info.theme != 0 ? info.theme : husk.ContextImpl.defaultTheme();
        setTheme(theme);
        if (info != null && info.label != 0) { try { mTitle = getResources().getText(info.label); } catch (Exception e) {} }
        if (mTitle == null) mTitle = husk.AppRunner.appLabel(this);
        if (info != null && info.screenOrientation >= 0) mRequestedOrientation = info.screenOrientation;
    }
    public final void huskCreate(Bundle state) {
        mCalled = false;
        onCreate(state);
        if (!mCalled) throw new SuperNotCalledException("Activity " + getClass().getName() + " did not call through to super.onCreate()");
        onPostCreate(state);
    }
    public final void huskStart() { mCalled = false; onStart(); mStarted = true; }
    public final void huskRestart() { onRestart(); }
    public final void huskResume() {
        mCalled = false;
        onResume();
        mResumed = true;
        onPostResume();
        if (mVisibleFromClient) makeVisible();
    }
    public final void huskPause() { mResumed = false; onPause(); }
    public final void huskStop() { mStarted = false; onStop(); if (mWindow != null && mWindow.peekDecorView() != null) { husk.ViewRoot r = husk.ViewRoot.of(mWindow.peekDecorView()); if (r != null) r.remove(); } }
    public final void huskDestroy() { mDestroyed = true; onDestroy(); if (mWindow != null) mWindow.huskDestroy(); }
    public final void huskBack() { onBackPressedHusk(); }
    public final boolean huskVisible() { return mWindow != null && mWindow.peekDecorView() != null && husk.ViewRoot.of(mWindow.peekDecorView()) != null; }
    public final Activity huskCaller() { return mCaller; }
    public final int huskRequestCode() { return mRequestCode; }
    public final int huskResultCode() { return mResultCode; }
    public final Intent huskResultData() { return mResultData; }
    public final void huskActivityResult(int request, int result, Intent data) { onActivityResult(request, result, data); }
    void makeVisible() {
        View decor = getWindow().getDecorView();
        if (husk.ViewRoot.of(decor) == null) getWindowManager().addView(decor, getWindow().getAttributes());
        decor.setVisibility(View.VISIBLE);
    }
    private void onBackPressedHusk() {
        if (getWindow().huskDispatchBack()) return;
        KeyEvent down = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK), up = new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BACK);
        if (!husk.ViewRoot.dispatchKey(down)) onKeyDown(KeyEvent.KEYCODE_BACK, down);
        if (!husk.ViewRoot.dispatchKey(up)) onKeyUp(KeyEvent.KEYCODE_BACK, up);
    }

    // ---- lifecycle
    protected void onCreate(Bundle state) {
        mCalled = true;
        for (Application.ActivityLifecycleCallbacks c : callbacks()) c.onActivityCreated(this, state);
    }
    public void onCreate(Bundle state, android.os.PersistableBundle p) { onCreate(state); }
    protected void onPostCreate(Bundle state) {}
    protected void onStart() { mCalled = true; for (Application.ActivityLifecycleCallbacks c : callbacks()) c.onActivityStarted(this); }
    protected void onRestart() { mCalled = true; }
    protected void onResume() { mCalled = true; for (Application.ActivityLifecycleCallbacks c : callbacks()) c.onActivityResumed(this); }
    protected void onPostResume() { if (mActionBar != null) mActionBar.setShowHideAnimationEnabled(true); }
    protected void onPause() { mCalled = true; for (Application.ActivityLifecycleCallbacks c : callbacks()) c.onActivityPaused(this); }
    protected void onStop() { mCalled = true; for (Application.ActivityLifecycleCallbacks c : callbacks()) c.onActivityStopped(this); }
    protected void onDestroy() { mCalled = true; for (Application.ActivityLifecycleCallbacks c : callbacks()) c.onActivityDestroyed(this); }
    protected void onSaveInstanceState(Bundle out) { for (Application.ActivityLifecycleCallbacks c : callbacks()) c.onActivitySaveInstanceState(this, out); }
    public void onSaveInstanceState(Bundle out, android.os.PersistableBundle p) { onSaveInstanceState(out); }
    protected void onRestoreInstanceState(Bundle state) {}
    protected void onNewIntent(Intent i) {}
    protected void onActivityResult(int request, int result, Intent data) {}
    public void onActivityReenter(int result, Intent data) {}
    protected void onUserLeaveHint() {}
    public void onUserInteraction() {}
    public void onRequestPermissionsResult(int request, String[] perms, int[] results) {}
    public void onConfigurationChanged(Configuration c) { mCalled = true; }
    public void onLowMemory() { mCalled = true; }
    public void onTrimMemory(int level) { mCalled = true; }
    public void onMultiWindowModeChanged(boolean m) {}
    public void onMultiWindowModeChanged(boolean m, Configuration c) {}
    public void onPictureInPictureModeChanged(boolean m) {}
    public void onPictureInPictureModeChanged(boolean m, Configuration c) {}
    public void onTopResumedActivityChanged(boolean top) {}
    public void onEnterAnimationComplete() {}
    public void onLocalVoiceInteractionStarted() {}
    public CharSequence onCreateDescription() { return null; }
    public Object onRetainNonConfigurationInstance() { return null; }
    public Object getLastNonConfigurationInstance() { return null; }
    private ArrayList<Application.ActivityLifecycleCallbacks> callbacks() {
        ArrayList<Application.ActivityLifecycleCallbacks> r = new ArrayList<>(mLifecycleCallbacks);
        if (mApplication != null) r.addAll(mApplication.huskLifecycleCallbacks());
        return r;
    }
    public void registerActivityLifecycleCallbacks(Application.ActivityLifecycleCallbacks c) { mLifecycleCallbacks.add(c); }
    public void unregisterActivityLifecycleCallbacks(Application.ActivityLifecycleCallbacks c) { mLifecycleCallbacks.remove(c); }
    public boolean isResumed() { return mResumed; }
    public final boolean isResumedHusk() { return mResumed; }
    public boolean isFinishing() { return mFinished; }
    public boolean isDestroyed() { return mDestroyed; }
    public boolean isChangingConfigurations() { return mChangingConfigurations; }
    public boolean isTaskRoot() { return mCaller == null; }
    public boolean isChild() { return mParent != null; }
    public final Activity getParent() { return mParent; }
    public boolean isInMultiWindowMode() { return false; }
    public boolean isInPictureInPictureMode() { return false; }
    public boolean isVoiceInteraction() { return false; }
    public boolean isVoiceInteractionRoot() { return false; }
    public boolean isLocalVoiceInteractionSupported() { return false; }
    public boolean isImmersive() { return false; }
    public void setImmersive(boolean i) {}
    public boolean isLaunchedFromBubble() { return false; }
    public boolean isActivityTransitionRunning() { return false; }
    public void recreate() { husk.AppRunner.recreate(this); }

    // ---- the window
    public Window getWindow() {
        if (mWindow == null) {
            mWindow = new Window(this);
            mWindow.setCallback(this);
            mWindow.huskSetActivityWindow(true);
            mWindow.setTitle(mTitle);
            if (mRequestedOrientation >= 0) husk.Native.setOrientation(mRequestedOrientation);
        }
        return mWindow;
    }
    public WindowManager getWindowManager() { return (WindowManager) getSystemService(WINDOW_SERVICE); }
    @Override public Object getSystemService(String name) {
        if (WINDOW_SERVICE.equals(name)) return new WindowManagerImpl(this);
        return super.getSystemService(name);
    }
    public final boolean requestWindowFeature(int f) { return getWindow().requestFeature(f); }
    public final void setFeatureDrawableResource(int f, int id) {}
    public final void setFeatureDrawable(int f, Drawable d) {}
    public final void setProgressBarVisibility(boolean v) {}
    public final void setProgressBarIndeterminateVisibility(boolean v) {}
    public final void setProgressBarIndeterminate(boolean v) {}
    public final void setProgress(int p) {}
    public void setContentView(int layout) { getWindow().setContentView(layout); initWindowDecorActionBar(); }
    public void setContentView(View v) { getWindow().setContentView(v); initWindowDecorActionBar(); }
    public void setContentView(View v, ViewGroup.LayoutParams p) { getWindow().setContentView(v, p); initWindowDecorActionBar(); }
    public void addContentView(View v, ViewGroup.LayoutParams p) { getWindow().addContentView(v, p); initWindowDecorActionBar(); }
    private void initWindowDecorActionBar() {
        if (mActionBar == null && getWindow().hasFeature(Window.FEATURE_ACTION_BAR) && !getWindow().isFloating()) mActionBar = new husk.SimpleActionBar(this, (husk.DecorView) getWindow().getDecorView());
    }
    public <T extends View> T findViewById(int id) { return getWindow().findViewById(id); }
    public final <T extends View> T requireViewById(int id) { T v = findViewById(id); if (v == null) throw new IllegalArgumentException("ID does not reference a View inside this Activity"); return v; }
    public View getCurrentFocus() { return mWindow != null ? mWindow.getCurrentFocus() : null; }
    public LayoutInflater getLayoutInflater() { return getWindow().getLayoutInflater(); }
    public MenuInflater getMenuInflater() { return new MenuInflater(this); }
    public ActionBar getActionBar() { initWindowDecorActionBar(); return mActionBar; }
    public void setActionBar(android.widget.Toolbar t) { if (t != null) t.setTitle(getTitle()); }
    public void setTitle(CharSequence t) { mTitle = t; onTitleChanged(t, mTitleColor); }
    public void setTitle(int id) { setTitle(getText(id)); }
    public void setTitleColor(int c) { mTitleColor = c; }
    public final CharSequence getTitle() { return mTitle; }
    public final int getTitleColor() { return mTitleColor; }
    protected void onTitleChanged(CharSequence t, int color) { if (mWindow != null) mWindow.setTitle(t); }
    protected void onChildTitleChanged(Activity child, CharSequence t) {}
    public void onContentChanged() {}
    public void onWindowFocusChanged(boolean f) {}
    public void onAttachedToWindow() {}
    public void onDetachedFromWindow() {}
    public void onWindowAttributesChanged(WindowManager.LayoutParams p) {}
    public boolean hasWindowFocus() { View d = mWindow != null ? mWindow.peekDecorView() : null; return d != null && d.hasWindowFocus(); }
    public void setVisible(boolean v) { mVisibleFromClient = v; if (mWindow != null && mWindow.peekDecorView() != null) mWindow.peekDecorView().setVisibility(v ? View.VISIBLE : View.INVISIBLE); }
    public void setRequestedOrientation(int o) { mRequestedOrientation = o; husk.Native.setOrientation(o); }
    public int getRequestedOrientation() { return mRequestedOrientation; }
    public void setShowWhenLocked(boolean b) {}
    public void setTurnScreenOn(boolean b) {}
    public void setInheritShowWhenLocked(boolean b) {}
    public void setTaskDescription(ActivityManager.TaskDescription d) {}
    public void setRecentsScreenshotEnabled(boolean e) {}
    public void reportFullyDrawn() {}
    public void setVolumeControlStream(int stream) {}
    public final int getVolumeControlStream() { return 3; }
    public final void setDefaultKeyMode(int m) {}
    public void overridePendingTransition(int a, int b) {}
    public void overridePendingTransition(int a, int b, int c) {}
    public void overrideActivityTransition(int type, int enter, int exit) {}
    public void overrideActivityTransition(int type, int enter, int exit, int bg) {}
    public void clearOverrideActivityTransition(int type) {}
    public void postponeEnterTransition() {}
    public void startPostponedEnterTransition() {}
    public void setEnterSharedElementCallback(SharedElementCallback c) {}
    public void setExitSharedElementCallback(SharedElementCallback c) {}
    public void enterPictureInPictureMode() {}
    public boolean enterPictureInPictureMode(PictureInPictureParams p) { return false; }
    public void setPictureInPictureParams(PictureInPictureParams p) {}
    public android.window.OnBackInvokedDispatcher getOnBackInvokedDispatcher() { return getWindow().getOnBackInvokedDispatcher(); }
    public android.window.SplashScreen getSplashScreen() { return husk.SplashScreens.of(this); }
    public final boolean isTaskRootHusk() { return isTaskRoot(); }

    // ---- intents and results
    public Intent getIntent() { return mIntent; }
    public void setIntent(Intent i) { mIntent = i; }
    public final Application getApplication() { return mApplication != null ? mApplication : Application.huskGet(); }
    public ComponentName getComponentName() { return mComponent != null ? mComponent : new ComponentName(this, getClass()); }
    public String getLocalClassName() { String p = getPackageName(), c = getClass().getName(); return c.startsWith(p + ".") ? c.substring(p.length() + 1) : c; }
    public String getCallingPackage() { return mCaller != null ? getPackageName() : null; }
    public ComponentName getCallingActivity() { return mCaller != null ? mCaller.getComponentName() : null; }
    public String getLaunchedFromPackage() { return getCallingPackage(); }
    public int getTaskId() { return 1; }
    public final void setResult(int code) { mResultCode = code; mResultData = null; }
    public final void setResult(int code, Intent data) { mResultCode = code; mResultData = data; }
    @Override public void startActivity(Intent i) { startActivityForResult(i, -1); }
    @Override public void startActivity(Intent i, Bundle o) { startActivityForResult(i, -1, o); }
    public void startActivityForResult(Intent i, int request) { startActivityForResult(i, request, null); }
    public void startActivityForResult(Intent i, int request, Bundle o) { husk.AppRunner.startActivity(this, i, request, o); }
    public void startActivityFromChild(Activity child, Intent i, int request) { startActivityForResult(i, request); }
    public void startActivityFromFragment(Fragment f, Intent i, int request) { startActivityForResult(i, request); }
    public boolean startActivityIfNeeded(Intent i, int request) { startActivityForResult(i, request); return true; }
    public boolean startNextMatchingActivity(Intent i) { return false; }
    public void startIntentSenderForResult(IntentSender s, int request, Intent fill, int mask, int values, int extra) {}
    public void startIntentSenderForResult(IntentSender s, int request, Intent fill, int mask, int values, int extra, Bundle o) {}
    public void finish() { if (mFinished) return; mFinished = true; husk.AppRunner.finish(this); }
    public void finishAffinity() { husk.AppRunner.finishAll(); }
    public void finishAndRemoveTask() { finishAffinity(); }
    public void finishAfterTransition() { finish(); }
    public void finishActivity(int request) {}
    public void finishFromChild(Activity child) { finish(); }
    public boolean moveTaskToBack(boolean nonRoot) { husk.Native.exit(); return true; }
    public boolean navigateUpTo(Intent up) { finish(); return true; }
    public boolean navigateUpToFromChild(Activity child, Intent up) { return navigateUpTo(up); }
    public boolean onNavigateUp() { if (mInfo != null && mInfo.parentActivity != null) { finish(); return true; } return false; }
    public boolean shouldUpRecreateTask(Intent target) { return false; }
    public Intent getParentActivityIntent() { return mInfo != null && mInfo.parentActivity != null ? new Intent().setClassName(getPackageName(), mInfo.parentActivity) : null; }
    public final void requestPermissions(String[] perms, int request) {
        final int[] r = new int[perms.length];
        mHandler.post(() -> onRequestPermissionsResult(request, perms, r));
    }
    public boolean shouldShowRequestPermissionRationale(String p) { return false; }
    public SharedPreferences getPreferences(int mode) { return getSharedPreferences(getLocalClassName(), mode); }
    public void runOnUiThread(Runnable r) { if (Looper.myLooper() == Looper.getMainLooper()) r.run(); else mHandler.post(r); }
    public final Cursor_ managedQueryHusk() { return null; }
    interface Cursor_ {}
    public FragmentManager getFragmentManager() { if (mFragments == null) mFragments = new FragmentManager(this); return mFragments; }
    public void onAttachFragment(Fragment f) {}
    public LoaderManager getLoaderManager() { return null; }
    public void showDialog(int id) {}
    public final boolean showDialog(int id, Bundle args) { return false; }
    public void dismissDialog(int id) {}
    public void removeDialog(int id) {}
    protected Dialog onCreateDialog(int id) { return null; }
    public boolean releaseInstance() { return false; }
    public void setLocusContext(Object id, Bundle b) {}
    public boolean showAssist(Bundle args) { return false; }
    public android.view.DragAndDropPermissions requestDragAndDropPermissions(DragEvent e) { return null; }
    public void startLockTask() {}
    public void stopLockTask() {}
    public void setDisablePreviewScreenshots(boolean d) {}
    public void triggerSearch(String q, Bundle d) {}
    public void startSearch(String q, boolean s, Bundle d, boolean g) {}
    public void takeKeyEvents(boolean b) {}
    public void registerScreenCaptureCallback(java.util.concurrent.Executor e, Object cb) {}
    public void unregisterScreenCaptureCallback(Object cb) {}

    // ---- events
    public boolean dispatchKeyEvent(KeyEvent e) {
        onUserInteraction();
        if (e.getKeyCode() == KeyEvent.KEYCODE_MENU && mActionBar != null && mActionBar.onMenuKeyEvent(e)) return true;
        Window w = getWindow();
        if (w.superDispatchKeyEvent(e)) return true;
        View decor = w.peekDecorView();
        return e.dispatch(this, decor != null ? decor.getKeyDispatcherState() : null, this);
    }
    public boolean dispatchKeyShortcutEvent(KeyEvent e) { return onKeyShortcut(e.getKeyCode(), e); }
    public boolean dispatchTouchEvent(MotionEvent e) {
        if (e.getActionMasked() == MotionEvent.ACTION_DOWN) onUserInteraction();
        if (getWindow().superDispatchTouchEvent(e)) return true;
        return onTouchEvent(e);
    }
    public boolean dispatchTrackballEvent(MotionEvent e) { return onTrackballEvent(e); }
    public boolean dispatchGenericMotionEvent(MotionEvent e) { if (getWindow().superDispatchGenericMotionEvent(e)) return true; return onGenericMotionEvent(e); }
    public boolean dispatchPopulateAccessibilityEvent(AccessibilityEvent e) { return false; }
    public boolean onTouchEvent(MotionEvent e) {
        if (getWindow().shouldCloseOnTouchHusk() && e.getActionMasked() == MotionEvent.ACTION_DOWN) { finish(); return true; }
        return false;
    }
    public boolean onTrackballEvent(MotionEvent e) { return false; }
    public boolean onGenericMotionEvent(MotionEvent e) { return false; }
    public boolean onKeyDown(int code, KeyEvent e) {
        if (code == KeyEvent.KEYCODE_BACK) { e.startTracking(); return true; }
        return false;
    }
    public boolean onKeyLongPress(int code, KeyEvent e) { return false; }
    public boolean onKeyUp(int code, KeyEvent e) {
        if (code == KeyEvent.KEYCODE_BACK && !e.isCanceled()) { onBackPressed(); return true; }
        return false;
    }
    public boolean onKeyMultiple(int code, int n, KeyEvent e) { return false; }
    public boolean onKeyShortcut(int code, KeyEvent e) { return false; }
    public void onBackPressed() {
        if (mActionBar != null && mActionBar.collapseActionView()) return;
        if (mFragments != null && mFragments.popBackStackImmediate()) return;
        finishAfterTransition();
    }
    public boolean onSearchRequested() { return false; }
    public boolean onSearchRequested(SearchEvent e) { return false; }
    public void onPointerCaptureChanged(boolean c) {}
    public void onProvideAssistData(Bundle d) {}
    public void onProvideKeyboardShortcuts(java.util.List<KeyboardShortcutGroup> d, Menu m, int dev) {}
    public void onGetDirectActions(android.os.CancellationSignal s, java.util.function.Consumer<java.util.List<Object>> c) {}

    // ---- menus (no on-screen menu: the options menu is created so apps can find their items, and the menu key can open it later)
    public View onCreatePanelView(int f) { return null; }
    public boolean onCreatePanelMenu(int f, Menu m) { return f == Window.FEATURE_OPTIONS_PANEL && onCreateOptionsMenu(m); }
    public boolean onPreparePanel(int f, View v, Menu m) { return f != Window.FEATURE_OPTIONS_PANEL || onPrepareOptionsMenu(m); }
    public boolean onMenuOpened(int f, Menu m) { return true; }
    public boolean onMenuItemSelected(int f, MenuItem item) { return f == Window.FEATURE_OPTIONS_PANEL ? onOptionsItemSelected(item) : onContextItemSelected(item); }
    public void onPanelClosed(int f, Menu m) {}
    public boolean onCreateOptionsMenu(Menu m) { return true; }
    public boolean onPrepareOptionsMenu(Menu m) { return true; }
    public boolean onOptionsItemSelected(MenuItem item) { if (item.getItemId() == android.R.id.home && mInfo != null && mInfo.parentActivity != null) return onNavigateUp(); return false; }
    public void onOptionsMenuClosed(Menu m) {}
    public void openOptionsMenu() {}
    public void closeOptionsMenu() {}
    public void invalidateOptionsMenu() { if (mActionBar != null) mActionBar.invalidateOptionsMenu(); }
    public void onCreateContextMenu(ContextMenu m, View v, ContextMenu.ContextMenuInfo i) {}
    public void registerForContextMenu(View v) { v.setOnCreateContextMenuListener(this); }
    public void unregisterForContextMenu(View v) { v.setOnCreateContextMenuListener(null); }
    public void openContextMenu(View v) { v.showContextMenu(); }
    public void closeContextMenu() {}
    public boolean onContextItemSelected(MenuItem item) { return false; }
    public void onContextMenuClosed(Menu m) {}
    public ActionMode startActionMode(ActionMode.Callback cb) { return null; }
    public ActionMode startActionMode(ActionMode.Callback cb, int type) { return null; }
    public ActionMode onWindowStartingActionMode(ActionMode.Callback cb) { return null; }
    public ActionMode onWindowStartingActionMode(ActionMode.Callback cb, int type) { return null; }
    public void onActionModeStarted(ActionMode m) {}
    public void onActionModeFinished(ActionMode m) {}

    // ---- LayoutInflater.Factory2 (fragments in layouts)
    public View onCreateView(String name, Context c, android.util.AttributeSet a) { return null; }
    public View onCreateView(View parent, String name, Context c, android.util.AttributeSet a) {
        if (!"fragment".equals(name)) return onCreateView(name, c, a);
        return getFragmentManager().huskInflateFragment(parent, c, a);
    }
    public void dump(String prefix, java.io.FileDescriptor fd, java.io.PrintWriter w, String[] args) {}
    static final class SuperNotCalledException extends android.util.AndroidRuntimeException { SuperNotCalledException(String s) { super(s); } }
}
