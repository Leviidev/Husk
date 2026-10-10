package android.text.method;
import android.text.Editable;
import android.text.Selection;
import android.view.KeyEvent;
import android.view.View;
/** Typing into an Editable from key events: printable keys insert, delete removes before the cursor. */
public abstract class BaseKeyListener extends MetaKeyKeyListener implements KeyListener {
    public boolean backspace(View v, Editable c, int code, KeyEvent e) {
        int a = Selection.getSelectionStart(c), b = Selection.getSelectionEnd(c);
        if (a < 0) return false;
        if (a != b) { c.delete(Math.min(a, b), Math.max(a, b)); return true; }
        if (a > 0) { int n = a >= 2 && Character.isSurrogatePair(c.charAt(a - 2), c.charAt(a - 1)) ? 2 : 1; c.delete(a - n, a); return true; }
        return false;
    }
    public boolean forwardDelete(View v, Editable c, int code, KeyEvent e) {
        int a = Selection.getSelectionStart(c), b = Selection.getSelectionEnd(c);
        if (a < 0) return false;
        if (a != b) { c.delete(Math.min(a, b), Math.max(a, b)); return true; }
        if (a < c.length()) { c.delete(a, a + 1); return true; }
        return false;
    }
    @Override public boolean onKeyDown(View v, Editable c, int code, KeyEvent e) {
        if (code == KeyEvent.KEYCODE_DEL) return backspace(v, c, code, e);
        if (code == KeyEvent.KEYCODE_FORWARD_DEL) return forwardDelete(v, c, code, e);
        int ch = e.getUnicodeChar();
        if (ch != 0 && ch != '\n' && acceptChar((char) ch)) {
            int a = Selection.getSelectionStart(c), b = Selection.getSelectionEnd(c);
            if (a < 0) { a = b = c.length(); }
            c.replace(Math.min(a, b), Math.max(a, b), String.valueOf((char) ch));
            return true;
        }
        return super.onKeyDown(v, c, code, e);
    }
    protected boolean acceptChar(char c) { return true; }
    public boolean onKeyOther(View v, Editable c, KeyEvent e) {
        if (e.getAction() != KeyEvent.ACTION_MULTIPLE || e.getKeyCode() != KeyEvent.KEYCODE_UNKNOWN) return false;
        int a = Selection.getSelectionStart(c), b = Selection.getSelectionEnd(c);
        if (a < 0) a = b = c.length();
        String t = e.getCharacters();
        if (t == null) return false;
        c.replace(Math.min(a, b), Math.max(a, b), t);
        return true;
    }
}
