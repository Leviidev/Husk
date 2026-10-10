package android.text;

/** A layout of text that changes: laid out again (as a StaticLayout) when the text has. */
public class DynamicLayout extends Layout {
    private final CharSequence mBase; private final boolean mIncludePad; private final TextUtils.TruncateAt mEllipsize; private final int mEllipsizedWidth;
    private StaticLayout mLayout; private String mLast;
    public DynamicLayout(CharSequence base, TextPaint paint, int width, Alignment align, float mult, float add, boolean includePad) { this(base, base, paint, width, align, mult, add, includePad); }
    public DynamicLayout(CharSequence base, CharSequence display, TextPaint paint, int width, Alignment align, float mult, float add, boolean includePad) { this(base, display, paint, width, align, mult, add, includePad, null, 0); }
    public DynamicLayout(CharSequence base, CharSequence display, TextPaint paint, int width, Alignment align, float mult, float add, boolean includePad, TextUtils.TruncateAt ellipsize, int ellipsizedWidth) {
        super(display, paint, width, align, mult, add); mBase = display; mIncludePad = includePad; mEllipsize = ellipsize; mEllipsizedWidth = ellipsizedWidth;
    }
    public static final class Builder {
        private final CharSequence b; private final TextPaint p; private final int w; private Alignment a = Alignment.ALIGN_NORMAL; private float m = 1, ad; private boolean pad = true; private TextUtils.TruncateAt e; private int ew; private CharSequence d;
        private Builder(CharSequence b, TextPaint p, int w) { this.b = b; this.p = p; this.w = w; ew = w; d = b; }
        public static Builder obtain(CharSequence base, TextPaint p, int w) { return new Builder(base, p, w); }
        public Builder setDisplayText(CharSequence t) { d = t; return this; } public Builder setAlignment(Alignment x) { a = x; return this; } public Builder setTextDirection(TextDirectionHeuristic x) { return this; }
        public Builder setLineSpacing(float add, float mult) { ad = add; m = mult; return this; } public Builder setIncludePad(boolean x) { pad = x; return this; } public Builder setUseLineSpacingFromFallbacks(boolean x) { return this; }
        public Builder setBreakStrategy(int s) { return this; } public Builder setHyphenationFrequency(int f) { return this; } public Builder setJustificationMode(int j) { return this; }
        public Builder setEllipsize(TextUtils.TruncateAt x) { e = x; return this; } public Builder setEllipsizedWidth(int x) { ew = x; return this; }
        public DynamicLayout build() { return new DynamicLayout(b, d, p, w, a, m, ad, pad, e, ew); }
    }
    private StaticLayout layout() {
        String now = mBase.toString();
        if (mLayout == null || !now.equals(mLast) || mBase instanceof Spanned) {
            mLast = now;
            mLayout = new StaticLayout(mBase, 0, mBase.length(), getPaint(), getWidth(), getAlignment(), getSpacingMultiplier(), getSpacingAdd(), mIncludePad, mEllipsize, mEllipsizedWidth > 0 ? mEllipsizedWidth : getWidth());
        }
        return mLayout;
    }
    public int getLineCount() { return layout().getLineCount(); }
    public int getLineTop(int l) { return layout().getLineTop(l); }
    public int getLineDescent(int l) { return layout().getLineDescent(l); }
    public int getLineStart(int l) { return layout().getLineStart(l); }
    public int getParagraphDirection(int l) { return DIR_LEFT_TO_RIGHT; }
    public boolean getLineContainsTab(int l) { return false; }
    public Directions getLineDirections(int l) { return DIRS_ALL_LEFT_TO_RIGHT; }
    public int getTopPadding() { return layout().getTopPadding(); }
    public int getBottomPadding() { return layout().getBottomPadding(); }
    public int getEllipsisStart(int l) { return layout().getEllipsisStart(l); }
    public int getEllipsisCount(int l) { return layout().getEllipsisCount(l); }
    public int getEllipsizedWidth() { return mEllipsizedWidth > 0 ? mEllipsizedWidth : getWidth(); }
    public void draw(android.graphics.Canvas c, android.graphics.Path h, android.graphics.Paint hp, int off) { layout().draw(c, h, hp, off); }
}
