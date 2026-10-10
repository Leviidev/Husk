package android.text.method;
public abstract class NumberKeyListener extends BaseKeyListener implements android.text.InputFilter {
    protected abstract char[] getAcceptedChars();
    @Override protected boolean acceptChar(char c) { for (char a : getAcceptedChars()) if (a == c) return true; return false; }
    public CharSequence filter(CharSequence source, int start, int end, android.text.Spanned dest, int dstart, int dend) {
        StringBuilder b = null;
        for (int i = start; i < end; i++) { char c = source.charAt(i); if (!acceptChar(c)) { if (b == null) { b = new StringBuilder(); b.append(source, start, i); } } else if (b != null) b.append(c); }
        return b == null ? null : b.toString();
    }
    protected static boolean ok(char[] accept, char c) { for (char a : accept) if (a == c) return true; return false; }
}
