package android.view.inputmethod;

import android.view.View;

/** The keyboard: the iPhone's, shown for the focused text view and typing into its input connection (husk.InputMethods). */
public final class InputMethodManager {
    public static final int SHOW_IMPLICIT = 1, SHOW_FORCED = 2, HIDE_IMPLICIT_ONLY = 1, HIDE_NOT_ALWAYS = 2, RESULT_UNCHANGED_SHOWN = 0, RESULT_UNCHANGED_HIDDEN = 1,
        RESULT_SHOWN = 2, RESULT_HIDDEN = 3;
    public boolean showSoftInput(View v, int flags) { return husk.InputMethods.show(v); }
    public boolean showSoftInput(View v, int flags, android.os.ResultReceiver r) { boolean s = husk.InputMethods.show(v); if (r != null) r.send(s ? RESULT_SHOWN : RESULT_UNCHANGED_SHOWN, null); return s; }
    public boolean hideSoftInputFromWindow(android.os.IBinder token, int flags) { return husk.InputMethods.hide(); }
    public boolean hideSoftInputFromWindow(android.os.IBinder token, int flags, android.os.ResultReceiver r) { boolean h = husk.InputMethods.hide(); if (r != null) r.send(h ? RESULT_HIDDEN : RESULT_UNCHANGED_HIDDEN, null); return h; }
    public void toggleSoftInput(int show, int hide) { if (husk.InputMethods.shown()) husk.InputMethods.hide(); else husk.InputMethods.show(husk.InputMethods.focused()); }
    public void toggleSoftInputFromWindow(android.os.IBinder token, int show, int hide) { toggleSoftInput(show, hide); }
    public boolean isActive() { return husk.InputMethods.focused() != null; }
    public boolean isActive(View v) { return husk.InputMethods.focused() == v; }
    public boolean isAcceptingText() { return husk.InputMethods.shown(); }
    public boolean isFullscreenMode() { return false; }
    public void restartInput(View v) { husk.InputMethods.restart(v); }
    public void updateSelection(View v, int ss, int se, int cs, int ce) {}
    public void updateCursorAnchorInfo(View v, CursorAnchorInfo info) {}
    public void updateExtractedText(View v, int token, ExtractedText t) {}
    public void displayCompletions(View v, CompletionInfo[] c) {}
    public void viewClicked(View v) {}
    public void sendAppPrivateCommand(View v, String action, android.os.Bundle data) {}
    public java.util.List<InputMethodInfo> getInputMethodList() { return new java.util.ArrayList<>(); }
    public java.util.List<InputMethodInfo> getEnabledInputMethodList() { return new java.util.ArrayList<>(); }
    public InputMethodSubtype getCurrentInputMethodSubtype() { return null; }
    public void showInputMethodPicker() {}
    public boolean isInputMethodSuppressingSpellChecker() { return false; }
    public void startStylusHandwriting(View v) {}
}
