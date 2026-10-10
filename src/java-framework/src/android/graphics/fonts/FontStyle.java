package android.graphics.fonts;

/** A font's weight and slant. */
public final class FontStyle {
    public static final int FONT_SLANT_ITALIC = 1, FONT_SLANT_UPRIGHT = 0;
    public static final int FONT_WEIGHT_UNSPECIFIED = -1, FONT_WEIGHT_MIN = 1, FONT_WEIGHT_THIN = 100, FONT_WEIGHT_EXTRA_LIGHT = 200, FONT_WEIGHT_LIGHT = 300,
            FONT_WEIGHT_NORMAL = 400, FONT_WEIGHT_MEDIUM = 500, FONT_WEIGHT_SEMI_BOLD = 600, FONT_WEIGHT_BOLD = 700, FONT_WEIGHT_EXTRA_BOLD = 800,
            FONT_WEIGHT_BLACK = 900, FONT_WEIGHT_MAX = 1000;
    private final int mWeight, mSlant;
    public FontStyle() { this(FONT_WEIGHT_NORMAL, FONT_SLANT_UPRIGHT); }
    public FontStyle(int weight, int slant) { mWeight = weight; mSlant = slant; }
    public int getWeight() { return mWeight; }
    public int getSlant() { return mSlant; }
    /** How far apart two styles are, as the platform scores it: smaller is closer. */
    public int getMatchScore(FontStyle o) { return Math.abs(mWeight - o.mWeight) / 100 + (mSlant == o.mSlant ? 0 : 2); }
    @Override public boolean equals(Object o) { return o instanceof FontStyle && ((FontStyle) o).mWeight == mWeight && ((FontStyle) o).mSlant == mSlant; }
    @Override public int hashCode() { return mWeight * 31 + mSlant; }
    @Override public String toString() { return "FontStyle { weight=" + mWeight + ", slant=" + mSlant + "}"; }
}
