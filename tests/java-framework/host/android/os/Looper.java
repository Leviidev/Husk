package android.os;
public final class Looper {
    public interface InputSink { void deliver(int[] events); }
    public static void setInputSink(InputSink sink) {}
    private static final Looper main = new Looper();
    public static Looper getMainLooper() { return main; }
    public static Looper myLooper() { return main; }
}
