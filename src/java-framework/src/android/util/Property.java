package android.util;

import java.lang.reflect.Field;
import java.lang.reflect.Method;

/** A named value of a class (an animator's target): a getter and setter pair, or a field. */
public abstract class Property<T, V> {
    private final String mName; private final Class<V> mType;
    public Property(Class<V> type, String name) { mName = name; mType = type; }
    /** The property by reflection, as Android's ReflectiveProperty finds it: getName()/isName() and setName(), else a public field. */
    public static <T, V> Property<T, V> of(Class<T> host, Class<V> value, String name) { return new Reflective<T, V>(host, value, name); }
    public boolean isReadOnly() { return false; }
    public void set(T object, V value) { throw new UnsupportedOperationException("Property " + mName + " is read-only"); }
    public abstract V get(T object);
    public String getName() { return mName; }
    public Class<V> getType() { return mType; }

    private static final class Reflective<T, V> extends Property<T, V> {
        private Method mGetter, mSetter; private Field mField;
        Reflective(Class<T> host, Class<V> value, String name) {
            super(value, name);
            String cap = name.isEmpty() ? name : Character.toUpperCase(name.charAt(0)) + name.substring(1);
            for (String g : new String[] { "get" + cap, "is" + cap, name }) {
                try { mGetter = host.getMethod(g); break; } catch (NoSuchMethodException e) {}
            }
            if (mGetter != null && !box(mGetter.getReturnType()).isAssignableFrom(box(value)) && !box(value).isAssignableFrom(box(mGetter.getReturnType())))
                throw new NoSuchPropertyException("Underlying type (" + mGetter.getReturnType() + ") does not match Property type (" + value + ")");
            try { mSetter = host.getMethod("set" + cap, mGetter != null ? mGetter.getReturnType() : value); } catch (NoSuchMethodException e) {}
            if (mGetter == null) {
                try { mField = host.getField(name); } catch (NoSuchFieldException e) { throw new NoSuchPropertyException("No accessor method or field found for property with name " + name); }
            }
        }
        private static Class<?> box(Class<?> c) {
            if (!c.isPrimitive()) return c;
            if (c == int.class) return Integer.class; if (c == float.class) return Float.class; if (c == boolean.class) return Boolean.class;
            if (c == long.class) return Long.class; if (c == double.class) return Double.class; if (c == short.class) return Short.class;
            if (c == byte.class) return Byte.class; if (c == char.class) return Character.class; return c;
        }
        @SuppressWarnings("unchecked") @Override public V get(T object) {
            try { return (V) (mGetter != null ? mGetter.invoke(object) : mField.get(object)); }
            catch (Exception e) { throw new AssertionError(e); }
        }
        @Override public void set(T object, V value) {
            try {
                if (mSetter != null) mSetter.invoke(object, value);
                else if (mField != null) mField.set(object, value);
                else throw new UnsupportedOperationException("Property " + getName() + " is read-only");
            } catch (UnsupportedOperationException e) { throw e; } catch (Exception e) { throw new AssertionError(e); }
        }
        @Override public boolean isReadOnly() { return mSetter == null && mField == null; }
    }
}
