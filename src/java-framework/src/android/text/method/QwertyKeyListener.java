package android.text.method;
public class QwertyKeyListener extends BaseKeyListener {
    private final TextKeyListener.Capitalize mCap;
    public QwertyKeyListener(TextKeyListener.Capitalize cap, boolean autotext) { mCap = cap; }
    public static QwertyKeyListener getInstance(boolean autotext, TextKeyListener.Capitalize cap) { return new QwertyKeyListener(cap, autotext); }
    public static QwertyKeyListener getInstanceForFullKeyboard() { return new QwertyKeyListener(TextKeyListener.Capitalize.NONE, false); }
    public int getInputType() { return android.text.InputType.TYPE_CLASS_TEXT; }
}
