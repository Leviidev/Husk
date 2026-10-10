package android.content.res;

import java.io.*;

public class AssetFileDescriptor implements Closeable {
    public static final long UNKNOWN_LENGTH = -1;
    private final String asset;
    private final FileDescriptor fd;
    private final long start, length;
    AssetFileDescriptor(String asset, FileDescriptor fd, long start, long length) { this.asset = asset; this.fd = fd; this.start = start; this.length = length; }
    public FileDescriptor getFileDescriptor() { return fd; }
    public long getStartOffset() { return start; }
    public long getLength() { return length; }
    public long getDeclaredLength() { return length; }
    /** Husk's own: the asset this describes, for players that read it by name. */
    public String huskAssetName() { return asset; }
    public FileInputStream createInputStream() throws IOException { return new FileInputStream(fd); }
    public void close() {}
}
