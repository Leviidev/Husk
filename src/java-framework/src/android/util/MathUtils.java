package android.util;
public final class MathUtils {
    public static float constrain(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
    public static int constrain(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
    public static float lerp(float a, float b, float t) { return a + (b - a) * t; }
}
