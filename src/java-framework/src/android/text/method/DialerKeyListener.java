package android.text.method;
public class DialerKeyListener extends NumberKeyListener {
    public static final char[] CHARACTERS = "0123456789#*+-(). ,/N;".toCharArray();
    public static DialerKeyListener getInstance() { return new DialerKeyListener(); }
    protected char[] getAcceptedChars() { return CHARACTERS; }
    public int getInputType() { return android.text.InputType.TYPE_CLASS_PHONE; }
}
