package android.text.style;
import android.graphics.Typeface;
public class TypefaceSpan extends MetricAffectingSpan implements android.text.ParcelableSpan {
    private final String mFamily; private final Typeface mTypeface;
    public TypefaceSpan(String family) { mFamily = family; mTypeface = null; }
    public TypefaceSpan(Typeface t) { mFamily = null; mTypeface = t; }
    public String getFamily() { return mFamily; }
    public Typeface getTypeface() { return mTypeface; }
    @Override public void updateDrawState(android.text.TextPaint tp) { apply(tp); }
    @Override public void updateMeasureState(android.text.TextPaint tp) { apply(tp); }
    private void apply(android.graphics.Paint p) { Typeface old = p.getTypeface(); int st = old == null ? 0 : old.getStyle(); p.setTypeface(mTypeface != null ? mTypeface : Typeface.create(mFamily, st)); }
    public int getSpanTypeId() { return 13; } public int describeContents() { return 0; }
}
