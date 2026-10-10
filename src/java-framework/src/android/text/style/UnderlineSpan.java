package android.text.style;
public class UnderlineSpan extends CharacterStyle implements UpdateAppearance, android.text.ParcelableSpan {
    public UnderlineSpan() {}
    @Override public void updateDrawState(android.text.TextPaint tp) { tp.setUnderlineText(true); }
    public int getSpanTypeId() { return 6; } public int describeContents() { return 0; }
}
