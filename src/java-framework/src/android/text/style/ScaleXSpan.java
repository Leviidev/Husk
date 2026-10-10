package android.text.style;
public class ScaleXSpan extends MetricAffectingSpan {
    private final float mProportion;
    public ScaleXSpan(float p) { mProportion = p; }
    public float getScaleX() { return mProportion; }
    @Override public void updateDrawState(android.text.TextPaint tp) { tp.setTextScaleX(tp.getTextScaleX() * mProportion); }
    @Override public void updateMeasureState(android.text.TextPaint tp) { tp.setTextScaleX(tp.getTextScaleX() * mProportion); }
}
