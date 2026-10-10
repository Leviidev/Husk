package android.text.style;
public abstract class ReplacementSpan extends MetricAffectingSpan {
    public abstract int getSize(android.graphics.Paint paint, CharSequence text, int start, int end, android.graphics.Paint.FontMetricsInt fm);
    public abstract void draw(android.graphics.Canvas canvas, CharSequence text, int start, int end, float x, int top, int y, int bottom, android.graphics.Paint paint);
    public CharSequence getContentDescription() { return null; } public void setContentDescription(CharSequence c) {}
    @Override public void updateMeasureState(android.text.TextPaint p) {}
    @Override public void updateDrawState(android.text.TextPaint ds) {}
}
