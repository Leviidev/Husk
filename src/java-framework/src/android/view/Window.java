package android.view;

public class Window {
    public static final int FEATURE_NO_TITLE = 1, FEATURE_ACTION_BAR = 8;
    public interface Callback { boolean dispatchKeyEvent(KeyEvent e); boolean dispatchTouchEvent(MotionEvent e); void onWindowFocusChanged(boolean f); }
    private final android.content.Context ctx;
    private final WindowManager.LayoutParams attrs = new WindowManager.LayoutParams();
    private View decor;
    public Window(android.content.Context c) { ctx = c; }
    public void setContentView(View v) { if (decor == null) decor = new android.widget.FrameLayout(ctx); ((ViewGroup) decor).removeAllViews(); ((ViewGroup) decor).addView(v); }
    public View getDecorView() { if (decor == null) decor = new android.widget.FrameLayout(ctx); return decor; }
    public View peekDecorView() { return decor; }
    public WindowManager.LayoutParams getAttributes() { return attrs; }
    public void setAttributes(WindowManager.LayoutParams a) {}
    public void addFlags(int f) { attrs.flags |= f; }
    public void clearFlags(int f) { attrs.flags &= ~f; }
    public void setFlags(int f, int mask) { attrs.flags = (attrs.flags & ~mask) | (f & mask); }
    public boolean requestFeature(int f) { return true; }
    public void setSoftInputMode(int m) {}
    public void setFormat(int f) {}
    public void setLayout(int w, int h) {}
    public void setBackgroundDrawable(Object d) {}
    public void setBackgroundDrawableResource(int r) {}
    public void setStatusBarColor(int c) {}
    public void setNavigationBarColor(int c) {}
    public void setDecorFitsSystemWindows(boolean b) {}
    public WindowInsetsController getInsetsController() { return new WindowInsetsController(); }
    public android.content.Context getContext() { return ctx; }
    public void takeKeyEvents(boolean b) {}
}
