package android.graphics;
public abstract class ColorSpace {
    public enum Named { SRGB, LINEAR_SRGB, EXTENDED_SRGB, LINEAR_EXTENDED_SRGB, BT709, BT2020, DCI_P3, DISPLAY_P3, NTSC_1953, SMPTE_C, ADOBE_RGB, PRO_PHOTO_RGB, ACES, ACESCG, CIE_XYZ, CIE_LAB, BT2020_HLG, BT2020_PQ }
    private static final ColorSpace SRGB = new ColorSpace() {};
    public static ColorSpace get(Named n) { return SRGB; }
    public String getName() { return "sRGB IEC61966-2.1"; }
    public boolean isSrgb() { return true; }
    public boolean isWideGamut() { return false; }
}
