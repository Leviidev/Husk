package android.graphics.drawable;

public class StateListDrawable extends DrawableContainer {
    private final java.util.ArrayList<int[]> mSets = new java.util.ArrayList<>();
    public StateListDrawable() {}
    public void addState(int[] set, Drawable d) { mSets.add(set); addChild(d); onStateChange(getState()); }
    public int getStateCount() { return mSets.size(); }
    public int[] getStateSet(int i) { return mSets.get(i); }
    public Drawable getStateDrawable(int i) { return mChildren.get(i); }
    public int findStateDrawableIndex(int[] s) { for (int i = 0; i < mSets.size(); i++) if (android.util.StateSet.stateSetMatches(mSets.get(i), s)) return i; return -1; }
    @Override protected boolean onStateChange(int[] s) {
        int i = findStateDrawableIndex(s);
        boolean ch = selectDrawable(i);
        return super.onStateChange(s) || ch;
    }
    protected final void huskCopyStateList(StateListDrawable src, android.content.res.Resources r, android.content.res.Resources.Theme t) {
        for (int[] set : src.mSets) mSets.add(set);
        huskCopyContainer(src, r, t);
        onStateChange(getState());
    }
    @Override public ConstantState getConstantState() {
        if (!huskCanCloneChildren()) return null;
        final StateListDrawable src = this;
        return new ConstantState() {
            @Override public Drawable newDrawable() { return newDrawable(null, null); }
            @Override public Drawable newDrawable(android.content.res.Resources r) { return newDrawable(r, null); }
            @Override public Drawable newDrawable(android.content.res.Resources r, android.content.res.Resources.Theme t) {
                StateListDrawable s = new StateListDrawable();
                s.huskCopyStateList(src, r, t);
                return s;
            }
            @Override public int getChangingConfigurations() { return src.getChangingConfigurations(); }
        };
    }
}
