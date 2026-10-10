package android.graphics;
/** Approximated: a multiply by the colour, the addition left out. */
public class LightingColorFilter extends ColorFilter {
    public LightingColorFilter(int mul, int add) { color = 0xFF000000 | mul; mode = 13; }
}
