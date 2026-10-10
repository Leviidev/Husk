package android.content.res;

import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.util.TypedValue;

/** Resolved attribute values, by index into the attribute array asked for. */
public class TypedArray implements AutoCloseable {
    private final Resources mRes;
    private final Resources.Theme mTheme;
    private final TypedValue[] mValues;
    private int[] mIndices = new int[4];
    private int mIndexCount;
    XmlBlock.Parser mXml;
    private boolean mRecycled;

    TypedArray(Resources r, Resources.Theme t, int n) { mRes = r; mTheme = t; mValues = new TypedValue[n]; }
    void set(int i, TypedValue v) {
        TypedValue c = new TypedValue();
        c.setTo(v);
        c.resourceId = v.resourceId;
        c.changingConfigurations = v.changingConfigurations;
        if (mValues[i] == null) { if (mIndexCount == mIndices.length) mIndices = java.util.Arrays.copyOf(mIndices, mIndexCount * 2); mIndices[mIndexCount++] = i; }
        mValues[i] = c;
    }
    public int length() { return mValues.length; }
    public int getIndexCount() { return mIndexCount; }
    public int getIndex(int at) { return mIndices[at]; }
    public Resources getResources() { return mRes; }
    public int getChangingConfigurations() { return 0; }
    public void recycle() { mRecycled = true; }
    public void close() { recycle(); }
    public String getPositionDescription() { return mXml != null ? mXml.getPositionDescription() : "<internal>"; }
    private TypedValue v(int i) { return i >= 0 && i < mValues.length ? mValues[i] : null; }
    public boolean hasValue(int i) { TypedValue t = v(i); return t != null && t.type != TypedValue.TYPE_NULL; }
    public boolean hasValueOrEmpty(int i) { return v(i) != null; }
    public TypedValue peekValue(int i) { TypedValue t = v(i); return t != null && t.type != TypedValue.TYPE_NULL ? t : null; }
    public boolean getValue(int i, TypedValue out) { TypedValue t = v(i); if (t == null) return false; out.setTo(t); out.resourceId = t.resourceId; return true; }
    public int getType(int i) { TypedValue t = v(i); return t == null ? TypedValue.TYPE_NULL : t.type; }
    private static boolean isInt(TypedValue t) { return t.type >= TypedValue.TYPE_FIRST_INT && t.type <= TypedValue.TYPE_LAST_INT; }

    public CharSequence getText(int i) {
        TypedValue t = v(i);
        if (t == null || t.type == TypedValue.TYPE_NULL) return null;
        if (t.type == TypedValue.TYPE_STRING) return t.string;
        if (t.type == TypedValue.TYPE_REFERENCE && t.resourceId != 0) { try { return mRes.getText(t.resourceId); } catch (Resources.NotFoundException e) { return null; } }
        return t.coerceToString();
    }
    public String getString(int i) { CharSequence s = getText(i); return s == null ? null : s.toString(); }
    public String getNonResourceString(int i) { TypedValue t = v(i); return t != null && t.type == TypedValue.TYPE_STRING && t.resourceId == 0 ? String.valueOf(t.string) : null; }
    public String getNonConfigurationString(int i, int allowed) { return getString(i); }
    public CharSequence[] getTextArray(int i) { TypedValue t = v(i); if (t == null || t.resourceId == 0) return null; try { return mRes.getTextArray(t.resourceId); } catch (Resources.NotFoundException e) { return null; } }
    public boolean getBoolean(int i, boolean def) {
        TypedValue t = v(i);
        if (t == null || t.type == TypedValue.TYPE_NULL) return def;
        if (isInt(t)) return t.data != 0;
        if (t.type == TypedValue.TYPE_STRING) return Boolean.parseBoolean(String.valueOf(t.string));
        return def;
    }
    public int getInt(int i, int def) {
        TypedValue t = v(i);
        if (t == null || t.type == TypedValue.TYPE_NULL) return def;
        if (isInt(t)) return t.data;
        if (t.type == TypedValue.TYPE_FLOAT) return (int) t.getFloat();
        if (t.type == TypedValue.TYPE_STRING) { try { return Integer.decode(String.valueOf(t.string)); } catch (NumberFormatException e) { return def; } }
        return def;
    }
    public int getInteger(int i, int def) { return getInt(i, def); }
    public float getFloat(int i, float def) {
        TypedValue t = v(i);
        if (t == null || t.type == TypedValue.TYPE_NULL) return def;
        if (t.type == TypedValue.TYPE_FLOAT) return t.getFloat();
        if (isInt(t)) return t.data;
        if (t.type == TypedValue.TYPE_DIMENSION) return TypedValue.complexToFloat(t.data);
        if (t.type == TypedValue.TYPE_STRING) { try { return Float.parseFloat(String.valueOf(t.string)); } catch (NumberFormatException e) { return def; } }
        return def;
    }
    public int getColor(int i, int def) {
        TypedValue t = v(i);
        if (t == null || t.type == TypedValue.TYPE_NULL) return def;
        if (isInt(t)) return t.data;
        if (t.type == TypedValue.TYPE_STRING) { ColorStateList c = mRes.loadColorStateList(t, t.resourceId, mTheme); return c.getDefaultColor(); }
        throw new UnsupportedOperationException("Can't convert value at index " + i + " to color: type=0x" + Integer.toHexString(t.type));
    }
    public ColorStateList getColorStateList(int i) {
        TypedValue t = v(i);
        if (t == null || t.type == TypedValue.TYPE_NULL) return null;
        if (isInt(t)) return ColorStateList.valueOf(t.data);
        if (t.type == TypedValue.TYPE_STRING) return mRes.loadColorStateList(t, t.resourceId, mTheme);
        return null;
    }
    public float getDimension(int i, float def) {
        TypedValue t = v(i);
        if (t == null || t.type == TypedValue.TYPE_NULL) return def;
        if (t.type == TypedValue.TYPE_DIMENSION) return TypedValue.complexToDimension(t.data, mRes.getDisplayMetrics());
        if (isInt(t)) return t.data;
        throw new UnsupportedOperationException("Can't convert value at index " + i + " to dimension: type=0x" + Integer.toHexString(t.type));
    }
    public int getDimensionPixelOffset(int i, int def) {
        TypedValue t = v(i);
        if (t == null || t.type == TypedValue.TYPE_NULL) return def;
        if (t.type == TypedValue.TYPE_DIMENSION) return TypedValue.complexToDimensionPixelOffset(t.data, mRes.getDisplayMetrics());
        if (isInt(t)) return t.data;
        throw new UnsupportedOperationException("Can't convert value at index " + i + " to dimension: type=0x" + Integer.toHexString(t.type));
    }
    public int getDimensionPixelSize(int i, int def) {
        TypedValue t = v(i);
        if (t == null || t.type == TypedValue.TYPE_NULL) return def;
        if (t.type == TypedValue.TYPE_DIMENSION) return TypedValue.complexToDimensionPixelSize(t.data, mRes.getDisplayMetrics());
        if (isInt(t)) return t.data;
        throw new UnsupportedOperationException("Can't convert value at index " + i + " to dimension: type=0x" + Integer.toHexString(t.type));
    }
    public int getLayoutDimension(int i, String name) {
        TypedValue t = v(i);
        if (t != null) {
            if (isInt(t)) return t.data;
            if (t.type == TypedValue.TYPE_DIMENSION) return TypedValue.complexToDimensionPixelSize(t.data, mRes.getDisplayMetrics());
        }
        throw new UnsupportedOperationException(getPositionDescription() + ": You must supply a " + name + " attribute.");
    }
    public int getLayoutDimension(int i, int def) {
        TypedValue t = v(i);
        if (t == null || t.type == TypedValue.TYPE_NULL) return def;
        if (isInt(t)) return t.data;
        if (t.type == TypedValue.TYPE_DIMENSION) return TypedValue.complexToDimensionPixelSize(t.data, mRes.getDisplayMetrics());
        return def;
    }
    public float getFraction(int i, int base, int pbase, float def) {
        TypedValue t = v(i);
        if (t == null || t.type != TypedValue.TYPE_FRACTION) return def;
        return TypedValue.complexToFraction(t.data, base, pbase);
    }
    public int getResourceId(int i, int def) {
        TypedValue t = v(i);
        if (t == null) return def;
        if (t.resourceId != 0) return t.resourceId;
        if (t.type == TypedValue.TYPE_REFERENCE && t.data != 0) return t.data;
        return def;
    }
    public int getSourceResourceId(int i, int def) { return getResourceId(i, def); }
    public int getThemeAttributeId(int i, int def) { TypedValue t = v(i); return t != null && t.type == TypedValue.TYPE_ATTRIBUTE ? t.data : def; }
    public Drawable getDrawable(int i) {
        TypedValue t = v(i);
        if (t == null || t.type == TypedValue.TYPE_NULL) return null;
        if (t.type == TypedValue.TYPE_REFERENCE && t.resourceId != 0) {
            TypedValue r = new TypedValue();
            if (mRes.huskValue(t.resourceId, r, true) && r.type != TypedValue.TYPE_NULL) return mRes.loadDrawable(r, t.resourceId, mTheme);
            return null;
        }
        return mRes.loadDrawable(t, t.resourceId, mTheme);
    }
    public Drawable getDrawableForDensity(int i, int density) { return getDrawable(i); }
    public Typeface getFont(int i) {
        TypedValue t = v(i);
        if (t == null || t.type == TypedValue.TYPE_NULL) return null;
        if (t.resourceId != 0) { try { return mRes.getFont(t.resourceId); } catch (Exception e) { return null; } }
        if (t.type == TypedValue.TYPE_STRING) return Typeface.create(String.valueOf(t.string), Typeface.NORMAL);
        return null;
    }
    @Override public String toString() { return "TypedArray" + java.util.Arrays.toString(mValues); }
}
