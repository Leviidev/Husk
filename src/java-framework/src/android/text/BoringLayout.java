package android.text;

/** One line of plain text: what StaticLayout does with a single line, measured directly. */
public class BoringLayout extends Layout implements TextUtils.EllipsizeCallback {
    public static class Metrics extends android.graphics.Paint.FontMetricsInt { public int width; public android.graphics.RectF getDrawingBoundingBox() { return new android.graphics.RectF(0, top, width, bottom); } }
    private StaticLayout mInner;
    public BoringLayout(CharSequence source, TextPaint paint, int outerwidth, Alignment align, float mult, float add, Metrics metrics, boolean includePad) {
        super(source, paint, outerwidth, align, mult, add);
        mInner = new StaticLayout(source, paint, outerwidth, align, mult, add, includePad);
    }
    public BoringLayout(CharSequence source, TextPaint paint, int outerwidth, Alignment align, float mult, float add, Metrics metrics, boolean includePad, TextUtils.TruncateAt ellipsize, int ellipsizedWidth) {
        super(source, paint, outerwidth, align, mult, add);
        mInner = new StaticLayout(source, 0, source.length(), paint, outerwidth, align, mult, add, includePad, ellipsize, ellipsizedWidth);
    }
    public static BoringLayout make(CharSequence source, TextPaint paint, int outerwidth, Alignment align, float mult, float add, Metrics metrics, boolean includePad) { return new BoringLayout(source, paint, outerwidth, align, mult, add, metrics, includePad); }
    public static BoringLayout make(CharSequence source, TextPaint paint, int outerwidth, Alignment align, float mult, float add, Metrics metrics, boolean includePad, TextUtils.TruncateAt ellipsize, int ellipsizedWidth) { return new BoringLayout(source, paint, outerwidth, align, mult, add, metrics, includePad, ellipsize, ellipsizedWidth); }
    public BoringLayout replaceOrMake(CharSequence source, TextPaint paint, int outerwidth, Alignment align, float mult, float add, Metrics metrics, boolean includePad) { return make(source, paint, outerwidth, align, mult, add, metrics, includePad); }
    public BoringLayout replaceOrMake(CharSequence source, TextPaint paint, int outerwidth, Alignment align, float mult, float add, Metrics metrics, boolean includePad, TextUtils.TruncateAt e, int ew) { return make(source, paint, outerwidth, align, mult, add, metrics, includePad, e, ew); }
    public static Metrics isBoring(CharSequence text, TextPaint paint) { return isBoring(text, paint, null); }
    public static Metrics isBoring(CharSequence text, TextPaint paint, Metrics metrics) {
        if (TextUtils.indexOf(text, '\n') >= 0 || TextUtils.indexOf(text, '\t') >= 0) return null;
        if (text instanceof Spanned && ((Spanned) text).getSpans(0, text.length(), android.text.style.ParagraphStyle.class).length > 0) return null;
        Metrics m = metrics != null ? metrics : new Metrics();
        paint.getFontMetricsInt(m);
        m.width = (int) Math.ceil(husk.TextRuns.measure(text, 0, text.length(), paint));
        return m;
    }
    public static Metrics isBoring(CharSequence text, TextPaint paint, TextDirectionHeuristic d, Metrics m) { return isBoring(text, paint, m); }
    public int getHeight() { return mInner.getHeight(); }
    public int getLineCount() { return mInner.getLineCount(); }
    public int getLineTop(int l) { return mInner.getLineTop(l); }
    public int getLineDescent(int l) { return mInner.getLineDescent(l); }
    public int getLineStart(int l) { return mInner.getLineStart(l); }
    public int getParagraphDirection(int l) { return DIR_LEFT_TO_RIGHT; }
    public boolean getLineContainsTab(int l) { return false; }
    public float getLineMax(int l) { return mInner.getLineMax(l); }
    public float getLineWidth(int l) { return mInner.getLineWidth(l); }
    public Directions getLineDirections(int l) { return DIRS_ALL_LEFT_TO_RIGHT; }
    public int getTopPadding() { return mInner.getTopPadding(); }
    public int getBottomPadding() { return mInner.getBottomPadding(); }
    public int getEllipsisCount(int l) { return mInner.getEllipsisCount(l); }
    public int getEllipsisStart(int l) { return mInner.getEllipsisStart(l); }
    public int getEllipsizedWidth() { return mInner.getEllipsizedWidth(); }
    public void draw(android.graphics.Canvas c, android.graphics.Path h, android.graphics.Paint hp, int off) { mInner.draw(c, h, hp, off); }
    public void ellipsized(int s, int e) {}
}
