package android.text.method;
public class PasswordTransformationMethod implements TransformationMethod, android.text.TextWatcher {
    private static PasswordTransformationMethod sInstance;
    public static PasswordTransformationMethod getInstance() { if (sInstance == null) sInstance = new PasswordTransformationMethod(); return sInstance; }
    public CharSequence getTransformation(CharSequence source, android.view.View v) {
        final CharSequence src = source;
        return new CharSequence() {
            public int length() { return src.length(); }
            public char charAt(int i) { return '\u2022'; }
            public CharSequence subSequence(int s, int e) { StringBuilder b = new StringBuilder(); for (int i = s; i < e; i++) b.append('\u2022'); return b.toString(); }
            @Override public String toString() { return subSequence(0, length()).toString(); }
        };
    }
    public void onFocusChanged(android.view.View v, CharSequence s, boolean f, int d, android.graphics.Rect p) {}
    public void beforeTextChanged(CharSequence s, int a, int b, int c) {}
    public void onTextChanged(CharSequence s, int a, int b, int c) {}
    public void afterTextChanged(android.text.Editable s) {}
}
