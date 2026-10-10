package android.text.style;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.Typeface;

public class TextAppearanceSpan extends MetricAffectingSpan {
    private final String mFamily; private final int mStyle, mSize; private final ColorStateList mColor, mLinkColor;
    public TextAppearanceSpan(Context c, int appearance) { this(c, appearance, -1); }
    public TextAppearanceSpan(Context c, int appearance, int colorList) {
        TypedArray a = c.obtainStyledAttributes(appearance, husk.S.TextAppearance);
        ColorStateList col = a.getColorStateList(husk.S.TextAppearance_textColor);
        mLinkColor = a.getColorStateList(husk.S.TextAppearance_textColorLink);
        mSize = a.getDimensionPixelSize(husk.S.TextAppearance_textSize, -1);
        mStyle = a.getInt(husk.S.TextAppearance_textStyle, 0);
        String fam = a.getString(husk.S.TextAppearance_fontFamily);
        if (fam == null) { int tf = a.getInt(husk.S.TextAppearance_typeface, 0); fam = tf == 1 ? "sans" : tf == 2 ? "serif" : tf == 3 ? "monospace" : null; }
        mFamily = fam;
        a.recycle();
        mColor = col;
    }
    public TextAppearanceSpan(String family, int style, int size, ColorStateList color, ColorStateList linkColor) { mFamily = family; mStyle = style; mSize = size; mColor = color; mLinkColor = linkColor; }
    public String getFamily() { return mFamily; } public ColorStateList getTextColor() { return mColor; } public ColorStateList getLinkTextColor() { return mLinkColor; } public int getTextSize() { return mSize; } public int getTextStyle() { return mStyle; }
    @Override public void updateDrawState(android.text.TextPaint tp) {
        updateMeasureState(tp);
        if (mColor != null) tp.setColor(mColor.getColorForState(tp.drawableState, 0));
        if (mLinkColor != null) tp.linkColor = mLinkColor.getColorForState(tp.drawableState, 0);
    }
    @Override public void updateMeasureState(android.text.TextPaint tp) {
        if (mFamily != null || mStyle != 0) { Typeface t = tp.getTypeface(); int st = (t == null ? 0 : t.getStyle()) | mStyle; tp.setTypeface(mFamily != null ? Typeface.create(mFamily, st) : Typeface.create(t, st)); }
        if (mSize > 0) tp.setTextSize(mSize);
    }
}
