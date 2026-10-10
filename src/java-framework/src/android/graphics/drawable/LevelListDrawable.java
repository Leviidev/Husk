package android.graphics.drawable;

public class LevelListDrawable extends DrawableContainer {
    private final java.util.ArrayList<int[]> mRanges = new java.util.ArrayList<>();
    public void addLevel(int low, int high, Drawable d) { mRanges.add(new int[] { low, high }); addChild(d); onLevelChange(getLevel()); }
    @Override protected boolean onLevelChange(int l) {
        for (int i = 0; i < mRanges.size(); i++) if (l >= mRanges.get(i)[0] && l <= mRanges.get(i)[1]) return selectDrawable(i);
        return selectDrawable(-1);
    }
}
