package android.graphics;
public class BlurMaskFilter extends MaskFilter {
    public enum Blur { NORMAL, SOLID, OUTER, INNER }
    public BlurMaskFilter(float radius, Blur style) {}
}
