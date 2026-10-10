package android.util;
public final class Rational extends Number implements Comparable<Rational> {
    private final int n, d;
    public Rational(int n, int d) { this.n = n; this.d = d; }
    public int getNumerator() { return n; } public int getDenominator() { return d; }
    public double doubleValue() { return (double) n / d; } public float floatValue() { return (float) n / d; } public int intValue() { return d == 0 ? 0 : n / d; } public long longValue() { return intValue(); }
    public int compareTo(Rational o) { return Double.compare(doubleValue(), o.doubleValue()); }
}
