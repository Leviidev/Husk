package android.app;

/** Process headroom must reach the game even while the device has free RAM. */
public final class MemoryInfoTest {
    public static void main(String[] args) {
        ActivityManager.MemoryInfo info = new ActivityManager.MemoryInfo();
        ActivityManager.huskApplyMemoryInfo(info, new long[] {8L << 30, 2L << 30});
        if (info.totalMem != 8L << 30 || info.availMem != 2L << 30 || info.lowMemory)
            throw new AssertionError("initial memory snapshot");
        ActivityManager.huskApplyMemoryInfo(info, new long[] {8L << 30, 45L << 20});
        if (info.availMem != 45L << 20 || !info.lowMemory || info.freeMem != info.availMem)
            throw new AssertionError("jetsam headroom must replace the old fixed free-memory answer");
        ActivityManager.huskApplyMemoryInfo(info, new long[] {8L << 30, 12L << 30});
        if (info.availMem != info.totalMem) throw new AssertionError("available exceeds total");
        if (ActivityManager.huskMemoryClassFromInfo(new long[] {8L << 30, 2L << 30}) != 512
            || ActivityManager.huskLargeMemoryClassFromInfo(new long[] {8L << 30, 2L << 30}) != 1024
            || ActivityManager.huskMemoryClassFromInfo(new long[] {4L << 30, 1L << 30}) != 256
            || ActivityManager.huskLargeMemoryClassFromInfo(new long[] {4L << 30, 1L << 30}) != 512)
            throw new AssertionError("memory classes must scale cleanly with host RAM");
        System.out.println("PASS: ActivityManager reports changing headroom, scaled memory class and low-memory state");
    }
}
