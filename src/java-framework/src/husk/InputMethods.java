package husk;

import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

/**
 * The soft keyboard: the host's keyboard comes up for a focused view that edits text, and what is typed goes to that view's input
 * connection (text committed, backspace as a delete before the cursor, return as the editor action). Text arrives through
 * Native.pollText as a list of strings: "t<text>" commit, "d" delete one, "e" the editor action.
 */
public final class InputMethods {
    private static View sFocused;
    private static InputConnection sConn;
    private static EditorInfo sInfo;
    private static boolean sShown;
    private InputMethods() {}
    public static View focused() { return sFocused; }
    public static boolean shown() { return sShown; }
    public static void focusChanged(View old, View now) {
        if (now != null && now.onCheckIsTextEditor()) { start(now); }
        else if (old != null && old == sFocused) { finish(); hide(); }
    }
    private static void start(View v) {
        if (sFocused == v && sConn != null) return;
        if (sConn != null) sConn.finishComposingText();
        sFocused = v;
        sInfo = new EditorInfo();
        sConn = v.onCreateInputConnection(sInfo);
    }
    private static void finish() { if (sConn != null) sConn.finishComposingText(); sConn = null; sFocused = null; }
    public static void restart(View v) { if (v == sFocused) { sConn = null; start(v); if (sShown) Native.showKeyboard(true, sInfo.inputType, sInfo.imeOptions); } }
    public static boolean show(View v) {
        if (v == null) return false;
        if (v != sFocused) start(v);
        if (sConn == null) return false;
        sShown = true;
        Native.showKeyboard(true, sInfo != null ? sInfo.inputType : 1, sInfo != null ? sInfo.imeOptions : 0);
        return true;
    }
    public static boolean hide() { boolean was = sShown; sShown = false; Native.showKeyboard(false, 0, 0); return was; }
    /** The host has text for us (an event in the input queue said so). */
    public static void deliverPending() {
        String[] items = Native.pollText();
        if (items == null) return;
        for (String it : items) {
            if (it.isEmpty()) continue;
            char k = it.charAt(0);
            if (k == 'h') { sShown = false; continue; }           /* the host closed the keyboard */
            if (sConn == null) {
                if (sFocused != null) { long t = android.os.SystemClock.uptimeMillis(); if (k == 'd') { sFocused.dispatchKeyEvent(new KeyEvent(t, t, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL, 0)); sFocused.dispatchKeyEvent(new KeyEvent(t, t, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL, 0)); } }
                continue;
            }
            switch (k) {
            case 't': sConn.commitText(it.substring(1), 1); break;
            case 'd': {
                CharSequence sel = sConn.getSelectedText(0);
                if (sel != null && sel.length() > 0) sConn.commitText("", 1);
                else {
                    CharSequence before = sConn.getTextBeforeCursor(2, 0);
                    int n = before != null && before.length() == 2 && Character.isSurrogatePair(before.charAt(0), before.charAt(1)) ? 2 : 1;
                    if (before == null || before.length() == 0) { long t = android.os.SystemClock.uptimeMillis(); sConn.sendKeyEvent(new KeyEvent(t, t, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL, 0)); sConn.sendKeyEvent(new KeyEvent(t, t, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL, 0)); }
                    else sConn.deleteSurroundingText(n, 0);
                }
                break;
            }
            case 'e': {
                int action = sInfo != null ? sInfo.imeOptions & EditorInfo.IME_MASK_ACTION : EditorInfo.IME_ACTION_DONE;
                boolean multi = sInfo != null && (sInfo.inputType & android.text.InputType.TYPE_TEXT_FLAG_MULTI_LINE) != 0 && (sInfo.imeOptions & EditorInfo.IME_FLAG_NO_ENTER_ACTION) == 0;
                if (multi) sConn.commitText("\n", 1);
                else sConn.performEditorAction(action == EditorInfo.IME_ACTION_UNSPECIFIED ? EditorInfo.IME_ACTION_DONE : action);
                break;
            }
            }
        }
    }
    /** A hardware key with a text view focused: printable keys become text there. */
    public static boolean key(KeyEvent e) { return false; }
}
