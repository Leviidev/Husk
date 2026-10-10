package android.graphics.fonts;

import java.util.ArrayList;

/** Fonts of one family in their styles: a Typeface draws with the one closest to the style asked for. */
public final class FontFamily {
    final ArrayList<Font> mFonts;
    FontFamily(ArrayList<Font> fonts) { mFonts = fonts; }
    public FontFamily(long p0) { this(new ArrayList<Font>()); }
    public Font getFont(int i) { return mFonts.get(i); }
    public int getSize() { return mFonts.size(); }
    public String getLangTags() { return null; }
    public int getVariant() { return 0; }
    public long getNativePtr() { return 0L; }

    /** The font closest to a style. */
    public Font huskClosest(FontStyle s) {
        Font best = null; int score = Integer.MAX_VALUE;
        for (Font f : mFonts) { int k = f.getStyle().getMatchScore(s); if (k < score) { score = k; best = f; } }
        return best;
    }

    public static final class Builder {
        public static final int VARIABLE_FONT_FAMILY_TYPE_NONE = 0, VARIABLE_FONT_FAMILY_TYPE_SINGLE_FONT_WGHT_ONLY = 1,
                VARIABLE_FONT_FAMILY_TYPE_SINGLE_FONT_WGHT_ITAL = 2, VARIABLE_FONT_FAMILY_TYPE_TWO_FONTS_WGHT = 3, VARIABLE_FONT_FAMILY_TYPE_UNKNOWN = -1;
        private final ArrayList<Font> mFonts = new ArrayList<>();
        public Builder(Font f) { mFonts.add(f); }
        public Builder addFont(Font f) {
            for (Font g : mFonts) if (g.getStyle().equals(f.getStyle())) throw new IllegalArgumentException("Font with the same style is already added: " + f.getStyle());
            mFonts.add(f); return this;
        }
        public FontFamily build() { return new FontFamily(new ArrayList<>(mFonts)); }
        public FontFamily build(String langTags, int variant, boolean isCustomFallback, boolean isDefaultFallback, int variableFamilyType) { return build(); }
        public FontFamily buildVariableFamily() { return mFonts.size() == 1 ? build() : null; }
        public static int analyzeAndResolveVariableType(ArrayList fonts) { return VARIABLE_FONT_FAMILY_TYPE_NONE; }
    }
}
