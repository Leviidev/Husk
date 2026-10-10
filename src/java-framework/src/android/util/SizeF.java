package android.util;
public final class SizeF {
    private final float w, h;
    public SizeF(float w, float h) { this.w = w; this.h = h; }
    public float getWidth() { return w; } public float getHeight() { return h; }
    @Override public boolean equals(Object o) { return o instanceof SizeF && ((SizeF) o).w == w && ((SizeF) o).h == h; }
    @Override public int hashCode() { return Float.floatToIntBits(w) ^ Float.floatToIntBits(h); }
    @Override public String toString() { return w + "x" + h; }
}
