package android.animation;
public abstract class Keyframe implements Cloneable {
    float mFraction; TimeInterpolator mInterpolator;
    public static Keyframe ofFloat(float f, float v) { FloatKeyframe k = new FloatKeyframe(); k.mFraction = f; k.v = v; return k; }
    public static Keyframe ofFloat(float f) { return ofFloat(f, 0); }
    public static Keyframe ofInt(float f, int v) { IntKeyframe k = new IntKeyframe(); k.mFraction = f; k.v = v; return k; }
    public static Keyframe ofInt(float f) { return ofInt(f, 0); }
    public static Keyframe ofObject(float f, Object v) { ObjectKeyframe k = new ObjectKeyframe(); k.mFraction = f; k.v = v; return k; }
    public abstract Object getValue();
    public float getFraction() { return mFraction; }
    public void setFraction(float f) { mFraction = f; }
    public TimeInterpolator getInterpolator() { return mInterpolator; }
    public void setInterpolator(TimeInterpolator i) { mInterpolator = i; }
    public boolean hasValue() { return true; }
    static class FloatKeyframe extends Keyframe { float v; public Object getValue() { return v; } }
    static class IntKeyframe extends Keyframe { int v; public Object getValue() { return v; } }
    static class ObjectKeyframe extends Keyframe { Object v; public Object getValue() { return v; } }
    @Override public Keyframe clone() { try { return (Keyframe) super.clone(); } catch (CloneNotSupportedException e) { throw new AssertionError(); } }
}
