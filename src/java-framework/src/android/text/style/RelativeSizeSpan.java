package android.text.style;
public class RelativeSizeSpan extends MetricAffectingSpan implements android.text.ParcelableSpan {
    private final float mProportion;
    public RelativeSizeSpan(float p) { mProportion = p; }
    public float getSizeChange() { return mProportion; }
    @Override public void updateDrawState(android.text.TextPaint tp) { tp.setTextSize(tp.getTextSize() * mProportion); }
    @Override public void updateMeasureState(android.text.TextPaint tp) { tp.setTextSize(tp.getTextSize() * mProportion); }
    public int getSpanTypeId() { return 3; } public int describeContents() { return 0; }
}
