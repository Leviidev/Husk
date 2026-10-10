package android.text.style;
public class ForegroundColorSpan extends CharacterStyle implements UpdateAppearance, android.text.ParcelableSpan {
    private final int mColor;
    public ForegroundColorSpan(int color) { mColor = color; }
    public int getForegroundColor() { return mColor; }
    @Override public void updateDrawState(android.text.TextPaint tp) { tp.setColor(mColor); }
    public int getSpanTypeId() { return 2; } public int describeContents() { return 0; }
}
