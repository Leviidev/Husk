package android.text.method;
public class TextKeyListener extends BaseKeyListener {
    public enum Capitalize { NONE, SENTENCES, WORDS, CHARACTERS }
    private final Capitalize mCap; private final boolean mAuto;
    public TextKeyListener(Capitalize cap, boolean autotext) { mCap = cap; mAuto = autotext; }
    public static TextKeyListener getInstance() { return new TextKeyListener(Capitalize.NONE, false); }
    public static TextKeyListener getInstance(boolean autotext, Capitalize cap) { return new TextKeyListener(cap, autotext); }
    public int getInputType() { int t = android.text.InputType.TYPE_CLASS_TEXT; if (mCap == Capitalize.SENTENCES) t |= android.text.InputType.TYPE_TEXT_FLAG_CAP_SENTENCES; else if (mCap == Capitalize.WORDS) t |= android.text.InputType.TYPE_TEXT_FLAG_CAP_WORDS; else if (mCap == Capitalize.CHARACTERS) t |= android.text.InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS; if (mAuto) t |= android.text.InputType.TYPE_TEXT_FLAG_AUTO_CORRECT; return t; }
    public static void clear(android.text.Editable e) { e.clear(); }
    public static boolean shouldCap(Capitalize cap, CharSequence cs, int off) { return cap != Capitalize.NONE && (cap == Capitalize.CHARACTERS || android.text.TextUtils.getCapsMode(cs, off, android.text.InputType.TYPE_TEXT_FLAG_CAP_SENTENCES) != 0); }
}
