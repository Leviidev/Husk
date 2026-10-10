package android.view.inputmethod;
public class EditorInfo {
    public static final int IME_ACTION_DONE = 6, IME_ACTION_GO = 2, IME_ACTION_NEXT = 5, IME_ACTION_SEARCH = 3, IME_ACTION_SEND = 4, IME_FLAG_NO_EXTRACT_UI = 0x10000000,
        IME_FLAG_NO_FULLSCREEN = 0x2000000, IME_MASK_ACTION = 255, TYPE_CLASS_TEXT = 1;
    public int inputType, imeOptions, initialSelStart, initialSelEnd;
    public CharSequence hintText;
}
