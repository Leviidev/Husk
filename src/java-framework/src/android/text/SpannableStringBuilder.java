package android.text;

import java.util.ArrayList;

/** Editable text with spans that follow the edits (by their point/mark flags), filters applied, and watchers told. */
public class SpannableStringBuilder extends SpannableStringInternal implements CharSequence, GetChars, Spannable, Editable, Appendable {
    private InputFilter[] mFilters = new InputFilter[0];
    public SpannableStringBuilder() { super("", 0, 0); }
    public SpannableStringBuilder(CharSequence s) { super(s, 0, s.length()); }
    public SpannableStringBuilder(CharSequence s, int start, int end) { super(s, start, end); }
    public static SpannableStringBuilder valueOf(CharSequence s) { return s instanceof SpannableStringBuilder ? (SpannableStringBuilder) s : new SpannableStringBuilder(s); }
    public void setFilters(InputFilter[] f) { mFilters = f == null ? new InputFilter[0] : f; }
    public InputFilter[] getFilters() { return mFilters; }
    public CharSequence subSequence(int start, int end) { return new SpannableStringBuilder(this, start, end); }
    public SpannableStringBuilder insert(int where, CharSequence tb, int start, int end) { return replace(where, where, tb, start, end); }
    public SpannableStringBuilder insert(int where, CharSequence tb) { return replace(where, where, tb, 0, tb.length()); }
    public SpannableStringBuilder delete(int start, int end) { return replace(start, end, "", 0, 0); }
    public SpannableStringBuilder append(CharSequence text) { int l = mLen; return replace(l, l, text, 0, text.length()); }
    public SpannableStringBuilder append(CharSequence text, Object what, int flags) { int s = mLen; append(text); setSpan(what, s, mLen, flags); return this; }
    public SpannableStringBuilder append(CharSequence text, int start, int end) { int l = mLen; return replace(l, l, text, start, end); }
    public SpannableStringBuilder append(char c) { return append(String.valueOf(c)); }
    public void clear() { replace(0, mLen, "", 0, 0); }
    public void clearSpans() { for (int i = mSpanCount - 1; i >= 0; i--) { Object w = mSpans[i]; int s = mStarts[i], e = mEnds[i]; removeSpanAt(i); sendSpanRemoved(w, s, e); } }
    public SpannableStringBuilder replace(int start, int end, CharSequence tb) { return replace(start, end, tb, 0, tb.length()); }
    public SpannableStringBuilder replace(int start, int end, CharSequence tb, int tbstart, int tbend) {
        for (InputFilter f : mFilters) {
            CharSequence repl = f.filter(tb, tbstart, tbend, this, start, end);
            if (repl != null) { tb = repl; tbstart = 0; tbend = repl.length(); }
        }
        int origLen = end - start, newLen = tbend - tbstart;
        if (origLen == 0 && newLen == 0) return this;
        TextWatcher[] watchers = getSpans(start, start + origLen, TextWatcher.class);
        for (TextWatcher w : watchers) w.beforeTextChanged(this, start, origLen, newLen);
        // the text
        int delta = newLen - origLen;
        if (mLen + delta > mText.length) mText = java.util.Arrays.copyOf(mText, Math.max(mText.length * 2, mLen + delta + 16));
        System.arraycopy(mText, end, mText, end + delta, mLen - end);
        for (int i = 0; i < newLen; i++) mText[start + i] = tb.charAt(tbstart + i);
        mLen += delta;
        // the spans: those after move, those inside shrink, points and marks at the edges follow their flags
        for (int i = 0; i < mSpanCount; i++) {
            mStarts[i] = adjust(mStarts[i], start, end, delta, (mFlags[i] & 0xf0) >> 4, true, origLen);
            mEnds[i] = adjust(mEnds[i], start, end, delta, mFlags[i] & 0x0f, false, origLen);
            if (mEnds[i] < mStarts[i]) mEnds[i] = mStarts[i];
        }
        // spans of a deleted range that are now empty and exclusive go away
        for (int i = mSpanCount - 1; i >= 0; i--) {
            if (mStarts[i] == mEnds[i] && origLen > 0 && (mFlags[i] & SPAN_POINT_MARK_MASK) == SPAN_EXCLUSIVE_EXCLUSIVE && mStarts[i] == start && !(mSpans[i] instanceof NoCopySpan)) {
                Object w = mSpans[i]; int s = mStarts[i], e2 = mEnds[i]; removeSpanAt(i); sendSpanRemoved(w, s, e2);
            }
        }
        if (tb instanceof Spanned) {
            Spanned sp = (Spanned) tb;
            Object[] spans = sp.getSpans(tbstart, tbend, Object.class);
            for (Object o : spans) {
                if (o instanceof NoCopySpan) continue;
                int st = Math.max(sp.getSpanStart(o), tbstart) - tbstart + start, en = Math.min(sp.getSpanEnd(o), tbend) - tbstart + start;
                if (indexOfSpan(o) < 0) setSpan(o, st, en, sp.getSpanFlags(o));
            }
        }
        watchers = getSpans(start, start + newLen, TextWatcher.class);
        for (TextWatcher w : watchers) w.onTextChanged(this, start, origLen, newLen);
        for (TextWatcher w : watchers) w.afterTextChanged(this);
        return this;
    }
    /** A span edge after replacing [start, end) with delta more characters: 1 mark (stays left), 2 point (moves right). */
    private static int adjust(int pos, int start, int end, int delta, int kind, boolean isStart, int origLen) {
        if (pos > end) return pos + delta;
        if (pos < start) return pos;
        if (pos == start && origLen == 0) {
            // an insertion at the edge: a point moves with the text, a mark stays
            return kind == 2 ? pos + delta : pos;
        }
        if (pos == end && pos != start) return pos + delta;
        // inside the replaced range
        return kind == 2 ? start + Math.max(0, (end - start) + delta) : start;
    }
    public void setSpan(Object what, int start, int end, int flags) {
        if (start < 0 || end > mLen || start > end) throw new IndexOutOfBoundsException("setSpan (" + start + " ... " + end + ") ends beyond length " + mLen);
        int i = indexOfSpan(what);
        if (i >= 0) {
            int os = mStarts[i], oe = mEnds[i];
            mStarts[i] = start; mEnds[i] = end; mFlags[i] = flags;
            for (SpanWatcher w : getSpans(Math.min(os, start), Math.max(oe, end), SpanWatcher.class)) w.onSpanChanged(this, what, os, oe, start, end);
            return;
        }
        addSpan(what, start, end, flags);
        if (!(what instanceof SpanWatcher)) for (SpanWatcher w : getSpans(start, end, SpanWatcher.class)) w.onSpanAdded(this, what, start, end);
    }
    public void removeSpan(Object what) { int i = indexOfSpan(what); if (i >= 0) { int s = mStarts[i], e = mEnds[i]; removeSpanAt(i); sendSpanRemoved(what, s, e); } }
    public void removeSpan(Object what, int flags) { removeSpan(what); }
    private void sendSpanRemoved(Object what, int s, int e) { if (what instanceof SpanWatcher) return; for (SpanWatcher w : getSpans(s, e, SpanWatcher.class)) w.onSpanRemoved(this, what, s, e); }
    public int getTextWatcherDepth() { return 0; }
    public String substring(int start, int end) { return new String(mText, start, end - start); }
    public void drawText(android.graphics.Canvas c, int start, int end, float x, float y, android.graphics.Paint p) { c.drawText(toString(), start, end, x, y, p); }
    public float getTextRunAdvances(int s, int e, int cs, int ce, boolean rtl, float[] adv, int ai, android.graphics.Paint p) { return p.measureText(toString(), s, e); }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public java.lang.Object[] getSpans(int p0, int p1, java.lang.Class p2, boolean p3) { return null; }
    public int getTextRunCursor(int p0, int p1, int p2, int p3, int p4, android.graphics.Paint p5) { return 0; }
    public int getTextRunCursor(int p0, int p1, boolean p2, int p3, int p4, android.graphics.Paint p5) { return 0; }
    public int getTextWidths(int p0, int p1, float[] p2, android.graphics.Paint p3) { return 0; }
    public float measureText(int p0, int p1, android.graphics.Paint p2) { return 0f; }
    // ---- end of generated members
}
