package android.text.method;
public class SingleLineTransformationMethod extends ReplacementTransformationMethod {
    private static final char[] ORIGINAL = { '\n', '\r' }, REPLACEMENT = { ' ', '\uFEFF' };
    private static SingleLineTransformationMethod sInstance;
    protected char[] getOriginal() { return ORIGINAL; }
    protected char[] getReplacement() { return REPLACEMENT; }
    public static SingleLineTransformationMethod getInstance() { if (sInstance == null) sInstance = new SingleLineTransformationMethod(); return sInstance; }
}
