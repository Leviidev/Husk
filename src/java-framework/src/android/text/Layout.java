package android.text;

import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Rect;
import android.text.style.*;

/** Laid-out text: lines with their offsets and vertical positions, drawn styled. StaticLayout computes them. */
public abstract class Layout {
    public enum Alignment { ALIGN_NORMAL, ALIGN_OPPOSITE, ALIGN_CENTER, ALIGN_LEFT, ALIGN_RIGHT }
    public static final int DIR_LEFT_TO_RIGHT = 1, DIR_RIGHT_TO_LEFT = -1, BREAK_STRATEGY_SIMPLE = 0, BREAK_STRATEGY_HIGH_QUALITY = 1, BREAK_STRATEGY_BALANCED = 2,
        HYPHENATION_FREQUENCY_NONE = 0, HYPHENATION_FREQUENCY_NORMAL = 1, HYPHENATION_FREQUENCY_FULL = 2, HYPHENATION_FREQUENCY_NORMAL_FAST = 3, HYPHENATION_FREQUENCY_FULL_FAST = 4,
        JUSTIFICATION_MODE_NONE = 0, JUSTIFICATION_MODE_INTER_WORD = 1, JUSTIFICATION_MODE_INTER_CHARACTER = 2, LINE_BREAK_STYLE_NONE = 0, LINE_BREAK_WORD_STYLE_NONE = 0,
        TEXT_SELECTION_GRANULARITY_GRAPHEME = 0;
    public static class Directions { int[] mDirections; Directions(int[] d) { mDirections = d; } }
    static final Directions DIRS_ALL_LEFT_TO_RIGHT = new Directions(new int[] { 0, 0x3ffffff });
    private CharSequence mText;
    private TextPaint mPaint;
    private int mWidth;
    private Alignment mAlignment = Alignment.ALIGN_NORMAL;
    private float mSpacingMult, mSpacingAdd;
    private final boolean mSpannedText;

    protected Layout(CharSequence text, TextPaint paint, int width, Alignment align, float spacingMult, float spacingAdd) {
        if (width < 0) throw new IllegalArgumentException("Layout: " + width + " < 0");
        mText = text; mPaint = paint; mWidth = width; mAlignment = align == null ? Alignment.ALIGN_NORMAL : align; mSpacingMult = spacingMult; mSpacingAdd = spacingAdd;
        mSpannedText = text instanceof Spanned;
    }
    void replaceWith(CharSequence text, TextPaint paint, int width, Alignment align, float m, float a) { mText = text; mPaint = paint; mWidth = width; mAlignment = align; mSpacingMult = m; mSpacingAdd = a; }
    public static float getDesiredWidth(CharSequence source, TextPaint paint) { return getDesiredWidth(source, 0, source.length(), paint); }
    public static float getDesiredWidth(CharSequence source, int start, int end, TextPaint paint) {
        float need = 0;
        for (int i = start; i <= end; ) {
            int next = TextUtils.indexOf(source, '\n', i, end);
            if (next < 0) next = end;
            need = Math.max(need, husk.TextRuns.measure(source, i, next, paint));
            i = next + 1;
        }
        return need;
    }
    public static float getDesiredWidth(CharSequence s, int start, int end, TextPaint p, TextDirectionHeuristic d) { return getDesiredWidth(s, start, end, p); }
    public final CharSequence getText() { return mText; }
    public final TextPaint getPaint() { return mPaint; }
    public final int getWidth() { return mWidth; }
    public int getEllipsizedWidth() { return mWidth; }
    public final void increaseWidthTo(int w) { if (w < mWidth) throw new RuntimeException("attempted to reduce Layout width"); mWidth = w; }
    public int getHeight() { return getLineTop(getLineCount()); }
    public int getHeight(boolean cap) { return getHeight(); }
    public final Alignment getAlignment() { return mAlignment; }
    public final float getSpacingMultiplier() { return mSpacingMult; }
    public final float getSpacingAdd() { return mSpacingAdd; }
    public final float getLineSpacingMultiplier() { return mSpacingMult; }
    public final float getLineSpacingAmount() { return mSpacingAdd; }
    public final TextDirectionHeuristic getTextDirectionHeuristic() { return TextDirectionHeuristics.FIRSTSTRONG_LTR; }
    public abstract int getLineCount();
    public int getLineBounds(int line, Rect bounds) { if (bounds != null) bounds.set(0, getLineTop(line), mWidth, getLineTop(line + 1)); return getLineBaseline(line); }
    public abstract int getLineTop(int line);
    public abstract int getLineDescent(int line);
    public abstract int getLineStart(int line);
    public abstract int getParagraphDirection(int line);
    public abstract boolean getLineContainsTab(int line);
    public abstract Directions getLineDirections(int line);
    public abstract int getTopPadding();
    public abstract int getBottomPadding();
    public abstract int getEllipsisStart(int line);
    public abstract int getEllipsisCount(int line);
    public int getHyphen(int line) { return 0; }
    public boolean isLevelBoundary(int off) { return false; }
    public boolean isRtlCharAt(int off) { return false; }
    public long getRunRange(int off) { return TextUtils.packRangeInLong(0, mText.length()); }
    public final int getLineEnd(int line) { return getLineStart(line + 1); }
    public int getLineVisibleEnd(int line) {
        int s = getLineStart(line), e = getLineStart(line + 1);
        if (line == getLineCount() - 1) return e;
        while (e > s) { char c = mText.charAt(e - 1); if (c == '\n') { e--; continue; } if (c != ' ' && c != '\t') break; e--; }
        return e;
    }
    public final int getLineBottom(int line) { return getLineTop(line + 1); }
    public final int getLineBottom(int line, boolean includeSpacing) { return getLineTop(line + 1); }
    public final int getLineBaseline(int line) { return getLineTop(line + 1) - getLineDescent(line); }
    public final int getLineAscent(int line) { return getLineTop(line) - (getLineTop(line + 1) - getLineDescent(line)); }
    public int getLineForVertical(int v) {
        int high = getLineCount(), low = -1, guess;
        while (high - low > 1) { guess = (high + low) / 2; if (getLineTop(guess) > v) high = guess; else low = guess; }
        return low < 0 ? 0 : low;
    }
    public int getLineForOffset(int off) {
        int high = getLineCount(), low = -1, guess;
        while (high - low > 1) { guess = (high + low) / 2; if (getLineStart(guess) > off) high = guess; else low = guess; }
        return low < 0 ? 0 : low;
    }
    public float getLineWidth(int line) { return husk.TextRuns.measure(mText, getLineStart(line), getLineVisibleEnd(line), mPaint); }
    public float getLineMax(int line) { return getLineWidth(line); }
    public float getLineLeft(int line) { return lineX(line, getLineWidth(line)); }
    public float getLineRight(int line) { return getLineLeft(line) + getLineWidth(line); }
    public int getParagraphLeft(int line) { return 0; }
    public int getParagraphRight(int line) { return mWidth; }
    public final Alignment getParagraphAlignment(int line) {
        if (mSpannedText) { AlignmentSpan[] a = ((Spanned) mText).getSpans(getLineStart(line), getLineEnd(line), AlignmentSpan.class); if (a.length > 0) return a[a.length - 1].getAlignment(); }
        return mAlignment;
    }
    float lineX(int line, float w) {
        Alignment a = getParagraphAlignment(line);
        int room = getEllipsizedWidth();
        if (a == Alignment.ALIGN_CENTER) return (room - w) / 2f;
        if (a == Alignment.ALIGN_OPPOSITE || a == Alignment.ALIGN_RIGHT) return room - w;
        return 0;
    }
    public float getPrimaryHorizontal(int off) { return getHorizontal(off); }
    public float getPrimaryHorizontal(int off, boolean clamped) { return getHorizontal(off); }
    public float getSecondaryHorizontal(int off) { return getHorizontal(off); }
    private float getHorizontal(int off) {
        int line = getLineForOffset(off), s = getLineStart(line);
        return getLineLeft(line) + husk.TextRuns.measure(mText, s, Math.min(off, getLineVisibleEnd(line)), mPaint);
    }
    public int getOffsetForHorizontal(int line, float h) {
        int s = getLineStart(line), e = getLineVisibleEnd(line);
        float x = h - getLineLeft(line);
        if (x <= 0) return s;
        int best = e; float prev = 0;
        for (int i = s + 1; i <= e; i++) {
            float w = husk.TextRuns.measure(mText, s, i, mPaint);
            if (w >= x) { best = (x - prev) < (w - x) ? i - 1 : i; break; }
            prev = w;
        }
        return best;
    }
    public int getOffsetForHorizontal(int line, float h, boolean primary) { return getOffsetForHorizontal(line, h); }
    public int getOffsetToLeftOf(int off) { return Math.max(0, off - 1); }
    public int getOffsetToRightOf(int off) { return Math.min(mText.length(), off + 1); }
    public void getCursorPath(int point, Path dest, CharSequence editingBuffer) {
        dest.reset();
        int line = getLineForOffset(point);
        float h = getPrimaryHorizontal(point) - 0.5f;
        dest.moveTo(h, getLineTop(line)); dest.lineTo(h, getLineBottom(line));
    }
    public void getSelectionPath(int start, int end, Path dest) {
        dest.reset();
        if (start == end) return;
        if (end < start) { int t = end; end = start; start = t; }
        int sl = getLineForOffset(start), el = getLineForOffset(end);
        for (int l = sl; l <= el; l++) {
            float x0 = l == sl ? getPrimaryHorizontal(start) : getLineLeft(l), x1 = l == el ? getPrimaryHorizontal(end) : getLineRight(l);
            dest.addRect(x0, getLineTop(l), Math.max(x1, x0 + 1), getLineBottom(l), Path.Direction.CW);
        }
    }
    public void getSelection(int start, int end, SelectionRectangleConsumer consumer) {}
    public void draw(Canvas c) { draw(c, null, null, 0); }
    public void draw(Canvas c, Path highlight, Paint highlightPaint, int cursorOffsetVertical) {
        drawBackground(c, highlight, highlightPaint, cursorOffsetVertical, 0, getLineCount());
        drawText(c, 0, getLineCount());
    }
    public void draw(Canvas c, java.util.List<Path> highlights, java.util.List<Paint> paints, Path sel, Paint selPaint, int off) { draw(c, sel, selPaint, off); }
    public void drawBackground(Canvas c, Path highlight, Paint hp, int cursorOffsetVertical, int first, int last) {
        if (highlight != null && hp != null) { if (cursorOffsetVertical != 0) c.translate(0, cursorOffsetVertical); c.drawPath(highlight, hp); if (cursorOffsetVertical != 0) c.translate(0, -cursorOffsetVertical); }
    }
    public void drawText(Canvas c, int first, int last) {
        android.graphics.Rect clip = new android.graphics.Rect();
        boolean clipped = c.getClipBounds(clip);
        for (int line = first; line < last; line++) {
            int top = getLineTop(line), bottom = getLineTop(line + 1);
            if (clipped && (bottom < clip.top || top > clip.bottom)) continue;
            int s = getLineStart(line), e = getLineVisibleEnd(line);
            int es = getEllipsisStart(line), ec = getEllipsisCount(line);
            CharSequence text = mText;
            if (ec > 0) {
                SpannableStringBuilder b = new SpannableStringBuilder(mText, s, e);
                int rs = es, re = Math.min(es + ec, e - s);
                if (rs >= 0 && rs <= b.length()) { b.replace(rs, Math.min(re, b.length()), "…"); }
                text = b; e = b.length(); s = 0;
            }
            float w = husk.TextRuns.measure(text, s, e, mPaint);
            float x = lineX(line, w);
            if (mSpannedText) {
                LeadingMarginSpan[] lm = ((Spanned) mText).getSpans(getLineStart(line), getLineEnd(line), LeadingMarginSpan.class);
                for (LeadingMarginSpan m : lm) { m.drawLeadingMargin(c, mPaint, (int) x, 1, top, getLineBaseline(line), bottom, mText, getLineStart(line), getLineEnd(line), getLineStart(line) == ((Spanned) mText).getSpanStart(m), this); x += m.getLeadingMargin(getLineStart(line) == ((Spanned) mText).getSpanStart(m)); }
            }
            husk.TextRuns.draw(c, text, s, e, x, getLineBaseline(line), top, bottom, mPaint);
        }
    }
    public boolean isSpanned() { return mSpannedText; }
    public final int getLineLeftHusk(int line) { return (int) getLineLeft(line); }
    // ---- platform API stubs (tools/compat/genstubs.py)
    public interface SelectionRectangleConsumer {
        void accept(float p0, float p1, float p2, float p3, int p4);
    }
}
