package android.os.storage;
public final class StorageVolume implements android.os.Parcelable {
    public java.io.File getDirectory() { return new java.io.File(husk.Native.externalDir()); }
    public boolean isPrimary() { return true; } public boolean isRemovable() { return false; } public boolean isEmulated() { return true; }
    public String getState() { return "mounted"; } public String getUuid() { return null; } public String getDescription(android.content.Context c) { return "Internal storage"; }
    public int describeContents() { return 0; }
}
