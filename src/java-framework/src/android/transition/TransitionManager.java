package android.transition;
import android.view.ViewGroup;
public class TransitionManager {
    public TransitionManager() {}
    public void setTransition(Scene s, Transition t) {}
    public void setTransition(Scene from, Scene to, Transition t) {}
    public void transitionTo(Scene s) { go(s, null); }
    public static void go(Scene s) { go(s, null); }
    public static void go(Scene s, Transition t) { s.enter(); if (t != null) t.huskRun(); }
    public static void beginDelayedTransition(ViewGroup root) { }
    public static void beginDelayedTransition(ViewGroup root, Transition t) { if (t != null) root.post(t::huskRun); }
    public static void endTransitions(ViewGroup root) {}
}
