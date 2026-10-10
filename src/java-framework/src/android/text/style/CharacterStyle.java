package android.text.style;
public abstract class CharacterStyle {
    public abstract void updateDrawState(android.text.TextPaint tp);
    public static CharacterStyle wrap(CharacterStyle cs) { return cs; }
    public CharacterStyle getUnderlying() { return this; }
}
