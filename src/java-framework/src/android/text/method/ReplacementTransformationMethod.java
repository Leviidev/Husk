package android.text.method;
public abstract class ReplacementTransformationMethod implements TransformationMethod {
    protected abstract char[] getOriginal();
    protected abstract char[] getReplacement();
    public CharSequence getTransformation(CharSequence source, android.view.View v) {
        char[] o = getOriginal(), r = getReplacement();
        StringBuilder b = new StringBuilder(source);
        for (int i = 0; i < b.length(); i++) for (int k = 0; k < o.length; k++) if (b.charAt(i) == o[k]) b.setCharAt(i, r[k]);
        return b.toString();
    }
    public void onFocusChanged(android.view.View v, CharSequence s, boolean f, int d, android.graphics.Rect p) {}
}
