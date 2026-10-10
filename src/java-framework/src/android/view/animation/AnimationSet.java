package android.view.animation;
import java.util.ArrayList;
import java.util.List;
public class AnimationSet extends Animation {
    private final ArrayList<Animation> mAnims = new ArrayList<>(); private final boolean mShare; private final Transformation mTmp = new Transformation();
    public AnimationSet(boolean shareInterpolator) { mShare = shareInterpolator; }
    public AnimationSet(android.content.Context c, android.util.AttributeSet a) { this(true); }
    public void addAnimation(Animation a) { mAnims.add(a); if (a.computeDurationHint() > mDuration) mDuration = a.computeDurationHint(); }
    public List<Animation> getAnimations() { return mAnims; }
    @Override public void setDuration(long d) { super.setDuration(d); for (Animation a : mAnims) a.setDuration(d); }
    @Override public void initialize(int w, int h, int pw, int ph) { super.initialize(w, h, pw, ph); for (Animation a : mAnims) { if (mShare) a.setInterpolator(mInterpolator); a.initialize(w, h, pw, ph); } }
    @Override public void reset() { super.reset(); for (Animation a : mAnims) a.reset(); }
    @Override public void setStartTime(long t) { super.setStartTime(t); for (Animation a : mAnims) a.setStartTime(t); }
    @Override public boolean getTransformation(long now, Transformation out) {
        boolean more = false;
        out.clear();
        if (mStartTime == -1) setStartTime(now);
        for (int i = mAnims.size() - 1; i >= 0; i--) { mTmp.clear(); more |= mAnims.get(i).getTransformation(now, mTmp); out.compose(mTmp); }
        if (!more && !mEnded) { mEnded = true; }
        if (!mStarted) mStarted = true;
        return more;
    }
}
