package android.text.style;
public class SubscriptSpan extends MetricAffectingSpan {
    public SubscriptSpan() {}
    @Override public void updateDrawState(android.text.TextPaint tp) { tp.baselineShift -= (int) (tp.ascent() / 2); }
    @Override public void updateMeasureState(android.text.TextPaint tp) { tp.baselineShift -= (int) (tp.ascent() / 2); }
}
