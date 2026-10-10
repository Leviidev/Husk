package android.os;
/** Systrace sections: accepted, never recorded. */
public final class Trace {
    private Trace() {}
    public static boolean isEnabled() { return false; }
    public static void beginSection(String name) {}
    public static void endSection() {}
    public static void beginAsyncSection(String name, int cookie) {}
    public static void endAsyncSection(String name, int cookie) {}
    public static void setCounter(String name, long value) {}
}
