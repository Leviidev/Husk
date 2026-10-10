package android.text;

import java.lang.reflect.Array;
import java.util.ArrayList;

/** Text with spans: the base of SpannableStringBuilder, SpannableString and SpannedString. */
abstract class SpannableStringInternal implements Spanned, GetChars {
    char[] mText = new char[16];
    int mLen;
    Object[] mSpans = new Object[4];
    int[] mStarts = new int[4], mEnds = new int[4], mFlags = new int[4];
    int mSpanCount;
    SpannableStringInternal(CharSequence s, int start, int end) {
        if (s == null) s = "";
        int n = end - start;
        mText = new char[Math.max(16, n + 8)];
        for (int i = 0; i < n; i++) mText[i] = s.charAt(start + i);
        mLen = n;
        if (s instanceof Spanned) copySpans((Spanned) s, start, end);
    }
    void copySpans(Spanned sp, int start, int end) {
        Object[] spans = sp.getSpans(start, end, Object.class);
        for (Object o : spans) {
            if (o instanceof NoCopySpan) continue;
            int st = sp.getSpanStart(o), en = sp.getSpanEnd(o), fl = sp.getSpanFlags(o);
            if (st < start) st = start; if (en > end) en = end;
            addSpan(o, st - start, en - start, fl);
        }
    }
    public final int length() { return mLen; }
    public final char charAt(int i) { if (i < 0 || i >= mLen) throw new IndexOutOfBoundsException("charAt: " + i + " of " + mLen); return mText[i]; }
    @Override public final String toString() { return new String(mText, 0, mLen); }
    public final void getChars(int start, int end, char[] dest, int off) { System.arraycopy(mText, start, dest, off, end - start); }
    public CharSequence subSequence(int start, int end) { return new SpannableString(this, start, end); }
    int indexOfSpan(Object what) { for (int i = 0; i < mSpanCount; i++) if (mSpans[i] == what) return i; return -1; }
    void addSpan(Object what, int start, int end, int flags) {
        int i = indexOfSpan(what);
        if (i >= 0) { mStarts[i] = start; mEnds[i] = end; mFlags[i] = flags; return; }
        if (mSpanCount == mSpans.length) {
            int n = mSpanCount * 2 + 2;
            mSpans = java.util.Arrays.copyOf(mSpans, n); mStarts = java.util.Arrays.copyOf(mStarts, n); mEnds = java.util.Arrays.copyOf(mEnds, n); mFlags = java.util.Arrays.copyOf(mFlags, n);
        }
        mSpans[mSpanCount] = what; mStarts[mSpanCount] = start; mEnds[mSpanCount] = end; mFlags[mSpanCount] = flags; mSpanCount++;
    }
    void removeSpanAt(int i) {
        System.arraycopy(mSpans, i + 1, mSpans, i, mSpanCount - i - 1); System.arraycopy(mStarts, i + 1, mStarts, i, mSpanCount - i - 1);
        System.arraycopy(mEnds, i + 1, mEnds, i, mSpanCount - i - 1); System.arraycopy(mFlags, i + 1, mFlags, i, mSpanCount - i - 1);
        mSpanCount--; mSpans[mSpanCount] = null;
    }
    public int getSpanStart(Object what) { int i = indexOfSpan(what); return i < 0 ? -1 : mStarts[i]; }
    public int getSpanEnd(Object what) { int i = indexOfSpan(what); return i < 0 ? -1 : mEnds[i]; }
    public int getSpanFlags(Object what) { int i = indexOfSpan(what); return i < 0 ? 0 : mFlags[i]; }
    @SuppressWarnings("unchecked")
    public <T> T[] getSpans(int qs, int qe, Class<T> kind) {
        if (kind == null) kind = (Class<T>) Object.class;
        ArrayList<Object> out = new ArrayList<>();
        ArrayList<Integer> prio = new ArrayList<>();
        for (int i = 0; i < mSpanCount; i++) {
            int s = mStarts[i], e = mEnds[i];
            if (s > qe || e < qs) continue;
            if (s != e && qs != qe) { if (s == qe || e == qs) continue; }
            if (!kind.isInstance(mSpans[i])) continue;
            int p = mFlags[i] & SPAN_PRIORITY;
            int k = out.size();
            if (p != 0) { k = 0; while (k < out.size() && prio.get(k) >= p) k++; }
            out.add(k, mSpans[i]); prio.add(k, p);
        }
        T[] r = (T[]) Array.newInstance(kind, out.size());
        return out.toArray(r);
    }
    public int nextSpanTransition(int start, int limit, Class kind) {
        if (kind == null) kind = Object.class;
        for (int i = 0; i < mSpanCount; i++) {
            if (!kind.isInstance(mSpans[i])) continue;
            int s = mStarts[i], e = mEnds[i];
            if (s > start && s < limit) limit = s;
            if (e > start && e < limit) limit = e;
        }
        return limit;
    }
    @Override public boolean equals(Object o) { return o instanceof Spanned && toString().equals(o.toString()); }
    @Override public int hashCode() { return toString().hashCode(); }
}
