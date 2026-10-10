package android.text;
public final class SpannedString extends SpannableStringInternal implements CharSequence, GetChars, Spanned {
    public SpannedString(CharSequence s) { super(s, 0, s.length()); }
    SpannedString(CharSequence s, int start, int end) { super(s, start, end); }
    public static SpannedString valueOf(CharSequence s) { return s instanceof SpannedString ? (SpannedString) s : new SpannedString(s); }
    public CharSequence subSequence(int start, int end) { return new SpannedString(this, start, end); }
}
