package android.transition;
public abstract class Visibility extends Transition {
    public static final int MODE_IN = 1, MODE_OUT = 2;
    private int mMode = MODE_IN | MODE_OUT;
    public Visibility() {}
    public Visibility(android.content.Context c, android.util.AttributeSet a) { super(c, a); }
    public void setMode(int m) { mMode = m; } public int getMode() { return mMode; }
    public void captureStartValues(TransitionValues v) {} public void captureEndValues(TransitionValues v) {}
    public boolean isVisible(TransitionValues v) { return v != null && v.view != null && v.view.getVisibility() == android.view.View.VISIBLE; }
    public android.animation.Animator onAppear(android.view.ViewGroup r, TransitionValues s, int sv, TransitionValues e, int ev) { return null; }
    public android.animation.Animator onAppear(android.view.ViewGroup r, android.view.View v, TransitionValues s, TransitionValues e) { return null; }
    public android.animation.Animator onDisappear(android.view.ViewGroup r, TransitionValues s, int sv, TransitionValues e, int ev) { return null; }
    public android.animation.Animator onDisappear(android.view.ViewGroup r, android.view.View v, TransitionValues s, TransitionValues e) { return null; }
}
