package android.transition;
import android.view.View;
import android.view.ViewGroup;
public final class Scene {
    private final ViewGroup mSceneRoot; private View mLayout; private int mLayoutId = -1; private android.content.Context mContext; private Runnable mEnter, mExit;
    public Scene(ViewGroup root) { mSceneRoot = root; }
    public Scene(ViewGroup root, View layout) { mSceneRoot = root; mLayout = layout; }
    @Deprecated public Scene(ViewGroup root, ViewGroup layout) { this(root, (View) layout); }
    private Scene(ViewGroup root, int id, android.content.Context c) { mSceneRoot = root; mLayoutId = id; mContext = c; }
    public static Scene getSceneForLayout(ViewGroup root, int layoutId, android.content.Context c) { return new Scene(root, layoutId, c); }
    public static Scene getCurrentScene(View v) { return null; }
    public ViewGroup getSceneRoot() { return mSceneRoot; }
    public void exit() { if (mExit != null) mExit.run(); }
    public void enter() {
        if (mLayoutId > 0 || mLayout != null) { mSceneRoot.removeAllViews(); if (mLayoutId > 0) android.view.LayoutInflater.from(mContext).inflate(mLayoutId, mSceneRoot); else mSceneRoot.addView(mLayout); }
        if (mEnter != null) mEnter.run();
    }
    public void setEnterAction(Runnable r) { mEnter = r; }
    public void setExitAction(Runnable r) { mExit = r; }
}
