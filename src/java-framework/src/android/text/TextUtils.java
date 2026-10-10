package android.text;

import android.graphics.Paint;
import java.util.Iterator;
import java.util.Locale;

public class TextUtils {
    public static final int CAP_MODE_CHARACTERS = InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS, CAP_MODE_WORDS = InputType.TYPE_TEXT_FLAG_CAP_WORDS,
        CAP_MODE_SENTENCES = InputType.TYPE_TEXT_FLAG_CAP_SENTENCES, SAFE_STRING_FLAG_TRIM = 1, SAFE_STRING_FLAG_SINGLE_LINE = 2, SAFE_STRING_FLAG_FIRST_LINE = 4;
    public static final Parcelable_ CHAR_SEQUENCE_CREATOR = null;
    public interface Parcelable_ {}
    public enum TruncateAt { START, MIDDLE, END, MARQUEE, END_SMALL }
    public interface EllipsizeCallback { void ellipsized(int start, int end); }
    public interface StringSplitter extends Iterable<String> { void setString(String s); }
    public static class SimpleStringSplitter implements StringSplitter, Iterator<String> {
        private String mString; private final char mDelim; private int mPos, mLen;
        public SimpleStringSplitter(char d) { mDelim = d; }
        public void setString(String s) { mString = s; mPos = 0; mLen = s.length(); }
        public Iterator<String> iterator() { return this; }
        public boolean hasNext() { return mPos < mLen; }
        public String next() { int e = mString.indexOf(mDelim, mPos); if (e == -1) e = mLen; String r = mString.substring(mPos, e); mPos = e + 1; return r; }
        public void remove() { throw new UnsupportedOperationException(); }
    }
    private TextUtils() {}
    public static boolean isEmpty(CharSequence s) { return s == null || s.length() == 0; }
    public static boolean isGraphic(CharSequence s) { for (int i = 0; i < s.length(); i++) if (!Character.isWhitespace(s.charAt(i))) return true; return false; }
    public static boolean isGraphic(char c) { return !Character.isWhitespace(c) && !Character.isISOControl(c); }
    public static boolean isDigitsOnly(CharSequence s) { for (int i = 0; i < s.length(); i++) if (!Character.isDigit(s.charAt(i))) return false; return true; }
    public static boolean equals(CharSequence a, CharSequence b) {
        if (a == b) return true;
        if (a != null && b != null && a.length() == b.length()) { if (a instanceof String && b instanceof String) return a.equals(b); for (int i = 0; i < a.length(); i++) if (a.charAt(i) != b.charAt(i)) return false; return true; }
        return false;
    }
    public static String join(CharSequence d, Iterable t) { Iterator it = t.iterator(); if (!it.hasNext()) return ""; StringBuilder b = new StringBuilder(); b.append(it.next()); while (it.hasNext()) { b.append(d); b.append(it.next()); } return b.toString(); }
    public static String join(CharSequence d, Object[] t) { return join(d, java.util.Arrays.asList(t)); }
    public static String[] split(String s, String re) { return s.isEmpty() ? new String[0] : s.split(re, -1); }
    public static String[] split(String s, java.util.regex.Pattern p) { return s.isEmpty() ? new String[0] : p.split(s, -1); }
    public static CharSequence concat(CharSequence... t) {
        boolean spanned = false;
        for (CharSequence c : t) if (c instanceof Spanned) { spanned = true; break; }
        if (!spanned) { StringBuilder b = new StringBuilder(); for (CharSequence c : t) b.append(c); return b.toString(); }
        SpannableStringBuilder b = new SpannableStringBuilder(); for (CharSequence c : t) if (c != null) b.append(c); return new SpannedString(b);
    }
    public static int getTrimmedLength(CharSequence s) { int len = s.length(), st = 0; while (st < len && s.charAt(st) <= ' ') st++; int e = len; while (e > st && s.charAt(e - 1) <= ' ') e--; return e - st; }
    public static CharSequence trimToSize(CharSequence s, int size) { return s == null || s.length() <= size ? s : s.subSequence(0, size); }
    public static String substring(CharSequence s, int st, int e) { return s.subSequence(st, e).toString(); }
    public static int indexOf(CharSequence s, char c) { return indexOf(s, c, 0); }
    public static int indexOf(CharSequence s, char c, int start) { for (int i = start; i < s.length(); i++) if (s.charAt(i) == c) return i; return -1; }
    public static int indexOf(CharSequence s, char c, int start, int end) { for (int i = start; i < end; i++) if (s.charAt(i) == c) return i; return -1; }
    public static int lastIndexOf(CharSequence s, char c) { return lastIndexOf(s, c, s.length() - 1); }
    public static int lastIndexOf(CharSequence s, char c, int last) { for (int i = last; i >= 0; i--) if (s.charAt(i) == c) return i; return -1; }
    public static int indexOf(CharSequence s, CharSequence n) { return s.toString().indexOf(n.toString()); }
    public static int indexOf(CharSequence s, CharSequence n, int start) { return s.toString().indexOf(n.toString(), start); }
    public static boolean regionMatches(CharSequence a, int ao, CharSequence b, int bo, int len) { for (int i = 0; i < len; i++) if (a.charAt(ao + i) != b.charAt(bo + i)) return false; return true; }
    public static void getChars(CharSequence s, int start, int end, char[] dest, int off) {
        if (s instanceof GetChars) ((GetChars) s).getChars(start, end, dest, off);
        else for (int i = start; i < end; i++) dest[off++] = s.charAt(i);
    }
    public static CharSequence stringOrSpannedString(CharSequence s) { if (s == null) return null; if (s instanceof SpannedString) return s; if (s instanceof Spanned) return new SpannedString(s); return s.toString(); }
    public static String htmlEncode(String s) {
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < s.length(); i++) { char c = s.charAt(i); switch (c) { case '<': b.append("&lt;"); break; case '>': b.append("&gt;"); break; case '&': b.append("&amp;"); break; case '\'': b.append("&#39;"); break; case '"': b.append("&quot;"); break; default: b.append(c); } }
        return b.toString();
    }
    public static CharSequence expandTemplate(CharSequence template, CharSequence... values) { String t = template.toString(); for (int i = 0; i < values.length; i++) t = t.replace("^" + (i + 1), values[i]); return t; }
    public static void copySpansFrom(Spanned src, int start, int end, Class kind, Spannable dest, int off) {
        if (kind == null) kind = Object.class;
        Object[] spans = src.getSpans(start, end, kind);
        for (Object o : spans) { int st = Math.max(src.getSpanStart(o), start), en = Math.min(src.getSpanEnd(o), end); dest.setSpan(o, st - start + off, en - start + off, src.getSpanFlags(o)); }
    }
    public static int getCapsMode(CharSequence cs, int off, int reqModes) {
        if (off < 0) return 0;
        int mode = 0;
        if ((reqModes & CAP_MODE_CHARACTERS) != 0) mode |= CAP_MODE_CHARACTERS;
        if ((reqModes & (CAP_MODE_WORDS | CAP_MODE_SENTENCES)) == 0) return mode;
        int i = off;
        while (i > 0 && (cs.charAt(i - 1) == '"' || cs.charAt(i - 1) == '\'' || cs.charAt(i - 1) == '(' || cs.charAt(i - 1) == '[')) i--;
        int j = i;
        while (j > 0 && cs.charAt(j - 1) == ' ') j--;
        if (j == 0 || cs.charAt(j - 1) == '\n') return mode | (reqModes & (CAP_MODE_WORDS | CAP_MODE_SENTENCES));
        if ((reqModes & CAP_MODE_SENTENCES) == 0) { if (i != j) mode |= CAP_MODE_WORDS; return mode; }
        if (i == j) return mode;
        char c = cs.charAt(j - 1);
        if (c == '.' || c == '?' || c == '!') return mode | CAP_MODE_SENTENCES;
        return mode;
    }
    public static CharSequence ellipsize(CharSequence text, android.text.TextPaint p, float avail, TruncateAt where) { return ellipsize(text, p, avail, where, false, null); }
    public static CharSequence ellipsize(CharSequence text, android.text.TextPaint p, float avail, TruncateAt where, boolean preserveLength, EllipsizeCallback cb) {
        String s = text.toString();
        float w = p.measureText(s);
        if (w <= avail) { if (cb != null) cb.ellipsized(0, 0); return text; }
        String ell = "…";
        float ew = p.measureText(ell);
        float room = Math.max(0, avail - ew);
        int n = s.length();
        if (where == TruncateAt.START) {
            int k = n;
            while (k > 0 && p.measureText(s, k - 1, n) <= room) k--;
            if (cb != null) cb.ellipsized(0, k);
            return ell + s.substring(k);
        } else if (where == TruncateAt.MIDDLE) {
            int left = 0, right = n;
            while (left < right) {
                float lw = p.measureText(s, 0, left + 1), rw = p.measureText(s, right - 1, n);
                if (lw + p.measureText(s, right, n) <= room / 2 + 0.5f && left + 1 <= right) left++;
                else if (p.measureText(s, 0, left) + rw <= room) right--;
                else break;
                if (p.measureText(s, 0, left) + p.measureText(s, right, n) > room) break;
            }
            if (cb != null) cb.ellipsized(left, right);
            return s.substring(0, left) + ell + s.substring(right);
        }
        int k = p.breakText(s, 0, n, true, room, null);
        if (cb != null) cb.ellipsized(k, n);
        return s.substring(0, k) + ell;
    }
    public static CharSequence commaEllipsize(CharSequence text, android.text.TextPaint p, float avail, String oneMore, String more) { return ellipsize(text, p, avail, TruncateAt.END); }
    public static boolean isPrintableAscii(char c) { return (c >= ' ' && c <= '~') || c == '\r' || c == '\n'; }
    public static boolean isPrintableAsciiOnly(CharSequence s) { for (int i = 0; i < s.length(); i++) if (!isPrintableAscii(s.charAt(i))) return false; return true; }
    public static int getLayoutDirectionFromLocale(Locale l) { return 0; }
    public static CharSequence toUpperCase(Locale l, CharSequence s, boolean copy) { return s.toString().toUpperCase(l); }
    public static CharSequence replace(CharSequence template, String[] sources, CharSequence[] dest) { String t = template.toString(); for (int i = 0; i < sources.length; i++) t = t.replace(sources[i], dest[i]); return t; }
    public static String nullIfEmpty(String s) { return isEmpty(s) ? null : s; }
    public static String emptyIfNull(String s) { return s == null ? "" : s; }
    public static int length(String s) { return s == null ? 0 : s.length(); }
    public static CharSequence makeSafeForPresentation(String s, int max, float w, int flags) { return s; }
    public static void writeToParcel(CharSequence c, android.os.Parcel p, int flags) { p.writeCharSequence(c); }
    public static String formatSimple(String f, Object... a) { return String.format(f, a); }
    public static void dumpSpans(CharSequence cs, android.util.Printer p, String prefix) { p.println(prefix + cs); }
    public static int unpackRangeStartFromLong(long r) { return (int) (r >>> 32); }
    public static int unpackRangeEndFromLong(long r) { return (int) (r & 0xffffffffL); }
    public static long packRangeInLong(int s, int e) { return (((long) s) << 32) | e; }
}
