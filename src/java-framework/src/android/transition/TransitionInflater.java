package android.transition;
public class TransitionInflater {
    private final android.content.Context mContext;
    private TransitionInflater(android.content.Context c) { mContext = c; }
    public static TransitionInflater from(android.content.Context c) { return new TransitionInflater(c); }
    public Transition inflateTransition(int resource) { return new AutoTransition(); }
    public TransitionManager inflateTransitionManager(int resource, android.view.ViewGroup root) { return new TransitionManager(); }
}
