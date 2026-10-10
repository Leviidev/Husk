package android.os;

import java.io.File;

public class Environment {
    public static final String MEDIA_MOUNTED = "mounted", MEDIA_MOUNTED_READ_ONLY = "mounted_ro";
    public static String DIRECTORY_DOWNLOADS = "Download", DIRECTORY_PICTURES = "Pictures", DIRECTORY_MUSIC = "Music", DIRECTORY_DOCUMENTS = "Documents";
    public static File getExternalStorageDirectory() { return new File(husk.Native.externalDir()); }
    public static File getExternalStoragePublicDirectory(String type) { return new File(husk.Native.externalDir(), type); }
    public static String getExternalStorageState() { return MEDIA_MOUNTED; }
    public static String getExternalStorageState(File f) { return MEDIA_MOUNTED; }
    public static File getDataDirectory() { return new File(husk.Native.dataDir()); }
    public static File getRootDirectory() { return new File("/system"); }
    public static File getDownloadCacheDirectory() { return new File(husk.Native.dataDir(), "cache"); }
    public static boolean isExternalStorageEmulated() { return true; }
    public static boolean isExternalStorageRemovable() { return false; }
    public static boolean isExternalStorageManager() { return true; }
}
