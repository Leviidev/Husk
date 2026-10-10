package android.os;

public final class Debug {
    public static long getNativeHeapAllocatedSize() { return Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory(); }
    public static long getNativeHeapSize() { return Runtime.getRuntime().totalMemory(); }
    public static long getNativeHeapFreeSize() { return Runtime.getRuntime().freeMemory(); }
    public static boolean isDebuggerConnected() { return false; }
    public static boolean waitingForDebugger() { return false; }
    public static class MemoryInfo { public int dalvikPss, nativePss, otherPss; public int getTotalPss() { return 0; } }
    public static void getMemoryInfo(MemoryInfo m) {}
}
