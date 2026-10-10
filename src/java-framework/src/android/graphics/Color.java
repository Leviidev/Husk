package android.graphics;
public class Color {
    public static final int BLACK = 0xFF000000, WHITE = 0xFFFFFFFF, TRANSPARENT = 0, RED = 0xFFFF0000, GREEN = 0xFF00FF00, BLUE = 0xFF0000FF, GRAY = 0xFF888888;
    public static int rgb(int r, int g, int b) { return 0xFF000000 | (r << 16) | (g << 8) | b; }
    public static int argb(int a, int r, int g, int b) { return (a << 24) | (r << 16) | (g << 8) | b; }
    public static int alpha(int c) { return c >>> 24; } public static int red(int c) { return (c >> 16) & 0xFF; } public static int green(int c) { return (c >> 8) & 0xFF; } public static int blue(int c) { return c & 0xFF; }
    public static int parseColor(String s) { if (s.startsWith("#")) { long v = Long.parseLong(s.substring(1), 16); if (s.length() == 7) v |= 0xFF000000L; return (int) v; } return BLACK; }
}
