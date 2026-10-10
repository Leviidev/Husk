package android.text.style;
public class BulletSpan implements LeadingMarginSpan, android.text.ParcelableSpan {
    public static final int STANDARD_GAP_WIDTH = 2;
    private final int mGap, mColor, mRadius; private final boolean mWantColor;
    public BulletSpan() { this(STANDARD_GAP_WIDTH, 0, false, 4); }
    public BulletSpan(int gap) { this(gap, 0, false, 4); }
    public BulletSpan(int gap, int color) { this(gap, color, true, 4); }
    public BulletSpan(int gap, int color, int radius) { this(gap, color, true, radius); }
    private BulletSpan(int gap, int color, boolean wantColor, int radius) { mGap = gap; mColor = color; mWantColor = wantColor; mRadius = radius; }
    public int getLeadingMargin(boolean first) { return 2 * mRadius + mGap; }
    public void drawLeadingMargin(android.graphics.Canvas c, android.graphics.Paint p, int x, int dir, int top, int baseline, int bottom, CharSequence text, int start, int end, boolean first, android.text.Layout l) {
        if (!first) return;
        int old = p.getColor(); android.graphics.Paint.Style os = p.getStyle();
        if (mWantColor) p.setColor(mColor);
        p.setStyle(android.graphics.Paint.Style.FILL);
        c.drawCircle(x + dir * mRadius, (top + bottom) / 2f, mRadius, p);
        p.setColor(old); p.setStyle(os);
    }
    public int getGapWidth() { return mGap; } public int getBulletRadius() { return mRadius; } public int getColor() { return mColor; }
    public int getSpanTypeId() { return 8; } public int describeContents() { return 0; }
}
