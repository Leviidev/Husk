package android.transition;
public class AutoTransition extends TransitionSet { public AutoTransition() { setOrdering(ORDERING_SEQUENTIAL); addTransition(new Fade(Fade.OUT)).addTransition(new ChangeBounds()).addTransition(new Fade(Fade.IN)); } public AutoTransition(android.content.Context c, android.util.AttributeSet a) { this(); } }
