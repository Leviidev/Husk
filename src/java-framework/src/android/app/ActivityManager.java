package android.app;
public class ActivityManager {
    public static class MemoryInfo { public long availMem = 2L << 30, totalMem = 6L << 30, threshold = 256L << 20; public boolean lowMemory; }
    public void getMemoryInfo(MemoryInfo m) {}
    public int getMemoryClass() { return 512; }
    public int getLargeMemoryClass() { return 512; }
    public boolean isLowRamDevice() { return false; }
    public static boolean isRunningInTestHarness() { return false; }
}
