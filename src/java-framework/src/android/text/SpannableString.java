package android.text;
public class SpannableString extends SpannableStringInternal implements CharSequence, GetChars, Spannable {
    public SpannableString(CharSequence s) { super(s, 0, s.length()); }
    private SpannableString(CharSequence s, boolean ignoreNoCopySpan) { super(s, 0, s.length()); }
    SpannableString(CharSequence s, int start, int end) { super(s, start, end); }
    public static SpannableString valueOf(CharSequence s) { return s instanceof SpannableString ? (SpannableString) s : new SpannableString(s); }
    public void setSpan(Object what, int start, int end, int flags) {
        if (start < 0 || end > mLen || start > end) throw new IndexOutOfBoundsException("setSpan (" + start + " ... " + end + ") ends beyond length " + mLen);
        addSpan(what, start, end, flags);
    }
    public void removeSpan(Object what) { int i = indexOfSpan(what); if (i >= 0) removeSpanAt(i); }
    public void removeSpan(Object what, int flags) { removeSpan(what); }
}
