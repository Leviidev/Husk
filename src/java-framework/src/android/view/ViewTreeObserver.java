package android.view;

import java.util.ArrayList;

public final class ViewTreeObserver {
    public interface OnGlobalLayoutListener { void onGlobalLayout(); }
    public interface OnPreDrawListener { boolean onPreDraw(); }
    public interface OnWindowFocusChangeListener { void onWindowFocusChanged(boolean f); }
    private final ArrayList<OnGlobalLayoutListener> layout = new ArrayList<>();
    public void addOnGlobalLayoutListener(OnGlobalLayoutListener l) { layout.add(l); }
    public void removeOnGlobalLayoutListener(OnGlobalLayoutListener l) { layout.remove(l); }
    public void removeGlobalOnLayoutListener(OnGlobalLayoutListener l) { layout.remove(l); }
    public void addOnPreDrawListener(OnPreDrawListener l) {}
    public void removeOnPreDrawListener(OnPreDrawListener l) {}
    public void addOnWindowFocusChangeListener(OnWindowFocusChangeListener l) {}
    public boolean isAlive() { return true; }
    void dispatchGlobalLayout() { for (OnGlobalLayoutListener l : new ArrayList<>(layout)) l.onGlobalLayout(); }
}
