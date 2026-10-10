package android.text.style;
public interface LineBackgroundSpan extends ParagraphStyle {
    void drawBackground(android.graphics.Canvas c, android.graphics.Paint p, int left, int right, int top, int baseline, int bottom, CharSequence text, int start, int end, int lineNumber);
    class Standard implements LineBackgroundSpan { private final int mColor; public Standard(int c) { mColor = c; } public int getColor() { return mColor; } public void drawBackground(android.graphics.Canvas c, android.graphics.Paint p, int l, int r, int t, int b2, int b, CharSequence tx, int s, int e, int n) { int o = p.getColor(); p.setColor(mColor); c.drawRect(l, t, r, b, p); p.setColor(o); } }
}
