package android.app;

import android.content.*;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.*;

public class Activity extends ContextThemeWrapper implements Window.Callback, KeyEvent.Callback {
    public static final int RESULT_OK = -1, RESULT_CANCELED = 0, DEFAULT_KEYS_DISABLE = 0;
    private Window mWindow;
    private Intent mIntent = new Intent(Intent.ACTION_MAIN);
    private boolean mFinished;
    private int mOrientation = -1;
    private Handler mHandler;
    private Application mApplication;
    View mContent;

    public Activity() {}

    /** Husk's driver: the lifecycle as ActivityThread runs it. */
    public final void huskCreate() { onCreate(null); onPostCreate(null); }
    public final void huskStart() { onStart(); onResume(); onPostResume(); onWindowFocusChanged(true); }
    public final void huskPause() { onWindowFocusChanged(false); onPause(); }
    public final void huskResume() { onResume(); onWindowFocusChanged(true); }
    public final void huskBack() { onBackPressed(); }

    protected void onCreate(Bundle state) {}
    protected void onPostCreate(Bundle state) {}
    protected void onStart() {}
    protected void onRestart() {}
    protected void onResume() {}
    protected void onPostResume() {}
    protected void onPause() {}
    protected void onStop() {}
    protected void onDestroy() {}
    protected void onSaveInstanceState(Bundle out) {}
    protected void onRestoreInstanceState(Bundle state) {}
    protected void onNewIntent(Intent i) {}
    protected void onActivityResult(int request, int result, Intent data) {}
    public void onRequestPermissionsResult(int request, String[] perms, int[] results) {}
    public void onConfigurationChanged(Configuration c) {}
    public void onLowMemory() {}
    public void onTrimMemory(int level) {}
    public void onWindowFocusChanged(boolean focus) {}
    public void onAttachedToWindow() {}
    public void onDetachedFromWindow() {}
    public void onUserInteraction() {}
    public void onContentChanged() {}
    public void onBackPressed() { finish(); }
    public boolean onKeyDown(int code, KeyEvent e) { if (code == KeyEvent.KEYCODE_BACK) { onBackPressed(); return true; } return false; }
    public boolean onKeyUp(int code, KeyEvent e) { return false; }
    public boolean onKeyLongPress(int code, KeyEvent e) { return false; }
    public boolean onKeyMultiple(int code, int count, KeyEvent e) { return false; }
    public boolean onTouchEvent(MotionEvent e) { return false; }
    public boolean onGenericMotionEvent(MotionEvent e) { return false; }
    public boolean dispatchTouchEvent(MotionEvent e) { return (mContent != null && mContent.dispatchTouchEvent(e)) || onTouchEvent(e); }
    public boolean dispatchKeyEvent(KeyEvent e) {
        if (mContent != null && mContent.dispatchKeyEvent(e)) return true;
        return e.getAction() == KeyEvent.ACTION_DOWN ? onKeyDown(e.getKeyCode(), e) : onKeyUp(e.getKeyCode(), e);
    }
    public boolean dispatchGenericMotionEvent(MotionEvent e) { return mContent != null && mContent.dispatchGenericMotionEvent(e); }

    public Window getWindow() { if (mWindow == null) mWindow = new Window(this); return mWindow; }
    public WindowManager getWindowManager() { return (WindowManager) getSystemService(WINDOW_SERVICE); }
    public boolean requestWindowFeature(int f) { return true; }
    public void setContentView(View v) { setContentView(v, new ViewGroup.LayoutParams(-1, -1)); }
    public void setContentView(View v, ViewGroup.LayoutParams p) {
        if (p != null) v.setLayoutParams(p);
        mContent = v;
        getWindow().setContentView(v);
        husk.Native.setContentView(v);
        v.huskAttach(husk.Native.screenWidth(), husk.Native.screenHeight());
        onContentChanged();
    }
    public void setContentView(int layoutId) { android.util.Log.w("Husk", "setContentView(layout " + layoutId + ") is not supported yet"); }
    public void addContentView(View v, ViewGroup.LayoutParams p) { if (mContent == null) setContentView(v, p); }
    @SuppressWarnings("unchecked") public <T extends View> T findViewById(int id) { return mContent == null ? null : (T) mContent.findViewById(id); }
    public LayoutInflater getLayoutInflater() { return new LayoutInflater(this); }
    public Intent getIntent() { return mIntent; }
    public void setIntent(Intent i) { mIntent = i; }
    public final Application getApplication() { if (mApplication == null) mApplication = new Application(); return mApplication; }
    public void finish() { mFinished = true; husk.Native.exit(); }
    public void finishAffinity() { finish(); }
    public boolean isFinishing() { return mFinished; }
    public boolean isDestroyed() { return false; }
    public boolean isChangingConfigurations() { return false; }
    public boolean hasWindowFocus() { return true; }
    public boolean moveTaskToBack(boolean nonRoot) { return true; }
    public void setRequestedOrientation(int o) { mOrientation = o; }
    public int getRequestedOrientation() { return mOrientation; }
    public void runOnUiThread(Runnable r) {
        if (Looper.myLooper() == Looper.getMainLooper()) r.run();
        else { if (mHandler == null) mHandler = new Handler(Looper.getMainLooper()); mHandler.post(r); }
    }
    public void startActivityForResult(Intent i, int request) { startActivity(i); }
    public void requestPermissions(String[] perms, int request) {
        int[] r = new int[perms.length];
        runOnUiThread(() -> onRequestPermissionsResult(request, perms, r));
    }
    public boolean shouldShowRequestPermissionRationale(String p) { return false; }
    public SharedPreferences getPreferences(int mode) { return getSharedPreferences(getClass().getName(), mode); }
    public void setVolumeControlStream(int stream) {}
    public int getVolumeControlStream() { return 3; }
    public void overridePendingTransition(int a, int b) {}
    public void setTitle(CharSequence t) {}
    public void setResult(int code) {}
    public void setResult(int code, Intent data) {}
    public ComponentName getComponentName() { return new ComponentName(this, getClass()); }
    public void registerForContextMenu(View v) {}
    public void openOptionsMenu() {}
    public boolean isInMultiWindowMode() { return false; }
    public void setTaskDescription(Object d) {}
    public void reportFullyDrawn() {}
    public void startIntentSenderForResult(Object a, int b, Intent c, int d, int e, int f) {}
}
