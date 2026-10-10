package android.graphics;

/**
 * Paint keeps its state in mP, the packet husk.Gfx reads in one go (the P_* slots in husk-tl-dvm-graphics.c); the objects it is handed
 * (typeface, shader, filters) are kept here and their native parts passed along with it.
 */
public class Paint {
    public static final int ANTI_ALIAS_FLAG = 1, FILTER_BITMAP_FLAG = 2, DITHER_FLAG = 4, UNDERLINE_TEXT_FLAG = 8, STRIKE_THRU_TEXT_FLAG = 16,
        FAKE_BOLD_TEXT_FLAG = 32, LINEAR_TEXT_FLAG = 64, SUBPIXEL_TEXT_FLAG = 128, EMBEDDED_BITMAP_TEXT_FLAG = 1024;
    public static final int HINTING_OFF = 0, HINTING_ON = 1;
    public static final int START_HYPHEN_EDIT_NO_EDIT = 0, END_HYPHEN_EDIT_NO_EDIT = 0;
    public enum Style { FILL, STROKE, FILL_AND_STROKE }
    public enum Cap { BUTT, ROUND, SQUARE }
    public enum Join { MITER, ROUND, BEVEL }
    public enum Align { LEFT, CENTER, RIGHT }

    static final int P_COLOR = 0, P_STYLE = 1, P_STROKE = 2, P_FLAGS = 3, P_CAP = 4, P_JOIN = 5, P_TEXTSIZE = 6, P_ALIGN = 7, P_MITER = 8, P_SHRAD = 9,
        P_SHDX = 10, P_SHDY = 11, P_SHCOL = 12, P_SCALEX = 13, P_SKEWX = 14, P_LSPACE = 15, P_XFER = 16, P_CFCOLOR = 17, P_CFMODE = 18, P_ALPHA = 19, P_COUNT = 20;
    static final int PF_AA = 1, PF_FILTER = 2, PF_FAKEBOLD = 4, PF_UNDERLINE = 8, PF_STRIKE = 16;

    /** The packet. */
    public final int[] mP = new int[P_COUNT];
    private int mFlags;
    private Typeface mTypeface;
    private Shader mShader;
    private ColorFilter mColorFilter;
    private Xfermode mXfermode;
    private PathEffect mPathEffect;
    private MaskFilter mMaskFilter;
    private Style mStyle = Style.FILL;
    private Cap mCap = Cap.BUTT;
    private Join mJoin = Join.MITER;
    private Align mAlign = Align.LEFT;
    private java.util.Locale mLocale = java.util.Locale.getDefault();
    private String mFontFeatures, mFontVariation;
    private int mHinting = HINTING_ON;
    private final float[] mMetrics = new float[5];

    public static class FontMetrics { public float top, ascent, descent, bottom, leading; }
    public static class FontMetricsInt {
        public int top, ascent, descent, bottom, leading;
        @Override public String toString() { return "FontMetricsInt: top=" + top + " ascent=" + ascent + " descent=" + descent + " bottom=" + bottom + " leading=" + leading; }
    }

    public Paint() { this(0); }
    public Paint(int flags) {
        mP[P_COLOR] = 0xFF000000;
        mP[P_TEXTSIZE] = Float.floatToRawIntBits(12f);
        mP[P_MITER] = Float.floatToRawIntBits(4f);
        mP[P_SCALEX] = Float.floatToRawIntBits(1f);
        mP[P_XFER] = 3;
        mP[P_CFMODE] = -1;
        mP[P_ALPHA] = 255;
        setFlags(flags);
    }
    public Paint(Paint p) { this(0); set(p); }

    public void set(Paint p) {
        if (p == this) return;
        System.arraycopy(p.mP, 0, mP, 0, P_COUNT);
        mFlags = p.mFlags; mTypeface = p.mTypeface; mShader = p.mShader; mColorFilter = p.mColorFilter; mXfermode = p.mXfermode;
        mPathEffect = p.mPathEffect; mMaskFilter = p.mMaskFilter; mStyle = p.mStyle; mCap = p.mCap; mJoin = p.mJoin; mAlign = p.mAlign; mLocale = p.mLocale;
    }
    public void reset() { Paint d = new Paint(); set(d); }

    private static float f(int bits) { return Float.intBitsToFloat(bits); }
    private void pf(int slot, float v) { mP[slot] = Float.floatToRawIntBits(v); }

    public int getFlags() { return mFlags; }
    public void setFlags(int flags) {
        mFlags = flags;
        int pfl = 0;
        if ((flags & ANTI_ALIAS_FLAG) != 0) pfl |= PF_AA;
        if ((flags & FILTER_BITMAP_FLAG) != 0) pfl |= PF_FILTER;
        if ((flags & FAKE_BOLD_TEXT_FLAG) != 0) pfl |= PF_FAKEBOLD;
        if ((flags & UNDERLINE_TEXT_FLAG) != 0) pfl |= PF_UNDERLINE;
        if ((flags & STRIKE_THRU_TEXT_FLAG) != 0) pfl |= PF_STRIKE;
        mP[P_FLAGS] = pfl;
    }
    private void flag(int f, boolean on) { setFlags(on ? mFlags | f : mFlags & ~f); }
    public final boolean isAntiAlias() { return (mFlags & ANTI_ALIAS_FLAG) != 0; }
    public void setAntiAlias(boolean aa) { flag(ANTI_ALIAS_FLAG, aa); }
    public final boolean isDither() { return (mFlags & DITHER_FLAG) != 0; }
    public void setDither(boolean d) { flag(DITHER_FLAG, d); }
    public final boolean isFilterBitmap() { return (mFlags & FILTER_BITMAP_FLAG) != 0; }
    public void setFilterBitmap(boolean f) { flag(FILTER_BITMAP_FLAG, f); }
    public final boolean isUnderlineText() { return (mFlags & UNDERLINE_TEXT_FLAG) != 0; }
    public void setUnderlineText(boolean u) { flag(UNDERLINE_TEXT_FLAG, u); }
    public final boolean isStrikeThruText() { return (mFlags & STRIKE_THRU_TEXT_FLAG) != 0; }
    public void setStrikeThruText(boolean s) { flag(STRIKE_THRU_TEXT_FLAG, s); }
    public final boolean isFakeBoldText() { return (mFlags & FAKE_BOLD_TEXT_FLAG) != 0; }
    public void setFakeBoldText(boolean b) { flag(FAKE_BOLD_TEXT_FLAG, b); }
    public final boolean isLinearText() { return (mFlags & LINEAR_TEXT_FLAG) != 0; }
    public void setLinearText(boolean b) { flag(LINEAR_TEXT_FLAG, b); }
    public final boolean isSubpixelText() { return (mFlags & SUBPIXEL_TEXT_FLAG) != 0; }
    public void setSubpixelText(boolean b) { flag(SUBPIXEL_TEXT_FLAG, b); }
    public int getHinting() { return mHinting; }
    public void setHinting(int h) { mHinting = h; }
    public boolean isElegantTextHeight() { return false; }
    public void setElegantTextHeight(boolean b) {}

    public Style getStyle() { return mStyle; }
    public void setStyle(Style s) { mStyle = s == null ? Style.FILL : s; mP[P_STYLE] = mStyle.ordinal(); }
    public int getColor() { return mP[P_COLOR]; }
    public long getColorLong() { return Color.pack(mP[P_COLOR]); }
    public void setColor(int c) { mP[P_COLOR] = c; }
    public void setColor(long c) { setColor(Color.toArgb(c)); }
    public int getAlpha() { return mP[P_COLOR] >>> 24; }
    public void setAlpha(int a) { mP[P_COLOR] = (mP[P_COLOR] & 0x00FFFFFF) | ((a & 0xFF) << 24); }
    public void setARGB(int a, int r, int g, int b) { setColor(Color.argb(a, r, g, b)); }
    public float getStrokeWidth() { return f(mP[P_STROKE]); }
    public void setStrokeWidth(float w) { pf(P_STROKE, w); }
    public float getStrokeMiter() { return f(mP[P_MITER]); }
    public void setStrokeMiter(float m) { pf(P_MITER, m); }
    public Cap getStrokeCap() { return mCap; }
    public void setStrokeCap(Cap c) { mCap = c == null ? Cap.BUTT : c; mP[P_CAP] = mCap.ordinal(); }
    public Join getStrokeJoin() { return mJoin; }
    public void setStrokeJoin(Join j) { mJoin = j == null ? Join.MITER : j; mP[P_JOIN] = mJoin.ordinal(); }
    public boolean getFillPath(Path src, Path dst) { dst.set(src); return true; }

    public Shader getShader() { return mShader; }
    public Shader setShader(Shader s) { mShader = s; return s; }
    public ColorFilter getColorFilter() { return mColorFilter; }
    public ColorFilter setColorFilter(ColorFilter f) {
        mColorFilter = f;
        mP[P_CFMODE] = f == null ? -1 : f.mode; mP[P_CFCOLOR] = f == null ? 0 : f.color;
        return f;
    }
    public Xfermode getXfermode() { return mXfermode; }
    public Xfermode setXfermode(Xfermode x) { mXfermode = x; mP[P_XFER] = x == null ? 3 : x.mode; return x; }
    public void setBlendMode(BlendMode m) { mP[P_XFER] = m == null ? 3 : m.n; }
    public BlendMode getBlendMode() { return null; }
    public PathEffect getPathEffect() { return mPathEffect; }
    public PathEffect setPathEffect(PathEffect e) { mPathEffect = e; return e; }
    public MaskFilter getMaskFilter() { return mMaskFilter; }
    public MaskFilter setMaskFilter(MaskFilter m) { mMaskFilter = m; return m; }
    public void setShadowLayer(float radius, float dx, float dy, int color) { pf(P_SHRAD, radius); pf(P_SHDX, dx); pf(P_SHDY, dy); mP[P_SHCOL] = color; }
    public void setShadowLayer(float radius, float dx, float dy, long color) { setShadowLayer(radius, dx, dy, Color.toArgb(color)); }
    public void clearShadowLayer() { pf(P_SHRAD, 0); }
    public float getShadowLayerRadius() { return f(mP[P_SHRAD]); }
    public float getShadowLayerDx() { return f(mP[P_SHDX]); }
    public float getShadowLayerDy() { return f(mP[P_SHDY]); }
    public int getShadowLayerColor() { return mP[P_SHCOL]; }

    public Align getTextAlign() { return mAlign; }
    public void setTextAlign(Align a) { mAlign = a == null ? Align.LEFT : a; mP[P_ALIGN] = mAlign.ordinal(); }
    public java.util.Locale getTextLocale() { return mLocale; }
    public void setTextLocale(java.util.Locale l) { mLocale = l; }
    public android.os.LocaleList getTextLocales() { return android.os.LocaleList.getDefault(); }
    public void setTextLocales(android.os.LocaleList l) {}
    public float getTextSize() { return f(mP[P_TEXTSIZE]); }
    public void setTextSize(float s) { if (s > 0) pf(P_TEXTSIZE, s); }
    public float getTextScaleX() { return f(mP[P_SCALEX]); }
    public void setTextScaleX(float s) { pf(P_SCALEX, s); }
    public float getTextSkewX() { return f(mP[P_SKEWX]); }
    public void setTextSkewX(float s) { pf(P_SKEWX, s); }
    public float getLetterSpacing() { return f(mP[P_LSPACE]); }
    public void setLetterSpacing(float s) { pf(P_LSPACE, s); }
    public float getWordSpacing() { return 0; }
    public void setWordSpacing(float s) {}
    public String getFontFeatureSettings() { return mFontFeatures; }
    public void setFontFeatureSettings(String s) { mFontFeatures = s; }
    public String getFontVariationSettings() { return mFontVariation; }
    public boolean setFontVariationSettings(String s) { mFontVariation = s; return true; }
    public int getStartHyphenEdit() { return 0; }
    public int getEndHyphenEdit() { return 0; }
    public void setStartHyphenEdit(int e) {}
    public void setEndHyphenEdit(int e) {}
    public Typeface getTypeface() { return mTypeface; }
    public Typeface setTypeface(Typeface t) { mTypeface = t; return t; }
    /** The native face and shader husk.Gfx draws with. */
    public long huskFace() { return mTypeface == null ? 0 : mTypeface.native_instance; }
    public long huskShader() { return mShader == null ? 0 : mShader.huskNative(); }

    public float ascent() { husk.Gfx.txMetrics(mP, huskFace(), mMetrics); return mMetrics[1]; }
    public float descent() { husk.Gfx.txMetrics(mP, huskFace(), mMetrics); return mMetrics[2]; }
    public float getFontMetrics(FontMetrics m) {
        float sp = husk.Gfx.txMetrics(mP, huskFace(), mMetrics);
        if (m != null) { m.top = mMetrics[0]; m.ascent = mMetrics[1]; m.descent = mMetrics[2]; m.bottom = mMetrics[3]; m.leading = mMetrics[4]; }
        return sp;
    }
    public FontMetrics getFontMetrics() { FontMetrics m = new FontMetrics(); getFontMetrics(m); return m; }
    public int getFontMetricsInt(FontMetricsInt m) {
        float sp = husk.Gfx.txMetrics(mP, huskFace(), mMetrics);
        if (m != null) {
            m.top = (int) Math.floor(mMetrics[0]); m.ascent = Math.round(mMetrics[1]); m.descent = Math.round(mMetrics[2]);
            m.bottom = (int) Math.ceil(mMetrics[3]); m.leading = Math.round(mMetrics[4]);
        }
        return Math.round(sp);
    }
    public FontMetricsInt getFontMetricsInt() { FontMetricsInt m = new FontMetricsInt(); getFontMetricsInt(m); return m; }
    public float getFontSpacing() { return getFontMetrics(null); }

    public float measureText(String s) { return s == null ? 0 : husk.Gfx.txMeasure(s, 0, s.length(), mP, huskFace()); }
    public float measureText(String s, int start, int end) { return husk.Gfx.txMeasure(s, start, end, mP, huskFace()); }
    public float measureText(CharSequence s, int start, int end) { return measureText(s.toString(), start, end); }
    public float measureText(char[] t, int index, int count) { return measureText(new String(t, index, count)); }
    public float getRunAdvance(CharSequence t, int start, int end, int cs, int ce, boolean rtl, int offset) { return measureText(t.toString(), start, offset); }
    public float getRunAdvance(char[] t, int start, int end, int cs, int ce, boolean rtl, int offset) { return measureText(new String(t), start, offset); }
    public int getOffsetForAdvance(CharSequence t, int start, int end, int cs, int ce, boolean rtl, float advance) {
        String s = t.toString();
        return start + husk.Gfx.txBreak(s, start, end, advance, mP, huskFace(), false);
    }
    public int breakText(String s, boolean forwards, float maxWidth, float[] measured) { return breakText(s, 0, s.length(), forwards, maxWidth, measured); }
    public int breakText(CharSequence s, int start, int end, boolean forwards, float maxWidth, float[] measured) {
        String str = s.toString();
        int n = husk.Gfx.txBreak(str, start, end, maxWidth, mP, huskFace(), false);
        if (n > end - start) n = end - start;
        if (measured != null && measured.length > 0) measured[0] = measureText(str, start, start + n);
        return n;
    }
    public int breakText(char[] t, int index, int count, float maxWidth, float[] measured) { return breakText(new String(t, index, count), 0, count, true, maxWidth, measured); }
    public int getTextWidths(String s, int start, int end, float[] widths) { husk.Gfx.txWidths(s, start, end, mP, huskFace(), widths); return end - start; }
    public int getTextWidths(String s, float[] widths) { return getTextWidths(s, 0, s.length(), widths); }
    public int getTextWidths(CharSequence s, int start, int end, float[] widths) { return getTextWidths(s.toString(), start, end, widths); }
    public int getTextWidths(char[] t, int index, int count, float[] widths) { return getTextWidths(new String(t, index, count), widths); }
    public void getTextBounds(String s, int start, int end, Rect bounds) {
        int[] o = new int[4];
        husk.Gfx.txBounds(s, start, end, mP, huskFace(), o);
        bounds.set(o[0], o[1], o[2], o[3]);
    }
    public void getTextBounds(CharSequence s, int start, int end, Rect bounds) { getTextBounds(s.toString(), start, end, bounds); }
    public void getTextBounds(char[] t, int index, int count, Rect bounds) { getTextBounds(new String(t, index, count), 0, count, bounds); }
    public void getTextPath(String s, int start, int end, float x, float y, Path path) { path.addRect(x, y + ascent(), x + measureText(s, start, end), y + descent(), Path.Direction.CW); }
    public boolean hasGlyph(String s) { return true; }
    public float getUnderlinePosition() { return getTextSize() / 9; }
    public float getUnderlineThickness() { return getTextSize() / 18; }
    public float getStrikeThruPosition() { return -getTextSize() * 0.3f; }
    public float getStrikeThruThickness() { return getTextSize() / 18; }
    public boolean equalsForTextMeasurement(Paint o) { return java.util.Arrays.equals(mP, o.mP) && mTypeface == o.mTypeface; }
}
