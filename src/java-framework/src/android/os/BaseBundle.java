package android.os;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Set;

public class BaseBundle {
    final HashMap<String, Object> map;
    BaseBundle() { map = new HashMap<>(); }
    BaseBundle(BaseBundle b) { map = b == null ? new HashMap<String, Object>() : new HashMap<>(b.map); }
    public int size() { return map.size(); }
    public boolean isEmpty() { return map.isEmpty(); }
    public void clear() { map.clear(); }
    public boolean containsKey(String k) { return map.containsKey(k); }
    public Object get(String k) { return map.get(k); }
    public void remove(String k) { map.remove(k); }
    public Set<String> keySet() { return map.keySet(); }
    public void putAll(PersistableBundle b) { map.putAll(b.map); }
    private <T> T as(String k, Class<T> c) { Object v = map.get(k); return c.isInstance(v) ? c.cast(v) : null; }
    public void putBoolean(String k, boolean v) { map.put(k, v); }
    public boolean getBoolean(String k) { return getBoolean(k, false); }
    public boolean getBoolean(String k, boolean d) { Boolean v = as(k, Boolean.class); return v != null ? v : d; }
    public void putInt(String k, int v) { map.put(k, v); }
    public int getInt(String k) { return getInt(k, 0); }
    public int getInt(String k, int d) { Object v = map.get(k); return v instanceof Integer ? (Integer) v : d; }
    public void putLong(String k, long v) { map.put(k, v); }
    public long getLong(String k) { return getLong(k, 0L); }
    public long getLong(String k, long d) { Object v = map.get(k); return v instanceof Long ? (Long) v : d; }
    public void putDouble(String k, double v) { map.put(k, v); }
    public double getDouble(String k) { return getDouble(k, 0); }
    public double getDouble(String k, double d) { Object v = map.get(k); return v instanceof Double ? (Double) v : d; }
    public void putString(String k, String v) { map.put(k, v); }
    public String getString(String k) { Object v = map.get(k); return v instanceof String ? (String) v : null; }
    public String getString(String k, String d) { String v = getString(k); return v == null ? d : v; }
    public void putBooleanArray(String k, boolean[] v) { map.put(k, v); }
    public boolean[] getBooleanArray(String k) { return as(k, boolean[].class); }
    public void putIntArray(String k, int[] v) { map.put(k, v); }
    public int[] getIntArray(String k) { return as(k, int[].class); }
    public void putLongArray(String k, long[] v) { map.put(k, v); }
    public long[] getLongArray(String k) { return as(k, long[].class); }
    public void putDoubleArray(String k, double[] v) { map.put(k, v); }
    public double[] getDoubleArray(String k) { return as(k, double[].class); }
    public void putStringArray(String k, String[] v) { map.put(k, v); }
    public String[] getStringArray(String k) { return as(k, String[].class); }
}
