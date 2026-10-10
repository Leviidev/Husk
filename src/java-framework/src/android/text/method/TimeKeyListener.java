package android.text.method;
public class TimeKeyListener extends NumberKeyListener {
    public static final char[] CHARACTERS = "0123456789amp:".toCharArray();
    public static TimeKeyListener getInstance() { return new TimeKeyListener(); } public static TimeKeyListener getInstance(java.util.Locale l) { return new TimeKeyListener(); }
    protected char[] getAcceptedChars() { return CHARACTERS; }
    public int getInputType() { return android.text.InputType.TYPE_CLASS_DATETIME | android.text.InputType.TYPE_DATETIME_VARIATION_TIME; }
}
