package android.util;
public class SparseArray<E> implements Cloneable {
    private int[] k; private Object[] v; private int n;
    public SparseArray() { this(10); }
    public SparseArray(int cap) { k = new int[Math.max(cap, 1)]; v = new Object[k.length]; }
    private int find(int key) { return java.util.Arrays.binarySearch(k, 0, n, key); }
    @SuppressWarnings("unchecked") public E get(int key) { int i = find(key); return i < 0 ? null : (E) v[i]; }
    @SuppressWarnings("unchecked") public E get(int key, E def) { int i = find(key); return i < 0 ? def : (E) v[i]; }
    public void put(int key, E value) {
        int i = find(key);
        if (i >= 0) { v[i] = value; return; }
        i = ~i;
        if (n == k.length) { k = java.util.Arrays.copyOf(k, n * 2); v = java.util.Arrays.copyOf(v, n * 2); }
        System.arraycopy(k, i, k, i + 1, n - i); System.arraycopy(v, i, v, i + 1, n - i);
        k[i] = key; v[i] = value; n++;
    }
    public void append(int key, E value) { put(key, value); }
    public void set(int key, E value) { put(key, value); }
    public boolean contains(int key) { return find(key) >= 0; }
    public void delete(int key) { int i = find(key); if (i >= 0) removeAt(i); }
    public void remove(int key) { delete(key); }
    public E removeReturnOld(int key) { int i = find(key); if (i < 0) return null; E o = valueAt(i); removeAt(i); return o; }
    public void removeAt(int i) { System.arraycopy(k, i + 1, k, i, n - i - 1); System.arraycopy(v, i + 1, v, i, n - i - 1); n--; v[n] = null; }
    public void removeAtRange(int i, int size) { for (int j = 0; j < size && i < n; j++) removeAt(i); }
    public int size() { return n; }
    public int keyAt(int i) { return k[i]; }
    @SuppressWarnings("unchecked") public E valueAt(int i) { return (E) v[i]; }
    public void setValueAt(int i, E value) { v[i] = value; }
    public int indexOfKey(int key) { int i = find(key); return i < 0 ? -1 : i; }
    public int indexOfValue(E value) { for (int i = 0; i < n; i++) if (v[i] == value) return i; return -1; }
    public boolean contentEquals(SparseArray<?> o) { if (o == null || o.n != n) return false; for (int i = 0; i < n; i++) if (k[i] != o.k[i] || !java.util.Objects.equals(v[i], o.v[i])) return false; return true; }
    public void clear() { java.util.Arrays.fill(v, 0, n, null); n = 0; }
    @SuppressWarnings("unchecked") @Override public SparseArray<E> clone() { try { SparseArray<E> c = (SparseArray<E>) super.clone(); c.k = k.clone(); c.v = v.clone(); return c; } catch (CloneNotSupportedException e) { throw new AssertionError(e); } }
    public void putAll(SparseArray<? extends E> o) { for (int i = 0; i < o.size(); i++) put(o.keyAt(i), o.valueAt(i)); }
    @Override public String toString() { StringBuilder b = new StringBuilder("{"); for (int i = 0; i < n; i++) { if (i > 0) b.append(", "); b.append(k[i]).append('=').append(v[i]); } return b.append('}').toString(); }
}
