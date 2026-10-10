package android.text;

import android.graphics.Paint;
import android.text.style.*;

/** Text broken into lines at a width: words kept together, paragraphs at newlines, line heights from the fonts used, optional ellipsis. */
public class StaticLayout extends Layout {
    private int mLineCount;
    private int[] mStarts = new int[8], mTops = new int[9], mDescents = new int[8], mEllStart = new int[8], mEllCount = new int[8];
    private int mTopPad, mBottomPad, mEllipsizedWidth;
    private boolean mIncludePad = true;
    private int mMaxLines = Integer.MAX_VALUE;
    private TextUtils.TruncateAt mEllipsize;
    private final CharSequence mSource;
    private final int mStart, mEnd;

    public static final class Builder {
        CharSequence text; int start, end, width, maxLines = Integer.MAX_VALUE, ellipsizedWidth; TextPaint paint; Layout.Alignment align = Layout.Alignment.ALIGN_NORMAL;
        float mult = 1, add = 0; boolean includePad = true; TextUtils.TruncateAt ellipsize;
        private Builder() {}
        public static Builder obtain(CharSequence source, int start, int end, TextPaint paint, int width) {
            Builder b = new Builder(); b.text = source; b.start = start; b.end = end; b.paint = paint; b.width = width; b.ellipsizedWidth = width; return b;
        }
        public Builder setText(CharSequence t) { text = t; start = 0; end = t.length(); return this; }
        public Builder setAlignment(Layout.Alignment a) { align = a; return this; }
        public Builder setTextDirection(TextDirectionHeuristic d) { return this; }
        public Builder setLineSpacing(float add, float mult) { this.add = add; this.mult = mult; return this; }
        public Builder setIncludePad(boolean p) { includePad = p; return this; }
        public Builder setFallbackLineSpacing(boolean b) { return this; }
        public Builder setEllipsizedWidth(int w) { ellipsizedWidth = w; return this; }
        public Builder setEllipsize(TextUtils.TruncateAt e) { ellipsize = e; return this; }
        public Builder setMaxLines(int m) { maxLines = m; return this; }
        public Builder setBreakStrategy(int s) { return this; }
        public Builder setHyphenationFrequency(int f) { return this; }
        public Builder setIndents(int[] l, int[] r) { return this; }
        public Builder setJustificationMode(int m) { return this; }
        public Builder setUseLineSpacingFromFallbacks(boolean b) { return this; }
        public Builder setLineBreakConfig(Object c) { return this; }
        public Builder setUseBoundsForWidth(boolean b) { return this; }
        public Builder setShiftDrawingOffsetForStartOverhang(boolean b) { return this; }
        public Builder setMinimumFontMetrics(Paint.FontMetrics m) { return this; }
        public StaticLayout build() { return new StaticLayout(this); }
    }
    private StaticLayout(Builder b) {
        super(b.ellipsize != null ? (b.text instanceof Spanned ? new SpannableStringBuilder(b.text, b.start, b.end) : b.text.subSequence(b.start, b.end)) : b.text, b.paint, b.width, b.align, b.mult, b.add);
        mSource = b.ellipsize != null ? getText() : b.text;
        mStart = b.ellipsize != null ? 0 : b.start; mEnd = b.ellipsize != null ? getText().length() : b.end;
        mIncludePad = b.includePad; mMaxLines = b.maxLines; mEllipsize = b.ellipsize; mEllipsizedWidth = b.ellipsize != null ? b.ellipsizedWidth : b.width;
        generate();
    }
    public StaticLayout(CharSequence source, TextPaint paint, int width, Layout.Alignment align, float mult, float add, boolean includePad) {
        this(source, 0, source.length(), paint, width, align, mult, add, includePad);
    }
    public StaticLayout(CharSequence source, int start, int end, TextPaint paint, int width, Layout.Alignment align, float mult, float add, boolean includePad) {
        this(Builder.obtain(source, start, end, paint, width).setAlignment(align).setLineSpacing(add, mult).setIncludePad(includePad));
    }
    public StaticLayout(CharSequence source, int start, int end, TextPaint paint, int width, Layout.Alignment align, float mult, float add, boolean includePad, TextUtils.TruncateAt ellipsize, int ellipsizedWidth) {
        this(Builder.obtain(source, start, end, paint, width).setAlignment(align).setLineSpacing(add, mult).setIncludePad(includePad).setEllipsize(ellipsize).setEllipsizedWidth(ellipsizedWidth).setMaxLines(ellipsize != null ? 1 : Integer.MAX_VALUE));
    }
    public StaticLayout(CharSequence source, int start, int end, TextPaint paint, int width, Layout.Alignment align, TextDirectionHeuristic d, float mult, float add, boolean includePad, TextUtils.TruncateAt ellipsize, int ellipsizedWidth, int maxLines) {
        this(Builder.obtain(source, start, end, paint, width).setAlignment(align).setLineSpacing(add, mult).setIncludePad(includePad).setEllipsize(ellipsize).setEllipsizedWidth(ellipsizedWidth).setMaxLines(maxLines));
    }

    private void addLine(int start, int top, int descent) {
        if (mLineCount + 2 > mStarts.length) {
            int n = mStarts.length * 2;
            mStarts = java.util.Arrays.copyOf(mStarts, n); mTops = java.util.Arrays.copyOf(mTops, n + 1); mDescents = java.util.Arrays.copyOf(mDescents, n);
            mEllStart = java.util.Arrays.copyOf(mEllStart, n); mEllCount = java.util.Arrays.copyOf(mEllCount, n);
        }
        mStarts[mLineCount] = start; mTops[mLineCount] = top; mDescents[mLineCount] = descent; mEllStart[mLineCount] = 0; mEllCount[mLineCount] = 0;
        mLineCount++;
    }
    void generate() {
        CharSequence text = mSource;
        TextPaint paint = getPaint();
        int width = getWidth();
        mLineCount = 0;
        Paint.FontMetricsInt fm = new Paint.FontMetricsInt();
        int y = 0;
        boolean spanned = text instanceof Spanned;
        float mult = getSpacingMultiplier(), add = getSpacingAdd();
        int paraStart = mStart;
        boolean done = false;
        while (paraStart <= mEnd && !done) {
            int paraEnd = TextUtils.indexOf(text, '\n', paraStart, mEnd);
            paraEnd = paraEnd < 0 ? mEnd : paraEnd + 1;
            int lineStart = paraStart;
            int contentEnd = paraEnd > paraStart && paraEnd <= mEnd && paraEnd > mStart && text.charAt(paraEnd - 1) == '\n' ? paraEnd - 1 : paraEnd;
            int firstWidth = width, restWidth = width;
            if (spanned) {
                LeadingMarginSpan[] lm = ((Spanned) text).getSpans(paraStart, paraEnd, LeadingMarginSpan.class);
                for (LeadingMarginSpan m : lm) { firstWidth -= m.getLeadingMargin(true); restWidth -= m.getLeadingMargin(false); }
            }
            do {
                int avail = Math.max(1, lineStart == paraStart ? firstWidth : restWidth);
                int brk = breakLine(text, lineStart, contentEnd, avail, paint);
                int next = brk;
                // a line of spaces at a break belongs to the line before
                while (next < contentEnd && text.charAt(next) == ' ') next++;
                if (next >= contentEnd) next = paraEnd;
                boolean lastOfPara = next >= paraEnd;
                metrics(text, lineStart, Math.max(lineStart + 1, Math.min(next, contentEnd)), paint, fm);
                int above = mIncludePad ? fm.top : fm.ascent, below = mIncludePad ? fm.bottom : fm.descent;
                int h = below - above;
                int extra = 0;
                if (mult != 1 || add != 0) extra = (int) (h * (mult - 1) + add + 0.5f);
                if (spanned) {
                    LineHeightSpan[] lh = ((Spanned) text).getSpans(lineStart, next, LineHeightSpan.class);
                    if (lh.length > 0) {
                        Paint.FontMetricsInt f2 = new Paint.FontMetricsInt(); f2.top = fm.top; f2.ascent = fm.ascent; f2.descent = fm.descent; f2.bottom = fm.bottom;
                        for (LineHeightSpan s : lh) s.chooseHeight(text, lineStart, next, ((Spanned) text).getSpanStart(s), y, f2);
                        above = mIncludePad ? f2.top : f2.ascent; below = mIncludePad ? f2.bottom : f2.descent; h = below - above;
                    }
                }
                boolean lastLineAllowed = mLineCount + 1 >= mMaxLines;
                addLine(lineStart, y, below + Math.max(0, extra));
                y += h + extra;
                if (lastLineAllowed && (next < mEnd || (!lastOfPara))) {
                    // more text than lines: the last one ellipsized (or cut)
                    if (mEllipsize != null && mEllipsize != TextUtils.TruncateAt.MARQUEE) ellipsizeLine(mLineCount - 1, lineStart, contentEnd == paraEnd && paraEnd < mEnd ? mEnd : Math.max(contentEnd, mEnd), paint);
                    done = true;
                    break;
                }
                if (mEllipsize != null && mEllipsize != TextUtils.TruncateAt.MARQUEE && mMaxLines == 1 && husk.TextRuns.measure(text, lineStart, contentEnd, paint) > mEllipsizedWidth) {
                    ellipsizeLine(mLineCount - 1, lineStart, contentEnd, paint);
                }
                lineStart = next;
            } while (lineStart < paraEnd && lineStart < contentEnd);
            if (paraEnd >= mEnd) {
                // a trailing newline gives an empty last line
                if (!done && paraEnd > mStart && paraEnd == mEnd && text.charAt(paraEnd - 1) == '\n' && mLineCount < mMaxLines) {
                    metrics(text, paraEnd, paraEnd, paint, fm);
                    int above = mIncludePad ? fm.top : fm.ascent, below = mIncludePad ? fm.bottom : fm.descent;
                    addLine(paraEnd, y, below);
                    y += below - above;
                }
                break;
            }
            paraStart = paraEnd;
        }
        if (mLineCount == 0) {
            metrics(text, mStart, mStart, paint, fm);
            int above = mIncludePad ? fm.top : fm.ascent, below = mIncludePad ? fm.bottom : fm.descent;
            addLine(mStart, 0, below);
            y = below - above;
        }
        mStarts[mLineCount] = done ? Math.max(mStarts[mLineCount - 1], Math.min(mEnd, endOfLastLine())) : mEnd;
        mTops[mLineCount] = y;
        if (mIncludePad) { mTopPad = fm.top - fm.ascent; mBottomPad = fm.bottom - fm.descent; }
    }
    private int endOfLastLine() { return mEnd; }
    private void ellipsizeLine(int line, int start, int end, TextPaint paint) {
        float avail = mEllipsizedWidth;
        float ell = paint.measureText("…");
        CharSequence t = mSource;
        int n = start;
        float w = 0;
        while (n < end) {
            float cw = husk.TextRuns.measure(t, start, n + 1, paint);
            if (cw + ell > avail) break;
            n++;
        }
        mEllStart[line] = n - start;
        mEllCount[line] = end - n;
    }
    /** Where the line from start breaks within avail pixels: the last word that fits, or as many characters as fit. */
    private static int breakLine(CharSequence text, int start, int end, int avail, TextPaint paint) {
        if (start >= end) return end;
        boolean styled = text instanceof Spanned && ((Spanned) text).nextSpanTransition(start, end, MetricAffectingSpan.class) < end;
        if (!styled && !(text instanceof Spanned && ((Spanned) text).getSpans(start, end, MetricAffectingSpan.class).length > 0) && !(text instanceof Spanned && ((Spanned) text).getSpans(start, end, ReplacementSpan.class).length > 0)) {
            String s = text.subSequence(start, end).toString();
            int tab = s.indexOf('\t');
            if (tab < 0) {
                int n = husk.Gfx.txBreak(s, 0, s.length(), avail + 0.5f, paint.mP, paint.huskFace(), true);
                if (n <= 0) n = Math.max(1, husk.Gfx.txBreak(s, 0, s.length(), avail + 0.5f, paint.mP, paint.huskFace(), false));
                return start + Math.min(n, s.length());
            }
        }
        // styled text: whole words while they fit
        int lastSpace = -1, i = start;
        while (i < end) {
            char c = text.charAt(i);
            float w = husk.TextRuns.measure(text, start, i + 1, paint);
            if (w > avail && c != ' ') {
                if (lastSpace >= start) return lastSpace + 1;
                return Math.max(start + 1, i);
            }
            if (c == ' ' || c == '\t' || c == '-') lastSpace = i;
            i++;
        }
        return end;
    }
    private static void metrics(CharSequence text, int start, int end, TextPaint paint, Paint.FontMetricsInt out) {
        paint.getFontMetricsInt(out);
        if (!(text instanceof Spanned) || start >= end) return;
        Spanned sp = (Spanned) text;
        MetricAffectingSpan[] spans = sp.getSpans(start, end, MetricAffectingSpan.class);
        if (spans.length == 0) return;
        TextPaint wp = new TextPaint();
        Paint.FontMetricsInt f = new Paint.FontMetricsInt();
        for (int i = start; i < end; ) {
            int next = sp.nextSpanTransition(i, end, MetricAffectingSpan.class);
            wp.set(paint);
            for (MetricAffectingSpan m : sp.getSpans(i, next, MetricAffectingSpan.class)) { if (m instanceof ReplacementSpan) { ((ReplacementSpan) m).getSize(wp, text, i, next, f); continue; } m.updateMeasureState(wp); }
            wp.getFontMetricsInt(f);
            out.top = Math.min(out.top, f.top + wp.baselineShift); out.ascent = Math.min(out.ascent, f.ascent + wp.baselineShift);
            out.descent = Math.max(out.descent, f.descent + wp.baselineShift); out.bottom = Math.max(out.bottom, f.bottom + wp.baselineShift);
            i = next;
        }
    }
    public int getLineForVertical(int v) { return super.getLineForVertical(v); }
    public int getLineCount() { return mLineCount; }
    public int getLineTop(int line) { return mTops[Math.max(0, Math.min(line, mLineCount))]; }
    public int getLineDescent(int line) { return mDescents[Math.max(0, Math.min(line, mLineCount - 1))]; }
    public int getLineStart(int line) { return line >= mLineCount ? mStarts[mLineCount] : mStarts[Math.max(0, line)]; }
    public int getParagraphDirection(int line) { return DIR_LEFT_TO_RIGHT; }
    public boolean getLineContainsTab(int line) { return false; }
    public Directions getLineDirections(int line) { return DIRS_ALL_LEFT_TO_RIGHT; }
    public int getTopPadding() { return mTopPad; }
    public int getBottomPadding() { return mBottomPad; }
    public int getEllipsisCount(int line) { return line < mLineCount ? mEllCount[line] : 0; }
    public int getEllipsisStart(int line) { return line < mLineCount ? mEllStart[line] : 0; }
    public int getEllipsizedWidth() { return mEllipsizedWidth; }
    public boolean isFallbackLineSpacingEnabled() { return false; }
}
