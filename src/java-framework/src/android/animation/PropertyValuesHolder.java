package android.animation;

import android.util.Property;
import java.lang.reflect.Method;

public class PropertyValuesHolder implements Cloneable {
    String mName;
    Property mProperty;
    Object[] mValues;
    Class<?> mType;
    TypeEvaluator mEvaluator;
    Object mAnimated;
    private Method mSetter, mGetter;
    private Class<?> mBoundClass;

    private PropertyValuesHolder(String name) { mName = name; }
    private PropertyValuesHolder(Property p) { mProperty = p; if (p != null) mName = p.getName(); }
    public static PropertyValuesHolder ofFloat(String name, float... v) { PropertyValuesHolder h = new PropertyValuesHolder(name); h.setFloatValues(v); return h; }
    public static PropertyValuesHolder ofFloat(Property<?, Float> p, float... v) { PropertyValuesHolder h = new PropertyValuesHolder(p); h.setFloatValues(v); return h; }
    public static PropertyValuesHolder ofInt(String name, int... v) { PropertyValuesHolder h = new PropertyValuesHolder(name); h.setIntValues(v); return h; }
    public static PropertyValuesHolder ofInt(Property<?, Integer> p, int... v) { PropertyValuesHolder h = new PropertyValuesHolder(p); h.setIntValues(v); return h; }
    public static PropertyValuesHolder ofObject(String name, TypeEvaluator e, Object... v) { PropertyValuesHolder h = new PropertyValuesHolder(name); h.setObjectValues(v); h.mEvaluator = e; return h; }
    public static <V> PropertyValuesHolder ofObject(Property p, TypeEvaluator<V> e, V... v) { PropertyValuesHolder h = new PropertyValuesHolder(p); h.setObjectValues(v); h.mEvaluator = e; return h; }
    public static PropertyValuesHolder ofKeyframe(String name, Keyframe... k) {
        PropertyValuesHolder h = new PropertyValuesHolder(name);
        Object[] v = new Object[k.length]; for (int i = 0; i < k.length; i++) v[i] = k[i].getValue();
        h.mValues = v; h.mType = k.length > 0 && k[0].getValue() instanceof Integer ? int.class : float.class;
        return h;
    }
    public static PropertyValuesHolder ofKeyframe(Property p, Keyframe... k) { PropertyValuesHolder h = ofKeyframe(p.getName(), k); h.mProperty = p; return h; }
    public static PropertyValuesHolder ofMultiFloat(String name, float[][] v) { return ofFloat(name, v.length > 0 ? v[0] : new float[0]); }
    public void setFloatValues(float... v) { mType = float.class; mValues = new Object[v.length]; for (int i = 0; i < v.length; i++) mValues[i] = v[i]; }
    public void setIntValues(int... v) { mType = int.class; mValues = new Object[v.length]; for (int i = 0; i < v.length; i++) mValues[i] = v[i]; }
    public void setObjectValues(Object... v) { mType = v.length > 0 && v[0] != null ? v[0].getClass() : Object.class; mValues = v; }
    public void setEvaluator(TypeEvaluator e) { mEvaluator = e; }
    public void setPropertyName(String n) { mName = n; }
    public String getPropertyName() { return mName; }
    public void setProperty(Property p) { mProperty = p; }
    public Object getAnimatedValue() { return mAnimated; }

    void calculate(float f) {
        if (mValues == null || mValues.length == 0) return;
        if (mValues.length == 1) { mAnimated = mValues[0]; return; }
        float scaled = f * (mValues.length - 1);
        int i = Math.max(0, Math.min(mValues.length - 2, (int) Math.floor(scaled)));
        float local = scaled - i;
        Object a = mValues[i], b = mValues[i + 1];
        if (mEvaluator != null) mAnimated = mEvaluator.evaluate(local, a, b);
        else if (a instanceof Float) mAnimated = (Float) a + local * ((Float) b - (Float) a);
        else if (a instanceof Integer) mAnimated = (int) ((Integer) a + local * ((Integer) b - (Integer) a));
        else mAnimated = local < 1 ? a : b;
    }
    private static String setterName(String prefix, String n) { return prefix + Character.toUpperCase(n.charAt(0)) + n.substring(1); }
    private Method find(Class<?> c, String name, Class<?>... args) {
        for (Class<?> k = c; k != null; k = k.getSuperclass()) {
            try { Method m = k.getDeclaredMethod(name, args); m.setAccessible(true); return m; } catch (NoSuchMethodException e) {}
        }
        try { return c.getMethod(name, args); } catch (NoSuchMethodException e) { return null; }
    }
    private void bind(Object target) {
        if (mBoundClass == target.getClass() || mProperty != null || mName == null) return;
        mBoundClass = target.getClass();
        Class<?> t = mType == Float.class ? float.class : mType == Integer.class ? int.class : mType;
        mSetter = find(mBoundClass, setterName("set", mName), t);
        if (mSetter == null && t == float.class) mSetter = find(mBoundClass, setterName("set", mName), Float.class);
        if (mSetter == null && t == int.class) mSetter = find(mBoundClass, setterName("set", mName), Integer.class);
        if (mSetter == null) mSetter = find(mBoundClass, setterName("set", mName), Object.class);
        mGetter = find(mBoundClass, setterName("get", mName));
    }
    @SuppressWarnings("unchecked")
    void setupStart(Object target) {
        if (target == null || mValues == null || mValues.length != 1) return;
        Object start = null;
        try {
            if (mProperty != null) start = mProperty.get(target);
            else { bind(target); if (mGetter != null) start = mGetter.invoke(target); }
        } catch (Exception e) {}
        if (start != null) mValues = new Object[] { start, mValues[0] };
    }
    @SuppressWarnings("unchecked")
    void apply(Object target) {
        if (target == null || mAnimated == null) return;
        try {
            if (mProperty != null) mProperty.set(target, mAnimated);
            else { bind(target); if (mSetter != null) mSetter.invoke(target, mAnimated); }
        } catch (Exception e) { android.util.Log.w("PropertyValuesHolder", "cannot set " + mName + " on " + target.getClass().getName(), e); }
    }
    @Override public PropertyValuesHolder clone() { try { PropertyValuesHolder h = (PropertyValuesHolder) super.clone(); h.mValues = mValues == null ? null : mValues.clone(); return h; } catch (CloneNotSupportedException e) { throw new AssertionError(); } }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public static android.animation.PropertyValuesHolder ofMultiFloat(java.lang.String p0, android.animation.TypeConverter p1, android.animation.TypeEvaluator p2, android.animation.Keyframe[] p3) { return null; }
    public static android.animation.PropertyValuesHolder ofMultiFloat(java.lang.String p0, android.animation.TypeConverter p1, android.animation.TypeEvaluator p2, java.lang.Object[] p3) { return null; }
    public static android.animation.PropertyValuesHolder ofMultiFloat(java.lang.String p0, android.graphics.Path p1) { return null; }
    public static android.animation.PropertyValuesHolder ofMultiInt(java.lang.String p0, android.animation.TypeConverter p1, android.animation.TypeEvaluator p2, android.animation.Keyframe[] p3) { return null; }
    public static android.animation.PropertyValuesHolder ofMultiInt(java.lang.String p0, android.animation.TypeConverter p1, android.animation.TypeEvaluator p2, java.lang.Object[] p3) { return null; }
    public static android.animation.PropertyValuesHolder ofMultiInt(java.lang.String p0, android.graphics.Path p1) { return null; }
    public static android.animation.PropertyValuesHolder ofMultiInt(java.lang.String p0, int[][] p1) { return null; }
    public static android.animation.PropertyValuesHolder ofObject(android.util.Property p0, android.animation.TypeConverter p1, android.animation.TypeEvaluator p2, java.lang.Object[] p3) { return null; }
    public static android.animation.PropertyValuesHolder ofObject(android.util.Property p0, android.animation.TypeConverter p1, android.graphics.Path p2) { return null; }
    public static android.animation.PropertyValuesHolder ofObject(java.lang.String p0, android.animation.TypeConverter p1, android.graphics.Path p2) { return null; }
    public java.lang.Class getValueType() { return null; }
    public void setConverter(android.animation.TypeConverter p0) {}
    public void setKeyframes(android.animation.Keyframe[] p0) {}
    // ---- end of generated members
}
