package android.view.inputmethod;

import android.os.Bundle;
import android.text.Editable;
import android.text.Selection;
import android.view.KeyEvent;
import android.view.View;

/** An input connection over an Editable: text committed at the cursor, deletes around it, keys sent to the view. */
public class BaseInputConnection implements InputConnection {
    protected final InputMethodManager mIMM;
    final View mTargetView;
    final boolean mFallbackMode;
    private Editable mDefaultEditable;
    private int mComposingStart = -1, mComposingEnd = -1, mBatch;
    public BaseInputConnection(View targetView, boolean fullEditor) {
        mIMM = (InputMethodManager) targetView.getContext().getSystemService(android.content.Context.INPUT_METHOD_SERVICE);
        mTargetView = targetView; mFallbackMode = !fullEditor;
    }
    public Editable getEditable() {
        if (mDefaultEditable == null) { mDefaultEditable = Editable.Factory.getInstance().newEditable(""); Selection.setSelection(mDefaultEditable, 0); }
        return mDefaultEditable;
    }
    public static int getComposingSpanStart(android.text.Spannable t) { return -1; }
    public static int getComposingSpanEnd(android.text.Spannable t) { return -1; }
    public static void removeComposingSpans(android.text.Spannable t) {}
    public static void setComposingSpans(android.text.Spannable t) {}
    public boolean beginBatchEdit() { mBatch++; return true; }
    public boolean endBatchEdit() { if (mBatch > 0) mBatch--; return mBatch > 0; }
    public void closeConnection() { finishComposingText(); }
    public boolean clearMetaKeyStates(int s) { return false; }
    public boolean commitCompletion(CompletionInfo t) { return false; }
    public boolean commitCorrection(CorrectionInfo i) { return false; }
    public boolean commitText(CharSequence text, int pos) { replace(text, pos, false); return true; }
    public boolean deleteSurroundingText(int before, int after) {
        Editable c = getEditable(); if (c == null) return false;
        int a = Selection.getSelectionStart(c), b = Selection.getSelectionEnd(c);
        if (a > b) { int t = a; a = b; b = t; }
        if (a < 0) a = b = c.length();
        int de = Math.min(c.length(), b + after);
        if (de > b) c.delete(b, de);
        int ds = Math.max(0, a - before);
        if (ds < a) c.delete(ds, a);
        mComposingStart = mComposingEnd = -1;
        return true;
    }
    public boolean deleteSurroundingTextInCodePoints(int before, int after) { return deleteSurroundingText(before, after); }
    public boolean finishComposingText() { mComposingStart = mComposingEnd = -1; return true; }
    public int getCursorCapsMode(int req) { Editable c = getEditable(); if (c == null) return 0; int a = Selection.getSelectionStart(c); return android.text.TextUtils.getCapsMode(c, Math.max(0, a), req); }
    public ExtractedText getExtractedText(ExtractedTextRequest r, int f) { return null; }
    public CharSequence getTextBeforeCursor(int n, int flags) {
        Editable c = getEditable(); if (c == null) return null;
        int a = Math.min(Selection.getSelectionStart(c), Selection.getSelectionEnd(c));
        if (a <= 0) return "";
        if (n > a) n = a;
        return c.subSequence(a - n, a).toString();
    }
    public CharSequence getSelectedText(int flags) {
        Editable c = getEditable(); if (c == null) return null;
        int a = Selection.getSelectionStart(c), b = Selection.getSelectionEnd(c);
        if (a == b || a < 0) return null;
        return c.subSequence(Math.min(a, b), Math.max(a, b)).toString();
    }
    public CharSequence getTextAfterCursor(int n, int flags) {
        Editable c = getEditable(); if (c == null) return null;
        int b = Math.max(Selection.getSelectionStart(c), Selection.getSelectionEnd(c));
        if (b < 0) b = 0;
        if (b + n > c.length()) n = c.length() - b;
        return c.subSequence(b, b + n).toString();
    }
    public boolean performEditorAction(int action) {
        long t = android.os.SystemClock.uptimeMillis();
        sendKeyEvent(new KeyEvent(t, t, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER, 0));
        sendKeyEvent(new KeyEvent(t, t, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER, 0));
        return true;
    }
    public boolean performContextMenuAction(int id) { return false; }
    public boolean performPrivateCommand(String a, Bundle d) { return false; }
    public boolean requestCursorUpdates(int m) { return false; }
    public android.os.Handler getHandler() { return null; }
    public boolean setComposingText(CharSequence text, int pos) { replace(text, pos, true); return true; }
    public boolean setComposingRegion(int s, int e) { mComposingStart = Math.min(s, e); mComposingEnd = Math.max(s, e); return true; }
    public boolean setSelection(int s, int e) { Editable c = getEditable(); if (c == null) return false; int len = c.length(); if (s > len || e > len || s < 0 || e < 0) return true; Selection.setSelection(c, s, e); return true; }
    public boolean sendKeyEvent(KeyEvent e) { mTargetView.dispatchKeyEvent(e); return false; }
    public boolean reportFullscreenMode(boolean e) { return true; }
    public boolean commitContent(InputContentInfo i, int f, Bundle o) { return false; }
    private void replace(CharSequence text, int newCursor, boolean composing) {
        Editable c = getEditable(); if (c == null) return;
        int a, b;
        if (mComposingStart >= 0) { a = mComposingStart; b = mComposingEnd; }
        else { a = Selection.getSelectionStart(c); b = Selection.getSelectionEnd(c); if (a < 0) a = 0; if (b < 0) b = 0; if (b < a) { int t = a; a = b; b = t; } }
        a = Math.min(a, c.length()); b = Math.min(b, c.length());
        c.replace(a, b, text);
        if (composing) { mComposingStart = a; mComposingEnd = a + text.length(); } else { mComposingStart = mComposingEnd = -1; }
        int cursor = newCursor > 0 ? a + text.length() + newCursor - 1 : a + newCursor;
        if (cursor < 0) cursor = 0; if (cursor > c.length()) cursor = c.length();
        Selection.setSelection(c, cursor);
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public static void setComposingSpans(android.text.Spannable p0, int p1, int p2) {}
    public void endComposingRegionEditInternal() {}
    public boolean replaceText(int p0, int p1, java.lang.CharSequence p2, int p3, android.view.inputmethod.TextAttribute p4) { return false; }
    // ---- end of generated members
}
