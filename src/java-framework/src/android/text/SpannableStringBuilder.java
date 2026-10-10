package android.text;
public class SpannableStringBuilder implements Editable {
    private final StringBuilder b;
    public SpannableStringBuilder() { b = new StringBuilder(); }
    public SpannableStringBuilder(CharSequence c) { b = new StringBuilder(c == null ? "" : c); }
    public int length() { return b.length(); } public char charAt(int i) { return b.charAt(i); }
    public CharSequence subSequence(int s, int e) { return b.subSequence(s, e); } public String toString() { return b.toString(); }
    public Editable append(CharSequence c) { b.append(c); return this; }
    public Editable replace(int s, int e, CharSequence c) { b.replace(s, e, c.toString()); return this; }
    public Editable delete(int s, int e) { b.delete(s, e); return this; } public void clear() { b.setLength(0); }
    public void setSpan(Object w, int s, int e, int f) {} public void removeSpan(Object w) {}
}
