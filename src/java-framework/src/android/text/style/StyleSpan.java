package android.text.style;
import android.graphics.Typeface;
public class StyleSpan extends MetricAffectingSpan implements android.text.ParcelableSpan {
    private final int mStyle; private final int mWeight;
    public StyleSpan(int style) { this(style, -1); }
    public StyleSpan(int style, int weight) { mStyle = style; mWeight = weight; }
    public int getStyle() { return mStyle; }
    public int getFontWeightAdjustment() { return mWeight; }
    @Override public void updateDrawState(android.text.TextPaint tp) { apply(tp, mStyle); }
    @Override public void updateMeasureState(android.text.TextPaint tp) { apply(tp, mStyle); }
    private static void apply(android.graphics.Paint p, int style) {
        Typeface old = p.getTypeface();
        int want = (old == null ? 0 : old.getStyle()) | style;
        p.setTypeface(Typeface.create(old, want));
    }
    public int getSpanTypeId() { return 7; } public int describeContents() { return 0; }
}
