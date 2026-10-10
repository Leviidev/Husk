package android.graphics;

import java.util.HashMap;
import java.util.Locale;

public class Color {
    public static final int BLACK = 0xFF000000, DKGRAY = 0xFF444444, GRAY = 0xFF888888, LTGRAY = 0xFFCCCCCC, WHITE = 0xFFFFFFFF, RED = 0xFFFF0000,
        GREEN = 0xFF00FF00, BLUE = 0xFF0000FF, YELLOW = 0xFFFFFF00, CYAN = 0xFF00FFFF, MAGENTA = 0xFFFF00FF, TRANSPARENT = 0;
    private final float r, g, b, a;
    public Color() { r = g = b = 0; a = 1; }
    private Color(float r, float g, float b, float a) { this.r = r; this.g = g; this.b = b; this.a = a; }
    public static Color valueOf(int c) { return new Color(red(c) / 255f, green(c) / 255f, blue(c) / 255f, alpha(c) / 255f); }
    public static Color valueOf(float r, float g, float b) { return new Color(r, g, b, 1); }
    public static Color valueOf(float r, float g, float b, float a) { return new Color(r, g, b, a); }
    public static Color valueOf(long c) { return valueOf(toArgb(c)); }
    public float red() { return r; } public float green() { return g; } public float blue() { return b; } public float alpha() { return a; }
    public int toArgb() { return argb(Math.round(a * 255), Math.round(r * 255), Math.round(g * 255), Math.round(b * 255)); }
    public long pack() { return pack(toArgb()); }
    public float luminance() { return luminance(toArgb()); }

    public static int alpha(int c) { return c >>> 24; }
    public static int red(int c) { return (c >> 16) & 0xFF; }
    public static int green(int c) { return (c >> 8) & 0xFF; }
    public static int blue(int c) { return c & 0xFF; }
    public static int rgb(int r, int g, int b) { return 0xFF000000 | (r << 16) | (g << 8) | b; }
    public static int rgb(float r, float g, float b) { return rgb((int) (r * 255 + 0.5f), (int) (g * 255 + 0.5f), (int) (b * 255 + 0.5f)); }
    public static int argb(int a, int r, int g, int b) { return (a << 24) | (r << 16) | (g << 8) | b; }
    public static int argb(float a, float r, float g, float b) { return argb((int) (a * 255 + 0.5f), (int) (r * 255 + 0.5f), (int) (g * 255 + 0.5f), (int) (b * 255 + 0.5f)); }
    /** Packed long colours (sRGB only): the int in the high 32 bits, as Color.pack makes them. */
    public static long pack(int c) { return ((long) c) << 32; }
    public static long pack(float r, float g, float b) { return pack(rgb(r, g, b)); }
    public static long pack(float r, float g, float b, float a) { return pack(argb(a, r, g, b)); }
    public static int toArgb(long c) { return (c & 0x3fL) == 0 ? (int) (c >> 32) : 0xFF000000; }
    public static float red(long c) { return red(toArgb(c)) / 255f; }
    public static float green(long c) { return green(toArgb(c)) / 255f; }
    public static float blue(long c) { return blue(toArgb(c)) / 255f; }
    public static float alpha(long c) { return alpha(toArgb(c)) / 255f; }
    public static boolean isSrgb(long c) { return true; }
    public static boolean isWideGamut(long c) { return false; }
    static int[] huskInts(long[] c) { int[] o = new int[c.length]; for (int i = 0; i < c.length; i++) o[i] = toArgb(c[i]); return o; }
    public static float luminance(int c) {
        double r = lin(red(c) / 255.0), g = lin(green(c) / 255.0), b = lin(blue(c) / 255.0);
        return (float) (0.2126 * r + 0.7152 * g + 0.0722 * b);
    }
    private static double lin(double v) { return v <= 0.04045 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4); }

    private static final HashMap<String, Integer> sNames = new HashMap<>();
    static {
        sNames.put("black", BLACK); sNames.put("darkgray", DKGRAY); sNames.put("gray", GRAY); sNames.put("lightgray", LTGRAY); sNames.put("white", WHITE);
        sNames.put("red", RED); sNames.put("green", GREEN); sNames.put("blue", BLUE); sNames.put("yellow", YELLOW); sNames.put("cyan", CYAN);
        sNames.put("magenta", MAGENTA); sNames.put("aqua", 0xFF00FFFF); sNames.put("fuchsia", 0xFFFF00FF); sNames.put("darkgrey", DKGRAY);
        sNames.put("grey", GRAY); sNames.put("lightgrey", LTGRAY); sNames.put("lime", 0xFF00FF00); sNames.put("maroon", 0xFF800000);
        sNames.put("navy", 0xFF000080); sNames.put("olive", 0xFF808000); sNames.put("purple", 0xFF800080); sNames.put("silver", 0xFFC0C0C0);
        sNames.put("teal", 0xFF008080);
    }
    public static int parseColor(String s) {
        if (s.length() > 0 && s.charAt(0) == '#') {
            long v = Long.parseLong(s.substring(1), 16);
            if (s.length() == 7) v |= 0xFF000000L;
            else if (s.length() != 9) throw new IllegalArgumentException("Unknown color");
            return (int) v;
        }
        Integer c = sNames.get(s.toLowerCase(Locale.ROOT));
        if (c != null) return c;
        throw new IllegalArgumentException("Unknown color");
    }
    public static void RGBToHSV(int r, int g, int b, float[] hsv) {
        float rf = r / 255f, gf = g / 255f, bf = b / 255f, max = Math.max(rf, Math.max(gf, bf)), min = Math.min(rf, Math.min(gf, bf)), d = max - min;
        float h = 0;
        if (d != 0) {
            if (max == rf) h = 60 * (((gf - bf) / d) % 6);
            else if (max == gf) h = 60 * ((bf - rf) / d + 2);
            else h = 60 * ((rf - gf) / d + 4);
        }
        if (h < 0) h += 360;
        hsv[0] = h; hsv[1] = max == 0 ? 0 : d / max; hsv[2] = max;
    }
    public static void colorToHSV(int c, float[] hsv) { RGBToHSV(red(c), green(c), blue(c), hsv); }
    public static int HSVToColor(float[] hsv) { return HSVToColor(0xFF, hsv); }
    public static int HSVToColor(int alpha, float[] hsv) {
        float h = ((hsv[0] % 360) + 360) % 360, s = Math.max(0, Math.min(1, hsv[1])), v = Math.max(0, Math.min(1, hsv[2]));
        float c = v * s, x = c * (1 - Math.abs((h / 60) % 2 - 1)), m = v - c, r, g, b;
        if (h < 60) { r = c; g = x; b = 0; } else if (h < 120) { r = x; g = c; b = 0; } else if (h < 180) { r = 0; g = c; b = x; }
        else if (h < 240) { r = 0; g = x; b = c; } else if (h < 300) { r = x; g = 0; b = c; } else { r = c; g = 0; b = x; }
        return argb(alpha, Math.round((r + m) * 255), Math.round((g + m) * 255), Math.round((b + m) * 255));
    }
}
