package android.text.style;
public abstract class MetricAffectingSpan extends CharacterStyle implements UpdateLayout {
    public abstract void updateMeasureState(android.text.TextPaint tp);
    @Override public MetricAffectingSpan getUnderlying() { return this; }
}
