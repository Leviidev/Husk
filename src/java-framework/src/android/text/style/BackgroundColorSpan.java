package android.text.style;
public class BackgroundColorSpan extends CharacterStyle implements UpdateAppearance, android.text.ParcelableSpan {
    private final int mColor;
    public BackgroundColorSpan(int color) { mColor = color; }
    public int getBackgroundColor() { return mColor; }
    @Override public void updateDrawState(android.text.TextPaint tp) { tp.bgColor = mColor; }
    public int getSpanTypeId() { return 12; } public int describeContents() { return 0; }
}
