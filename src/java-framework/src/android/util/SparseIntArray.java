package android.util;
public class SparseIntArray implements Cloneable {
    private int[] k, v; private int n;
    public SparseIntArray() { this(10); }
    public SparseIntArray(int cap) { k = new int[Math.max(cap, 1)]; v = new int[k.length]; }
    private int find(int key) { return java.util.Arrays.binarySearch(k, 0, n, key); }
    public int get(int key) { return get(key, 0); }
    public int get(int key, int def) { int i = find(key); return i < 0 ? def : v[i]; }
    public void put(int key, int value) {
        int i = find(key);
        if (i >= 0) { v[i] = value; return; }
        i = ~i;
        if (n == k.length) { k = java.util.Arrays.copyOf(k, n * 2); v = java.util.Arrays.copyOf(v, n * 2); }
        System.arraycopy(k, i, k, i + 1, n - i); System.arraycopy(v, i, v, i + 1, n - i);
        k[i] = key; v[i] = value; n++;
    }
    public void append(int key, int value) { put(key, value); }
    public void delete(int key) { int i = find(key); if (i >= 0) removeAt(i); }
    public void removeAt(int i) { System.arraycopy(k, i + 1, k, i, n - i - 1); System.arraycopy(v, i + 1, v, i, n - i - 1); n--; }
    public int size() { return n; }
    public int keyAt(int i) { return k[i]; }
    public int valueAt(int i) { return v[i]; }
    public void setValueAt(int i, int value) { v[i] = value; }
    public int indexOfKey(int key) { int i = find(key); return i < 0 ? -1 : i; }
    public int indexOfValue(int value) { for (int i = 0; i < n; i++) if (v[i] == value) return i; return -1; }
    public void clear() { n = 0; }
    public int[] copyKeys() { return java.util.Arrays.copyOf(k, n); }
    @Override public SparseIntArray clone() { try { SparseIntArray c = (SparseIntArray) super.clone(); c.k = k.clone(); c.v = v.clone(); return c; } catch (CloneNotSupportedException e) { throw new AssertionError(e); } }
}
