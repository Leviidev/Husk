package android.util;
public final class Range<T extends Comparable<? super T>> {
    private final T lo, hi;
    public Range(T lo, T hi) { this.lo = lo; this.hi = hi; }
    public static <T extends Comparable<? super T>> Range<T> create(T lo, T hi) { return new Range<>(lo, hi); }
    public T getLower() { return lo; } public T getUpper() { return hi; }
    public boolean contains(T v) { return v.compareTo(lo) >= 0 && v.compareTo(hi) <= 0; }
    public T clamp(T v) { return v.compareTo(lo) < 0 ? lo : v.compareTo(hi) > 0 ? hi : v; }
    @Override public String toString() { return "[" + lo + ", " + hi + "]"; }
}
