package android.graphics.drawable;

/** A StateListDrawable whose state changes would animate; Husk switches straight to the new state's drawable. */
public class AnimatedStateListDrawable extends StateListDrawable {
    public AnimatedStateListDrawable() { super(); }
    public void addState(int[] stateSet, Drawable d, int id) { addState(stateSet, d); }
    public <T extends Drawable & Animatable> void addTransition(int fromId, int toId, T transition, boolean reversible) {}
    @Override public ConstantState getConstantState() {
        if (!huskCanCloneChildren()) return null;
        final AnimatedStateListDrawable src = this;
        return new ConstantState() {
            @Override public Drawable newDrawable() { return newDrawable(null, null); }
            @Override public Drawable newDrawable(android.content.res.Resources r) { return newDrawable(r, null); }
            @Override public Drawable newDrawable(android.content.res.Resources r, android.content.res.Resources.Theme t) {
                AnimatedStateListDrawable s = new AnimatedStateListDrawable();
                s.huskCopyStateList(src, r, t);
                return s;
            }
            @Override public int getChangingConfigurations() { return src.getChangingConfigurations(); }
        };
    }
}
