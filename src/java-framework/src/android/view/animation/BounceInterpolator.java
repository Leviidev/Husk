package android.view.animation;
public class BounceInterpolator extends BaseInterpolator {
    public BounceInterpolator() {}
    public BounceInterpolator(android.content.Context c, android.util.AttributeSet a) {}
    private static float b(float t) { return t * t * 8.0f; }
    public float getInterpolation(float t) {
        t *= 1.1226f;
        if (t < 0.3535f) return b(t);
        else if (t < 0.7408f) return b(t - 0.54719f) + 0.7f;
        else if (t < 0.9644f) return b(t - 0.8526f) + 0.9f;
        else return b(t - 1.0435f) + 0.95f;
    }
}
