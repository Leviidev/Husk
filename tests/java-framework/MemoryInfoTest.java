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
        System.out.println("PASS: ActivityManager reports changing headroom and low-memory state");
    }
}
