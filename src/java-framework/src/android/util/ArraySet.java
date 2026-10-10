package android.util;
public final class ArraySet<E> extends java.util.LinkedHashSet<E> {
    public ArraySet() {}
    public ArraySet(int cap) { super(Math.max(cap, 1)); }
    public ArraySet(ArraySet<E> s) { super(s); }
    public ArraySet(java.util.Collection<? extends E> s) { super(s == null ? java.util.Collections.<E>emptySet() : s); }
    public ArraySet(E[] a) { super(java.util.Arrays.asList(a)); }
    @SuppressWarnings("unchecked") public E valueAt(int i) { return (E) toArray()[i]; }
    public E removeAt(int i) { E e = valueAt(i); remove(e); return e; }
    public int indexOf(Object key) { int i = 0; for (E e : this) { if (java.util.Objects.equals(e, key)) return i; i++; } return -1; }
    public void ensureCapacity(int c) {}
    public void addAll(ArraySet<? extends E> s) { super.addAll(s); }
}
