package android.text;
public class TextPaint extends android.graphics.Paint {
    public int bgColor, baselineShift, linkColor, underlineColor;
    public int[] drawableState;
    public float density = 1.0f, underlineThickness;
    public TextPaint() { super(); }
    public TextPaint(int flags) { super(flags); }
    public TextPaint(android.graphics.Paint p) { super(p); }
    public void set(TextPaint tp) { super.set(tp); bgColor = tp.bgColor; baselineShift = tp.baselineShift; linkColor = tp.linkColor; drawableState = tp.drawableState; density = tp.density; underlineColor = tp.underlineColor; underlineThickness = tp.underlineThickness; }
    public void setUnderlineText(int color, float thickness) { underlineColor = color; underlineThickness = thickness; }
    public float getUnderlineThickness() { return underlineThickness > 0 ? underlineThickness : super.getUnderlineThickness(); }
}
