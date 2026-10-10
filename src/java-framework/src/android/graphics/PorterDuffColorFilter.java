package android.graphics;
public class PorterDuffColorFilter extends ColorFilter {
    public PorterDuffColorFilter(int c, PorterDuff.Mode m) { color = c; mode = m.nativeInt; }
    public int getColor() { return color; }
}
