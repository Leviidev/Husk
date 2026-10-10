package android.util;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;

/** Writes a file all at once or not at all: the old version is kept as name.bak until the new one is complete. */
public class AtomicFile {
    private final File mBase, mBackup;
    public AtomicFile(File base) { this(base, null); }
    public AtomicFile(File base, String commitTag) { mBase = base; mBackup = new File(base.getPath() + ".bak"); }
    public File getBaseFile() { return mBase; }
    public void delete() { mBase.delete(); mBackup.delete(); }
    public boolean exists() { return mBase.exists() || mBackup.exists(); }
    public FileOutputStream startWrite() throws IOException { return startWrite(0); }
    public FileOutputStream startWrite(long startTime) throws IOException {
        if (mBase.exists()) {
            if (!mBackup.exists()) { if (!mBase.renameTo(mBackup)) Log.w("AtomicFile", "Couldn't rename file " + mBase + " to backup file " + mBackup); }
            else mBase.delete();
        }
        try { return new FileOutputStream(mBase); }
        catch (FileNotFoundException e) {
            File parent = mBase.getParentFile();
            if (parent == null || !parent.mkdirs()) throw new IOException("Failed to create directory for " + mBase);
            return new FileOutputStream(mBase);
        }
    }
    public void finishWrite(FileOutputStream str) {
        if (str == null) return;
        try { str.getFD().sync(); } catch (IOException e) {}
        try { str.close(); } catch (IOException e) {}
        mBackup.delete();
    }
    public void failWrite(FileOutputStream str) {
        if (str == null) return;
        try { str.close(); } catch (IOException e) {}
        mBase.delete();
        mBackup.renameTo(mBase);
    }
    @Deprecated public void truncate() throws IOException { try (FileOutputStream f = new FileOutputStream(mBase)) {} }
    @Deprecated public FileOutputStream openAppend() throws IOException { return new FileOutputStream(mBase, true); }
    public FileInputStream openRead() throws FileNotFoundException {
        if (mBackup.exists()) { mBase.delete(); mBackup.renameTo(mBase); }
        return new FileInputStream(mBase);
    }
    public long getLastModifiedTime() { return mBackup.exists() ? mBackup.lastModified() : mBase.lastModified(); }
    public byte[] readFully() throws IOException {
        try (FileInputStream in = openRead()) {
            java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
            byte[] b = new byte[8192]; int n;
            while ((n = in.read(b)) > 0) out.write(b, 0, n);
            return out.toByteArray();
        }
    }
    @Override public String toString() { return "AtomicFile[" + mBase + "]"; }
}
