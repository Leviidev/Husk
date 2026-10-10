package android.graphics;
public class BlendModeColorFilter extends ColorFilter {
    public BlendModeColorFilter(int c, BlendMode m) { color = c; mode = m.n; }
    public int getColor() { return color; }
}
