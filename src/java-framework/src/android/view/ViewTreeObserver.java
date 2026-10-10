package android.view;

import java.util.concurrent.CopyOnWriteArrayList;

public final class ViewTreeObserver {
    public interface OnGlobalLayoutListener { void onGlobalLayout(); }
    public interface OnPreDrawListener { boolean onPreDraw(); }
    public interface OnDrawListener { void onDraw(); }
    public interface OnWindowFocusChangeListener { void onWindowFocusChanged(boolean f); }
    public interface OnWindowAttachListener { void onWindowAttached(); void onWindowDetached(); }
    public interface OnGlobalFocusChangeListener { void onGlobalFocusChanged(View oldFocus, View newFocus); }
    public interface OnScrollChangedListener { void onScrollChanged(); }
    public interface OnTouchModeChangeListener { void onTouchModeChanged(boolean inTouchMode); }
    public interface OnWindowVisibilityChangeListener { void onWindowVisibilityChanged(int v); }
    private final CopyOnWriteArrayList<OnGlobalLayoutListener> mLayout = new CopyOnWriteArrayList<>();
    private final CopyOnWriteArrayList<OnPreDrawListener> mPreDraw = new CopyOnWriteArrayList<>();
    private final CopyOnWriteArrayList<OnDrawListener> mDraw = new CopyOnWriteArrayList<>();
    private final CopyOnWriteArrayList<OnWindowFocusChangeListener> mFocus = new CopyOnWriteArrayList<>();
    private final CopyOnWriteArrayList<OnWindowAttachListener> mAttach = new CopyOnWriteArrayList<>();
    private final CopyOnWriteArrayList<OnGlobalFocusChangeListener> mGlobalFocus = new CopyOnWriteArrayList<>();
    private final CopyOnWriteArrayList<OnScrollChangedListener> mScroll = new CopyOnWriteArrayList<>();
    private final CopyOnWriteArrayList<OnTouchModeChangeListener> mTouchMode = new CopyOnWriteArrayList<>();
    private final CopyOnWriteArrayList<Runnable> mFrameCommit = new CopyOnWriteArrayList<>();
    public ViewTreeObserver() {}
    void merge(ViewTreeObserver o) {
        mLayout.addAll(o.mLayout); mPreDraw.addAll(o.mPreDraw); mDraw.addAll(o.mDraw); mFocus.addAll(o.mFocus); mAttach.addAll(o.mAttach);
        mGlobalFocus.addAll(o.mGlobalFocus); mScroll.addAll(o.mScroll); mTouchMode.addAll(o.mTouchMode);
    }
    public boolean isAlive() { return true; }
    public void addOnGlobalLayoutListener(OnGlobalLayoutListener l) { mLayout.add(l); }
    public void removeOnGlobalLayoutListener(OnGlobalLayoutListener l) { mLayout.remove(l); }
    @Deprecated public void removeGlobalOnLayoutListener(OnGlobalLayoutListener l) { mLayout.remove(l); }
    public void addOnPreDrawListener(OnPreDrawListener l) { mPreDraw.add(l); }
    public void removeOnPreDrawListener(OnPreDrawListener l) { mPreDraw.remove(l); }
    public void addOnDrawListener(OnDrawListener l) { mDraw.add(l); }
    public void removeOnDrawListener(OnDrawListener l) { mDraw.remove(l); }
    public void addOnWindowFocusChangeListener(OnWindowFocusChangeListener l) { mFocus.add(l); }
    public void removeOnWindowFocusChangeListener(OnWindowFocusChangeListener l) { mFocus.remove(l); }
    public void addOnWindowAttachListener(OnWindowAttachListener l) { mAttach.add(l); }
    public void removeOnWindowAttachListener(OnWindowAttachListener l) { mAttach.remove(l); }
    public void addOnGlobalFocusChangeListener(OnGlobalFocusChangeListener l) { mGlobalFocus.add(l); }
    public void removeOnGlobalFocusChangeListener(OnGlobalFocusChangeListener l) { mGlobalFocus.remove(l); }
    public void addOnScrollChangedListener(OnScrollChangedListener l) { mScroll.add(l); }
    public void removeOnScrollChangedListener(OnScrollChangedListener l) { mScroll.remove(l); }
    public void addOnTouchModeChangeListener(OnTouchModeChangeListener l) { mTouchMode.add(l); }
    public void removeOnTouchModeChangeListener(OnTouchModeChangeListener l) { mTouchMode.remove(l); }
    public void addOnWindowVisibilityChangeListener(OnWindowVisibilityChangeListener l) {}
    public void removeOnWindowVisibilityChangeListener(OnWindowVisibilityChangeListener l) {}
    public void registerFrameCommitCallback(Runnable r) { mFrameCommit.add(r); }
    public boolean unregisterFrameCommitCallback(Runnable r) { return mFrameCommit.remove(r); }
    public final void dispatchOnGlobalLayout() { for (OnGlobalLayoutListener l : mLayout) l.onGlobalLayout(); }
    void dispatchGlobalLayout() { dispatchOnGlobalLayout(); }
    public final boolean dispatchOnPreDraw() { boolean cancel = false; for (OnPreDrawListener l : mPreDraw) cancel |= !l.onPreDraw(); return cancel; }
    public final void dispatchOnDraw() { for (OnDrawListener l : mDraw) l.onDraw(); for (Runnable r : mFrameCommit) r.run(); mFrameCommit.clear(); }
    final void dispatchOnWindowFocusChange(boolean f) { for (OnWindowFocusChangeListener l : mFocus) l.onWindowFocusChanged(f); }
    final void dispatchOnWindowAttachedChange(boolean a) { for (OnWindowAttachListener l : mAttach) { if (a) l.onWindowAttached(); else l.onWindowDetached(); } }
    final void dispatchOnGlobalFocusChange(View o, View n) { for (OnGlobalFocusChangeListener l : mGlobalFocus) l.onGlobalFocusChanged(o, n); }
    final void dispatchOnScrollChanged() { for (OnScrollChangedListener l : mScroll) l.onScrollChanged(); }
    /** For husk.ViewRoot. */
    public void huskWindowFocus(boolean f) { dispatchOnWindowFocusChange(f); }
    public void huskAttached(boolean a) { dispatchOnWindowAttachedChange(a); }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public void addOnComputeInternalInsetsListener(android.view.ViewTreeObserver.OnComputeInternalInsetsListener p0) {}
    public void addOnEnterAnimationCompleteListener(android.view.ViewTreeObserver.OnEnterAnimationCompleteListener p0) {}
    public void addOnSystemGestureExclusionRectsChangedListener(java.util.function.Consumer p0) {}
    public void addOnWindowShownListener(android.view.ViewTreeObserver.OnWindowShownListener p0) {}
    public void dispatchOnEnterAnimationComplete() {}
    public void dispatchOnWindowShown() {}
    public void removeOnComputeInternalInsetsListener(android.view.ViewTreeObserver.OnComputeInternalInsetsListener p0) {}
    public void removeOnEnterAnimationCompleteListener(android.view.ViewTreeObserver.OnEnterAnimationCompleteListener p0) {}
    public void removeOnSystemGestureExclusionRectsChangedListener(java.util.function.Consumer p0) {}
    public void removeOnWindowShownListener(android.view.ViewTreeObserver.OnWindowShownListener p0) {}
    // ---- end of generated members
    // ---- generated by tools/compat/genstubs.py: the platform's nested classes this class does not write
    public static final class InternalInsetsInfo {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public static final int TOUCHABLE_INSETS_CONTENT = 1;
        public static final int TOUCHABLE_INSETS_FRAME = 0;
        public static final int TOUCHABLE_INSETS_REGION = 3;
        public static final int TOUCHABLE_INSETS_VISIBLE = 2;
        public android.graphics.Rect contentInsets;
        public android.graphics.Region touchableRegion;
        public android.graphics.Rect visibleInsets;
        public InternalInsetsInfo() {}
        public void setTouchableInsets(int p0) { huskProps.put("TouchableInsets", Integer.valueOf(p0)); }
    }
    public interface OnComputeInternalInsetsListener {
        void onComputeInternalInsets(android.view.ViewTreeObserver.InternalInsetsInfo p0);
    }
    public interface OnEnterAnimationCompleteListener {
        void onEnterAnimationComplete();
    }
    public interface OnWindowShownListener {
        void onWindowShown();
    }
    // ---- end of generated nested classes
}
