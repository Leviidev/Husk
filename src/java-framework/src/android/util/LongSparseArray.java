package android.util;
public class LongSparseArray<E> implements Cloneable {
    private long[] k; private Object[] v; private int n;
    public LongSparseArray() { this(10); }
    public LongSparseArray(int cap) { k = new long[Math.max(cap, 1)]; v = new Object[k.length]; }
    private int find(long key) { return java.util.Arrays.binarySearch(k, 0, n, key); }
    @SuppressWarnings("unchecked") public E get(long key) { int i = find(key); return i < 0 ? null : (E) v[i]; }
    @SuppressWarnings("unchecked") public E get(long key, E def) { int i = find(key); return i < 0 ? def : (E) v[i]; }
    public void put(long key, E value) {
        int i = find(key);
        if (i >= 0) { v[i] = value; return; }
        i = ~i;
        if (n == k.length) { k = java.util.Arrays.copyOf(k, n * 2); v = java.util.Arrays.copyOf(v, n * 2); }
        System.arraycopy(k, i, k, i + 1, n - i); System.arraycopy(v, i, v, i + 1, n - i);
        k[i] = key; v[i] = value; n++;
    }
    public void append(long key, E value) { put(key, value); }
    public void delete(long key) { int i = find(key); if (i >= 0) removeAt(i); }
    public void remove(long key) { delete(key); }
    public void removeAt(int i) { System.arraycopy(k, i + 1, k, i, n - i - 1); System.arraycopy(v, i + 1, v, i, n - i - 1); n--; v[n] = null; }
    public int size() { return n; }
    public long keyAt(int i) { return k[i]; }
    @SuppressWarnings("unchecked") public E valueAt(int i) { return (E) v[i]; }
    public void setValueAt(int i, E value) { v[i] = value; }
    public int indexOfKey(long key) { int i = find(key); return i < 0 ? -1 : i; }
    public int indexOfValue(E value) { for (int i = 0; i < n; i++) if (v[i] == value) return i; return -1; }
    public void clear() { java.util.Arrays.fill(v, 0, n, null); n = 0; }
    @SuppressWarnings("unchecked") @Override public LongSparseArray<E> clone() { try { LongSparseArray<E> c = (LongSparseArray<E>) super.clone(); c.k = k.clone(); c.v = v.clone(); return c; } catch (CloneNotSupportedException e) { throw new AssertionError(e); } }
}
