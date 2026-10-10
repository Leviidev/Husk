package android.view.inputmethod;
public class BaseInputConnection implements InputConnection {
    protected final android.view.View view;
    public BaseInputConnection(android.view.View v, boolean full) { view = v; }
    public boolean commitText(CharSequence t, int pos) { return true; }
    public boolean deleteSurroundingText(int b, int a) { return true; }
    public boolean sendKeyEvent(android.view.KeyEvent e) { return view.dispatchKeyEvent(e); }
    public boolean setComposingText(CharSequence t, int pos) { return true; }
    public boolean finishComposingText() { return true; }
    public CharSequence getTextBeforeCursor(int n, int f) { return ""; }
    public android.text.Editable getEditable() { return new android.text.SpannableStringBuilder(""); }
}
