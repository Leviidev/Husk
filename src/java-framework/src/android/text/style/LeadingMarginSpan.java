package android.text.style;
public interface LeadingMarginSpan extends ParagraphStyle {
    int getLeadingMargin(boolean first);
    void drawLeadingMargin(android.graphics.Canvas c, android.graphics.Paint p, int x, int dir, int top, int baseline, int bottom, CharSequence text, int start, int end, boolean first, android.text.Layout layout);
    interface LeadingMarginSpan2 extends LeadingMarginSpan, WrapTogetherSpan { int getLeadingMarginLineCount(); }
    class Standard implements LeadingMarginSpan, android.text.ParcelableSpan {
        private final int mFirst, mRest;
        public Standard(int first, int rest) { mFirst = first; mRest = rest; }
        public Standard(int every) { this(every, every); }
        public int getLeadingMargin(boolean first) { return first ? mFirst : mRest; }
        public void drawLeadingMargin(android.graphics.Canvas c, android.graphics.Paint p, int x, int dir, int top, int baseline, int bottom, CharSequence text, int start, int end, boolean first, android.text.Layout layout) {}
        public int getSpanTypeId() { return 10; } public int describeContents() { return 0; }
    }
}
