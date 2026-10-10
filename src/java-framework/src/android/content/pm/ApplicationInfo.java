package android.content.pm;
public class ApplicationInfo {
    public static final int FLAG_DEBUGGABLE = 2, FLAG_SYSTEM = 1;
    private static ApplicationInfo sSelf;
    public String packageName, sourceDir, publicSourceDir, dataDir, nativeLibraryDir, processName, className, name;
    public int flags, targetSdkVersion = 34, minSdkVersion = 21, uid = 10100, icon, labelRes, theme;
    public String[] splitSourceDirs;
    public static synchronized ApplicationInfo self() {
        if (sSelf == null) {
            sSelf = new ApplicationInfo();
            sSelf.packageName = sSelf.processName = husk.Native.packageName();
            sSelf.dataDir = husk.Native.dataDir();
            sSelf.sourceDir = sSelf.publicSourceDir = sSelf.dataDir + "/base.apk";
            sSelf.nativeLibraryDir = sSelf.dataDir + "/lib";
        }
        return sSelf;
    }
    public CharSequence loadLabel(PackageManager pm) { return packageName; }
}
