package android.text.style;
public class MaskFilterSpan extends CharacterStyle implements UpdateAppearance { private final android.graphics.MaskFilter m; public MaskFilterSpan(android.graphics.MaskFilter f) { m = f; } public android.graphics.MaskFilter getMaskFilter() { return m; } @Override public void updateDrawState(android.text.TextPaint tp) { tp.setMaskFilter(m); }
}
