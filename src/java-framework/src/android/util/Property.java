package android.util;
public abstract class Property<T, V> {
    private final String mName; private final Class<V> mType;
    public Property(Class<V> type, String name) { mName = name; mType = type; }
    public static <T, V> Property<T, V> of(Class<T> host, Class<V> value, String name) { throw new UnsupportedOperationException(); }
    public boolean isReadOnly() { return false; }
    public void set(T object, V value) { throw new UnsupportedOperationException("Property " + mName + " is read-only"); }
    public abstract V get(T object);
    public String getName() { return mName; }
    public Class<V> getType() { return mType; }
}
