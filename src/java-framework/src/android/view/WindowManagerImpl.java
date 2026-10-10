package android.view;

import android.content.Context;

/** Windows on the one screen: each added view gets a husk.ViewRoot, drawn in the order added, the last on top. */
public final class WindowManagerImpl implements WindowManager {
    private static final Display sDisplay = new Display();
    private final Context mContext;
    public WindowManagerImpl() { this(null); }
    public WindowManagerImpl(Context c) { mContext = c; }
    public Display getDefaultDisplay() { return sDisplay; }
    public void addView(View v, ViewGroup.LayoutParams p) {
        if (!(p instanceof WindowManager.LayoutParams)) throw new IllegalArgumentException("Params must be WindowManager.LayoutParams");
        if (v.getParent() != null && v.mRoot != null) throw new IllegalStateException("View " + v + " has already been added to the window manager.");
        husk.ViewRoot.add(v, (WindowManager.LayoutParams) p);
    }
    public void updateViewLayout(View v, ViewGroup.LayoutParams p) {
        if (!(p instanceof WindowManager.LayoutParams)) throw new IllegalArgumentException("Params must be WindowManager.LayoutParams");
        husk.ViewRoot r = husk.ViewRoot.of(v);
        if (r == null) throw new IllegalArgumentException("View=" + v + " not attached to window manager");
        r.setLayoutParams((WindowManager.LayoutParams) p);
    }
    public void removeView(View v) { husk.ViewRoot r = husk.ViewRoot.of(v); if (r == null) throw new IllegalArgumentException("View=" + v + " not attached to window manager"); r.remove(); }
    public void removeViewImmediate(View v) { removeView(v); }
}
