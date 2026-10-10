package android.view.inputmethod;

import android.os.Bundle;
import android.view.KeyEvent;

public interface InputConnection {
    int GET_TEXT_WITH_STYLES = 1, GET_EXTRACTED_TEXT_MONITOR = 1, INPUT_CONTENT_GRANT_READ_URI_PERMISSION = 1, CURSOR_UPDATE_IMMEDIATE = 1, CURSOR_UPDATE_MONITOR = 2;
    CharSequence getTextBeforeCursor(int n, int flags);
    CharSequence getTextAfterCursor(int n, int flags);
    CharSequence getSelectedText(int flags);
    int getCursorCapsMode(int reqModes);
    ExtractedText getExtractedText(ExtractedTextRequest request, int flags);
    boolean deleteSurroundingText(int before, int after);
    boolean deleteSurroundingTextInCodePoints(int before, int after);
    boolean setComposingText(CharSequence text, int newCursorPosition);
    boolean setComposingRegion(int start, int end);
    boolean finishComposingText();
    boolean commitText(CharSequence text, int newCursorPosition);
    boolean commitCompletion(CompletionInfo text);
    boolean commitCorrection(CorrectionInfo info);
    boolean setSelection(int start, int end);
    boolean performEditorAction(int editorAction);
    boolean performContextMenuAction(int id);
    boolean beginBatchEdit();
    boolean endBatchEdit();
    boolean sendKeyEvent(KeyEvent event);
    boolean clearMetaKeyStates(int states);
    boolean reportFullscreenMode(boolean enabled);
    boolean performPrivateCommand(String action, Bundle data);
    boolean requestCursorUpdates(int cursorUpdateMode);
    android.os.Handler getHandler();
    void closeConnection();
    boolean commitContent(InputContentInfo info, int flags, Bundle opts);
    default SurroundingText getSurroundingText(int before, int after, int flags) { return null; }
    default boolean setImeConsumesInput(boolean c) { return false; }
    default TextSnapshot takeSnapshot() { return null; }
}
