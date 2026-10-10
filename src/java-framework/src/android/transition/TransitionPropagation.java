package android.transition;
public abstract class TransitionPropagation { public abstract long getStartDelay(android.view.ViewGroup r, Transition t, TransitionValues s, TransitionValues e); public abstract void captureValues(TransitionValues v); public abstract String[] getPropagationProperties(); }
