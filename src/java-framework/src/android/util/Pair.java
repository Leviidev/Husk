package android.util;
public class Pair<F, S> {
    public final F first; public final S second;
    public Pair(F f, S s) { first = f; second = s; }
    public static <A, B> Pair<A, B> create(A a, B b) { return new Pair<>(a, b); }
    @Override public boolean equals(Object o) { if (!(o instanceof Pair)) return false; Pair<?, ?> p = (Pair<?, ?>) o; return java.util.Objects.equals(p.first, first) && java.util.Objects.equals(p.second, second); }
    @Override public int hashCode() { return (first == null ? 0 : first.hashCode()) ^ (second == null ? 0 : second.hashCode()); }
    @Override public String toString() { return "Pair{" + first + " " + second + "}"; }
}
