package android.view.inputmethod;

import android.os.Bundle;
import android.view.KeyEvent;

public class InputConnectionWrapper implements InputConnection {
    private InputConnection mTarget;
    public InputConnectionWrapper(InputConnection target, boolean mutable) { mTarget = target; }
    public void setTarget(InputConnection t) { mTarget = t; }
    public CharSequence getTextBeforeCursor(int n, int f) { return mTarget.getTextBeforeCursor(n, f); }
    public CharSequence getTextAfterCursor(int n, int f) { return mTarget.getTextAfterCursor(n, f); }
    public CharSequence getSelectedText(int f) { return mTarget.getSelectedText(f); }
    public int getCursorCapsMode(int r) { return mTarget.getCursorCapsMode(r); }
    public ExtractedText getExtractedText(ExtractedTextRequest r, int f) { return mTarget.getExtractedText(r, f); }
    public boolean deleteSurroundingText(int b, int a) { return mTarget.deleteSurroundingText(b, a); }
    public boolean deleteSurroundingTextInCodePoints(int b, int a) { return mTarget.deleteSurroundingTextInCodePoints(b, a); }
    public boolean setComposingText(CharSequence t, int p) { return mTarget.setComposingText(t, p); }
    public boolean setComposingRegion(int s, int e) { return mTarget.setComposingRegion(s, e); }
    public boolean finishComposingText() { return mTarget.finishComposingText(); }
    public boolean commitText(CharSequence t, int p) { return mTarget.commitText(t, p); }
    public boolean commitCompletion(CompletionInfo t) { return mTarget.commitCompletion(t); }
    public boolean commitCorrection(CorrectionInfo i) { return mTarget.commitCorrection(i); }
    public boolean setSelection(int s, int e) { return mTarget.setSelection(s, e); }
    public boolean performEditorAction(int a) { return mTarget.performEditorAction(a); }
    public boolean performContextMenuAction(int id) { return mTarget.performContextMenuAction(id); }
    public boolean beginBatchEdit() { return mTarget.beginBatchEdit(); }
    public boolean endBatchEdit() { return mTarget.endBatchEdit(); }
    public boolean sendKeyEvent(KeyEvent e) { return mTarget.sendKeyEvent(e); }
    public boolean clearMetaKeyStates(int s) { return mTarget.clearMetaKeyStates(s); }
    public boolean reportFullscreenMode(boolean e) { return mTarget.reportFullscreenMode(e); }
    public boolean performPrivateCommand(String a, Bundle d) { return mTarget.performPrivateCommand(a, d); }
    public boolean requestCursorUpdates(int m) { return mTarget.requestCursorUpdates(m); }
    public android.os.Handler getHandler() { return mTarget.getHandler(); }
    public void closeConnection() { mTarget.closeConnection(); }
    public boolean commitContent(InputContentInfo i, int f, Bundle o) { return mTarget.commitContent(i, f, o); }
}
