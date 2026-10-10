package android.text.method;
public class DateKeyListener extends NumberKeyListener {
    public static final char[] CHARACTERS = "0123456789/-.".toCharArray();
    public DateKeyListener() {} public DateKeyListener(java.util.Locale l) {}
    public static DateKeyListener getInstance() { return new DateKeyListener(); } public static DateKeyListener getInstance(java.util.Locale l) { return new DateKeyListener(); }
    protected char[] getAcceptedChars() { return CHARACTERS; }
    public int getInputType() { return android.text.InputType.TYPE_CLASS_DATETIME | android.text.InputType.TYPE_DATETIME_VARIATION_DATE; }
}
