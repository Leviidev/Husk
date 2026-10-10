package android.util;
public class LruCache<K, V> {
    private final java.util.LinkedHashMap<K, V> map = new java.util.LinkedHashMap<>(0, 0.75f, true);
    private int size, maxSize, putCount, createCount, evictionCount, hitCount, missCount;
    public LruCache(int maxSize) { if (maxSize <= 0) throw new IllegalArgumentException("maxSize <= 0"); this.maxSize = maxSize; }
    public void resize(int m) { synchronized (this) { maxSize = m; } trimToSize(m); }
    public final V get(K key) {
        V v;
        synchronized (this) { v = map.get(key); if (v != null) { hitCount++; return v; } missCount++; }
        V c = create(key);
        if (c == null) return null;
        synchronized (this) { createCount++; v = map.put(key, c); if (v != null) map.put(key, v); else size += safeSizeOf(key, c); }
        if (v != null) { entryRemoved(false, key, c, v); return v; }
        trimToSize(maxSize);
        return c;
    }
    public final V put(K key, V value) {
        V prev;
        synchronized (this) { putCount++; size += safeSizeOf(key, value); prev = map.put(key, value); if (prev != null) size -= safeSizeOf(key, prev); }
        if (prev != null) entryRemoved(false, key, prev, value);
        trimToSize(maxSize);
        return prev;
    }
    public void trimToSize(int max) {
        while (true) {
            K key; V value;
            synchronized (this) {
                if (size <= max || map.isEmpty()) break;
                java.util.Map.Entry<K, V> e = map.entrySet().iterator().next();
                key = e.getKey(); value = e.getValue(); map.remove(key); size -= safeSizeOf(key, value); evictionCount++;
            }
            entryRemoved(true, key, value, null);
        }
    }
    public final V remove(K key) { V p; synchronized (this) { p = map.remove(key); if (p != null) size -= safeSizeOf(key, p); } if (p != null) entryRemoved(false, key, p, null); return p; }
    protected void entryRemoved(boolean evicted, K key, V oldValue, V newValue) {}
    protected V create(K key) { return null; }
    private int safeSizeOf(K key, V value) { int r = sizeOf(key, value); if (r < 0) throw new IllegalStateException("Negative size"); return r; }
    protected int sizeOf(K key, V value) { return 1; }
    public final void evictAll() { trimToSize(-1); }
    public synchronized final int size() { return size; }
    public synchronized final int maxSize() { return maxSize; }
    public synchronized final int hitCount() { return hitCount; }
    public synchronized final int missCount() { return missCount; }
    public synchronized final int createCount() { return createCount; }
    public synchronized final int putCount() { return putCount; }
    public synchronized final int evictionCount() { return evictionCount; }
    public synchronized final java.util.Map<K, V> snapshot() { return new java.util.LinkedHashMap<>(map); }
}
