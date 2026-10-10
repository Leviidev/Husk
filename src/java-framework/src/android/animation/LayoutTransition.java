package android.animation;
public class LayoutTransition {
    public static final int CHANGE_APPEARING = 0, CHANGE_DISAPPEARING = 1, APPEARING = 2, DISAPPEARING = 3, CHANGING = 4;
    public interface TransitionListener { void startTransition(LayoutTransition t, android.view.ViewGroup c, android.view.View v, int type); void endTransition(LayoutTransition t, android.view.ViewGroup c, android.view.View v, int type); }
    public void setDuration(long d) {} public void setDuration(int t, long d) {} public void enableTransitionType(int t) {} public void disableTransitionType(int t) {}
    public void setAnimator(int t, Animator a) {} public void setInterpolator(int t, TimeInterpolator i) {} public void setStartDelay(int t, long d) {}
    public void addTransitionListener(TransitionListener l) {} public void setAnimateParentHierarchy(boolean b) {} public boolean isRunning() { return false; }
}
