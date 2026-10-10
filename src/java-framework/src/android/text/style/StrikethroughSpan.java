package android.text.style;
public class StrikethroughSpan extends CharacterStyle implements UpdateAppearance, android.text.ParcelableSpan {
    public StrikethroughSpan() {}
    @Override public void updateDrawState(android.text.TextPaint tp) { tp.setStrikeThruText(true); }
    public int getSpanTypeId() { return 5; } public int describeContents() { return 0; }
}
