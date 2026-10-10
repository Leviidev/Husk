package android.text.method;
public class HideReturnsTransformationMethod extends ReplacementTransformationMethod {
    private static final char[] ORIGINAL = { '\r' }, REPLACEMENT = { '\uFEFF' };
    private static HideReturnsTransformationMethod sInstance;
    protected char[] getOriginal() { return ORIGINAL; }
    protected char[] getReplacement() { return REPLACEMENT; }
    public static HideReturnsTransformationMethod getInstance() { if (sInstance == null) sInstance = new HideReturnsTransformationMethod(); return sInstance; }
}
