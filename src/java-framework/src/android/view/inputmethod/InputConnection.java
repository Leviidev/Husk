package android.view.inputmethod;
public interface InputConnection {
    boolean commitText(CharSequence t, int pos); boolean deleteSurroundingText(int before, int after); boolean sendKeyEvent(android.view.KeyEvent e);
    boolean setComposingText(CharSequence t, int pos); boolean finishComposingText(); CharSequence getTextBeforeCursor(int n, int flags);
}
