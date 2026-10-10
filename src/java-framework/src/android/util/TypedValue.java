package android.util;
public class TypedValue {
    public static final int TYPE_NULL = 0, TYPE_REFERENCE = 1, TYPE_ATTRIBUTE = 2, TYPE_STRING = 3, TYPE_FLOAT = 4, TYPE_DIMENSION = 5, TYPE_FRACTION = 6,
        TYPE_FIRST_INT = 0x10, TYPE_INT_DEC = 0x10, TYPE_INT_HEX = 0x11, TYPE_INT_BOOLEAN = 0x12, TYPE_FIRST_COLOR_INT = 0x1c, TYPE_INT_COLOR_ARGB8 = 0x1c,
        TYPE_INT_COLOR_RGB8 = 0x1d, TYPE_INT_COLOR_ARGB4 = 0x1e, TYPE_INT_COLOR_RGB4 = 0x1f, TYPE_LAST_COLOR_INT = 0x1f, TYPE_LAST_INT = 0x1f;
    public static final int COMPLEX_UNIT_SHIFT = 0, COMPLEX_UNIT_MASK = 0xf, COMPLEX_UNIT_PX = 0, COMPLEX_UNIT_DIP = 1, COMPLEX_UNIT_SP = 2,
        COMPLEX_UNIT_PT = 3, COMPLEX_UNIT_IN = 4, COMPLEX_UNIT_MM = 5, COMPLEX_UNIT_FRACTION = 0, COMPLEX_UNIT_FRACTION_PARENT = 1,
        COMPLEX_RADIX_SHIFT = 4, COMPLEX_RADIX_MASK = 3, COMPLEX_RADIX_23p0 = 0, COMPLEX_RADIX_16p7 = 1, COMPLEX_RADIX_8p15 = 2, COMPLEX_RADIX_0p23 = 3,
        COMPLEX_MANTISSA_SHIFT = 8, COMPLEX_MANTISSA_MASK = 0xffffff;
    public static final int DATA_NULL_UNDEFINED = 0, DATA_NULL_EMPTY = 1, DENSITY_DEFAULT = 0, DENSITY_NONE = 0xffff;
    public int type, data, assetCookie, resourceId, density, changingConfigurations, sourceResourceId;
    public CharSequence string;

    private static final float MANTISSA_MULT = 1.0f / (1 << COMPLEX_MANTISSA_SHIFT);
    private static final float[] RADIX_MULTS = { 1.0f * MANTISSA_MULT, 1.0f / (1 << 7) * MANTISSA_MULT, 1.0f / (1 << 15) * MANTISSA_MULT, 1.0f / (1 << 23) * MANTISSA_MULT };
    public static float complexToFloat(int c) { return (c & (COMPLEX_MANTISSA_MASK << COMPLEX_MANTISSA_SHIFT)) * RADIX_MULTS[(c >> COMPLEX_RADIX_SHIFT) & COMPLEX_RADIX_MASK]; }
    public static float applyDimension(int unit, float v, DisplayMetrics m) {
        switch (unit) {
        case COMPLEX_UNIT_PX: return v;
        case COMPLEX_UNIT_DIP: return v * m.density;
        case COMPLEX_UNIT_SP: return v * m.scaledDensity;
        case COMPLEX_UNIT_PT: return v * m.xdpi * (1.0f / 72);
        case COMPLEX_UNIT_IN: return v * m.xdpi;
        case COMPLEX_UNIT_MM: return v * m.xdpi * (1.0f / 25.4f);
        }
        return 0;
    }
    public static float deriveDimension(int unit, float px, DisplayMetrics m) { float one = applyDimension(unit, 1, m); return one == 0 ? 0 : px / one; }
    public static float convertPixelsToDimension(int unit, float px, DisplayMetrics m) { return deriveDimension(unit, px, m); }
    public static float convertDimensionToPixels(int unit, float v, DisplayMetrics m) { return applyDimension(unit, v, m); }
    public static float complexToDimension(int data, DisplayMetrics m) { return applyDimension((data >> COMPLEX_UNIT_SHIFT) & COMPLEX_UNIT_MASK, complexToFloat(data), m); }
    public static int complexToDimensionPixelOffset(int data, DisplayMetrics m) { return (int) complexToDimension(data, m); }
    public static int complexToDimensionPixelSize(int data, DisplayMetrics m) {
        float v = complexToFloat(data), f = applyDimension((data >> COMPLEX_UNIT_SHIFT) & COMPLEX_UNIT_MASK, v, m);
        int r = (int) (f >= 0 ? f + 0.5f : f - 0.5f);
        if (r != 0) return r; if (v == 0) return 0; return v > 0 ? 1 : -1;
    }
    public static float complexToFraction(int data, float base, float pbase) {
        switch ((data >> COMPLEX_UNIT_SHIFT) & COMPLEX_UNIT_MASK) { case COMPLEX_UNIT_FRACTION: return complexToFloat(data) * base; case COMPLEX_UNIT_FRACTION_PARENT: return complexToFloat(data) * pbase; }
        return 0;
    }
    public int getComplexUnit() { return COMPLEX_UNIT_MASK & (data >> COMPLEX_UNIT_SHIFT); }
    public float getDimension(DisplayMetrics m) { return complexToDimension(data, m); }
    public final float getFloat() { return Float.intBitsToFloat(data); }
    public float getFraction(float base, float pbase) { return complexToFraction(data, base, pbase); }
    public boolean isColorType() { return type >= TYPE_FIRST_COLOR_INT && type <= TYPE_LAST_COLOR_INT; }
    public final CharSequence coerceToString() { return string != null ? string : coerceToString(type, data); }
    public static final String coerceToString(int type, int data) {
        switch (type) {
        case TYPE_NULL: return null;
        case TYPE_REFERENCE: return "@" + data;
        case TYPE_ATTRIBUTE: return "?" + data;
        case TYPE_FLOAT: return Float.toString(Float.intBitsToFloat(data));
        case TYPE_DIMENSION: return complexToFloat(data) + new String[] { "px", "dip", "sp", "pt", "in", "mm" }[Math.min(5, (data >> COMPLEX_UNIT_SHIFT) & COMPLEX_UNIT_MASK)];
        case TYPE_FRACTION: return complexToFloat(data) * 100 + "%";
        case TYPE_INT_HEX: return "0x" + Integer.toHexString(data);
        case TYPE_INT_BOOLEAN: return data != 0 ? "true" : "false";
        }
        if (type >= TYPE_FIRST_COLOR_INT && type <= TYPE_LAST_COLOR_INT) return "#" + Integer.toHexString(data);
        if (type >= TYPE_FIRST_INT && type <= TYPE_LAST_INT) return Integer.toString(data);
        return null;
    }
    public void setTo(TypedValue o) { type = o.type; string = o.string; data = o.data; assetCookie = o.assetCookie; resourceId = o.resourceId; density = o.density; }
    @Override public String toString() { return "TypedValue{t=0x" + Integer.toHexString(type) + "/d=0x" + Integer.toHexString(data) + (string != null ? " \"" + string + "\"" : "") + "}"; }
}
