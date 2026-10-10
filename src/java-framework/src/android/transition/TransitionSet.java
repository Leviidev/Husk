package android.transition;
import java.util.ArrayList;
public class TransitionSet extends Transition {
    public static final int ORDERING_TOGETHER = 0, ORDERING_SEQUENTIAL = 1;
    private final ArrayList<Transition> mTransitions = new ArrayList<>();
    private int mOrdering = ORDERING_TOGETHER;
    public TransitionSet() {}
    public TransitionSet(android.content.Context c, android.util.AttributeSet a) { super(c, a); }
    public TransitionSet setOrdering(int o) { mOrdering = o; return this; } public int getOrdering() { return mOrdering; }
    public TransitionSet addTransition(Transition t) { if (t != null) mTransitions.add(t); return this; }
    public int getTransitionCount() { return mTransitions.size(); }
    public Transition getTransitionAt(int i) { return i < 0 || i >= mTransitions.size() ? null : mTransitions.get(i); }
    public TransitionSet removeTransition(Transition t) { mTransitions.remove(t); return this; }
    @Override public TransitionSet setDuration(long d) { super.setDuration(d); for (Transition t : mTransitions) t.setDuration(d); return this; }
    @Override public TransitionSet setStartDelay(long d) { super.setStartDelay(d); return this; }
    @Override public TransitionSet setInterpolator(android.animation.TimeInterpolator i) { super.setInterpolator(i); return this; }
    @Override public TransitionSet addTarget(android.view.View v) { super.addTarget(v); return this; }
    @Override public TransitionSet addTarget(int id) { super.addTarget(id); return this; }
    @Override public TransitionSet addTarget(String n) { return this; }
    @Override public TransitionSet addTarget(Class<?> c) { return this; }
    @Override public TransitionSet addListener(TransitionListener l) { super.addListener(l); return this; }
    @Override public TransitionSet removeListener(TransitionListener l) { super.removeListener(l); return this; }
    @Override public TransitionSet removeTarget(int id) { super.removeTarget(id); return this; }
    @Override public TransitionSet removeTarget(android.view.View v) { super.removeTarget(v); return this; }
    @Override public TransitionSet removeTarget(Class<?> c) { return this; }
    @Override public TransitionSet removeTarget(String n) { return this; }
    public void captureStartValues(TransitionValues v) {} public void captureEndValues(TransitionValues v) {}
    @Override void huskRun() { super.huskRun(); for (Transition t : mTransitions) t.huskRun(); }
    @Override public TransitionSet clone() { TransitionSet c = (TransitionSet) super.clone(); return c; }
}
