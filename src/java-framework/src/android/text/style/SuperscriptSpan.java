package android.text.style;
public class SuperscriptSpan extends MetricAffectingSpan {
    public SuperscriptSpan() {}
    @Override public void updateDrawState(android.text.TextPaint tp) { tp.baselineShift += (int) (tp.ascent() / 2); }
    @Override public void updateMeasureState(android.text.TextPaint tp) { tp.baselineShift += (int) (tp.ascent() / 2); }
}
