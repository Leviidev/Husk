package android.text.style;
public class QuoteSpan implements LeadingMarginSpan, android.text.ParcelableSpan {
    private final int mColor;
    public QuoteSpan() { this(0xff0000ff); } public QuoteSpan(int color) { mColor = color; } public QuoteSpan(int c, int s, int g) { this(c); }
    public int getColor() { return mColor; } public int getStripeWidth() { return 2; } public int getGapWidth() { return 2; }
    public int getLeadingMargin(boolean first) { return 4; }
    public void drawLeadingMargin(android.graphics.Canvas c, android.graphics.Paint p, int x, int dir, int top, int baseline, int bottom, CharSequence text, int start, int end, boolean first, android.text.Layout l) {
        int old = p.getColor(); p.setColor(mColor); c.drawRect(x, top, x + dir * 2, bottom, p); p.setColor(old);
    }
    public int getSpanTypeId() { return 9; } public int describeContents() { return 0; }
}
