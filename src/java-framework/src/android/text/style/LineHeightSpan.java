package android.text.style;
public interface LineHeightSpan extends ParagraphStyle, WrapTogetherSpan {
    void chooseHeight(CharSequence text, int start, int end, int spanstartv, int lineHeight, android.graphics.Paint.FontMetricsInt fm);
    interface WithDensity extends LineHeightSpan { void chooseHeight(CharSequence text, int start, int end, int spanstartv, int lineHeight, android.graphics.Paint.FontMetricsInt fm, android.text.TextPaint p); }
    class Standard implements LineHeightSpan {
        private final int mHeight;
        public Standard(int h) { mHeight = h; }
        public int getHeight() { return mHeight; }
        public void chooseHeight(CharSequence text, int start, int end, int spanstartv, int lineHeight, android.graphics.Paint.FontMetricsInt fm) {
            int orig = fm.descent - fm.ascent; if (orig <= 0) return;
            float ratio = mHeight * 1.0f / orig;
            fm.descent = Math.round(fm.descent * ratio); fm.ascent = fm.descent - mHeight;
        }
    }
}
