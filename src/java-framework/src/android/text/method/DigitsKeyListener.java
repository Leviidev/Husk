package android.text.method;
public class DigitsKeyListener extends NumberKeyListener {
    private final char[] mAccepted; private final boolean mSign, mDecimal;
    public DigitsKeyListener() { this(false, false); }
    public DigitsKeyListener(boolean sign, boolean decimal) { mSign = sign; mDecimal = decimal; mAccepted = (("0123456789") + (sign ? "-+" : "") + (decimal ? "." : "")).toCharArray(); }
    public DigitsKeyListener(java.util.Locale l) { this(false, false); }
    public DigitsKeyListener(java.util.Locale l, boolean sign, boolean decimal) { this(sign, decimal); }
    private DigitsKeyListener(String accepted) { mSign = false; mDecimal = false; mAccepted = accepted.toCharArray(); }
    public static DigitsKeyListener getInstance() { return new DigitsKeyListener(); }
    public static DigitsKeyListener getInstance(boolean sign, boolean decimal) { return new DigitsKeyListener(sign, decimal); }
    public static DigitsKeyListener getInstance(java.util.Locale l) { return new DigitsKeyListener(); }
    public static DigitsKeyListener getInstance(java.util.Locale l, boolean sign, boolean decimal) { return new DigitsKeyListener(sign, decimal); }
    public static DigitsKeyListener getInstance(String accepted) { return new DigitsKeyListener(accepted); }
    protected char[] getAcceptedChars() { return mAccepted; }
    public int getInputType() { int t = android.text.InputType.TYPE_CLASS_NUMBER; if (mSign) t |= android.text.InputType.TYPE_NUMBER_FLAG_SIGNED; if (mDecimal) t |= android.text.InputType.TYPE_NUMBER_FLAG_DECIMAL; return t; }
}
