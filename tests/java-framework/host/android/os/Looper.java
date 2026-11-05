package android.os;
public final class Looper {
    private static final Looper main = new Looper();
    public static Looper getMainLooper() { return main; }
    public static Looper myLooper() { return main; }
}
