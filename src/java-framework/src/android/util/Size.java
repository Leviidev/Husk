package android.util;
public final class Size {
    private final int w, h;
    public Size(int w, int h) { this.w = w; this.h = h; }
    public int getWidth() { return w; } public int getHeight() { return h; }
    @Override public boolean equals(Object o) { return o instanceof Size && ((Size) o).w == w && ((Size) o).h == h; }
    @Override public int hashCode() { return h ^ ((w << 16) | (w >>> 16)); }
    @Override public String toString() { return w + "x" + h; }
    public static Size parseSize(String s) { int i = s.indexOf('x'); if (i < 0) i = s.indexOf('*'); return new Size(Integer.parseInt(s.substring(0, i)), Integer.parseInt(s.substring(i + 1))); }
}
