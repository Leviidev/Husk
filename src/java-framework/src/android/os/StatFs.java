package android.os;

/** The file system a path is on: its size and free space, from the host. */
public class StatFs {
    private java.io.File mFile;
    public StatFs(String path) { restat(path); }
    public void restat(String path) { mFile = new java.io.File(path); }
    @Deprecated public int getBlockSize() { return 4096; }
    public long getBlockSizeLong() { return 4096; }
    @Deprecated public int getBlockCount() { return (int) Math.min(Integer.MAX_VALUE, getBlockCountLong()); }
    public long getBlockCountLong() { return mFile.getTotalSpace() / 4096; }
    @Deprecated public int getFreeBlocks() { return (int) Math.min(Integer.MAX_VALUE, getFreeBlocksLong()); }
    public long getFreeBlocksLong() { return mFile.getFreeSpace() / 4096; }
    public long getFreeBytes() { return mFile.getFreeSpace(); }
    @Deprecated public int getAvailableBlocks() { return (int) Math.min(Integer.MAX_VALUE, getAvailableBlocksLong()); }
    public long getAvailableBlocksLong() { return mFile.getUsableSpace() / 4096; }
    public long getAvailableBytes() { return mFile.getUsableSpace(); }
    public long getTotalBytes() { return mFile.getTotalSpace(); }
}
