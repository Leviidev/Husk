package android.os.storage;
public class StorageManager {
    public static final String ACTION_MANAGE_STORAGE = "android.os.storage.action.MANAGE_STORAGE";
    public java.util.List<StorageVolume> getStorageVolumes() { java.util.ArrayList<StorageVolume> l = new java.util.ArrayList<>(); l.add(getPrimaryStorageVolume()); return l; }
    public StorageVolume getPrimaryStorageVolume() { return new StorageVolume(); }
    public StorageVolume getStorageVolume(java.io.File f) { return new StorageVolume(); }
    public boolean isEncrypted(java.io.File f) { return false; }
    public long getAllocatableBytes(java.util.UUID u) { return new java.io.File(husk.Native.dataDir()).getUsableSpace(); }
    public java.util.UUID getUuidForPath(java.io.File f) { return java.util.UUID.nameUUIDFromBytes(new byte[0]); }
    public void allocateBytes(java.util.UUID u, long n) {}
    public boolean isCacheBehaviorGroup(java.io.File f) { return false; }
}
