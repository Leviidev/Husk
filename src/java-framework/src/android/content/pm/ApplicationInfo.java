package android.content.pm;
public class ApplicationInfo extends PackageItemInfo implements android.os.Parcelable {
    public static final int FLAG_SYSTEM = 1, FLAG_DEBUGGABLE = 2, FLAG_HAS_CODE = 4, FLAG_PERSISTENT = 8, FLAG_FACTORY_TEST = 16, FLAG_ALLOW_TASK_REPARENTING = 32,
        FLAG_ALLOW_CLEAR_USER_DATA = 64, FLAG_UPDATED_SYSTEM_APP = 128, FLAG_TEST_ONLY = 256, FLAG_SUPPORTS_SMALL_SCREENS = 512, FLAG_SUPPORTS_NORMAL_SCREENS = 1024,
        FLAG_SUPPORTS_LARGE_SCREENS = 2048, FLAG_RESIZEABLE_FOR_SCREENS = 4096, FLAG_SUPPORTS_SCREEN_DENSITIES = 8192, FLAG_VM_SAFE_MODE = 16384, FLAG_ALLOW_BACKUP = 32768,
        FLAG_KILL_AFTER_RESTORE = 65536, FLAG_RESTORE_ANY_VERSION = 131072, FLAG_EXTERNAL_STORAGE = 262144, FLAG_SUPPORTS_XLARGE_SCREENS = 524288, FLAG_LARGE_HEAP = 1048576,
        FLAG_STOPPED = 2097152, FLAG_SUPPORTS_RTL = 4194304, FLAG_INSTALLED = 8388608, FLAG_IS_DATA_ONLY = 16777216, FLAG_IS_GAME = 33554432, FLAG_FULL_BACKUP_ONLY = 67108864,
        FLAG_USES_CLEARTEXT_TRAFFIC = 134217728, FLAG_EXTRACT_NATIVE_LIBS = 268435456, FLAG_HARDWARE_ACCELERATED = 536870912, FLAG_SUSPENDED = 1073741824, FLAG_MULTIARCH = 0x80000000;
    public static final int CATEGORY_UNDEFINED = -1, CATEGORY_GAME = 0, CATEGORY_AUDIO = 1, CATEGORY_VIDEO = 2, CATEGORY_IMAGE = 3, CATEGORY_SOCIAL = 4, CATEGORY_NEWS = 5,
        CATEGORY_MAPS = 6, CATEGORY_PRODUCTIVITY = 7, CATEGORY_ACCESSIBILITY = 8;
    private static ApplicationInfo sSelf;
    public String taskAffinity, permission, processName, className, sourceDir, publicSourceDir, dataDir, nativeLibraryDir, deviceProtectedDataDir, manageSpaceActivityName,
        backupAgentName, appComponentFactory, nativeLibraryRootDir, primaryCpuAbi;
    public String[] splitSourceDirs, splitPublicSourceDirs, splitNames, sharedLibraryFiles, resourceDirs;
    public int theme, descriptionRes, uiOptions, flags, uid = 10100, targetSdkVersion = 34, minSdkVersion = 21, compileSdkVersion = 34, category = CATEGORY_UNDEFINED, largestWidthLimitDp,
        requiresSmallestWidthDp, compatibleWidthLimitDp;
    public boolean enabled = true;
    public java.util.UUID storageUuid;
    public ApplicationInfo() {}
    public ApplicationInfo(ApplicationInfo o) { super(o); taskAffinity = o.taskAffinity; processName = o.processName; className = o.className; sourceDir = o.sourceDir; publicSourceDir = o.publicSourceDir; dataDir = o.dataDir; nativeLibraryDir = o.nativeLibraryDir; theme = o.theme; flags = o.flags; uid = o.uid; targetSdkVersion = o.targetSdkVersion; minSdkVersion = o.minSdkVersion; enabled = o.enabled; }
    public static synchronized ApplicationInfo self() {
        if (sSelf == null) {
            husk.Manifest.read();
            ApplicationInfo a = new ApplicationInfo();
            a.packageName = a.processName = husk.Native.packageName();
            a.dataDir = a.deviceProtectedDataDir = husk.Native.dataDir();
            a.sourceDir = a.publicSourceDir = husk.Native.apkPath();
            a.nativeLibraryDir = a.dataDir + "/lib";
            a.className = husk.Manifest.applicationClass;
            a.name = husk.Manifest.applicationClass;
            a.theme = husk.Manifest.appTheme; a.labelRes = husk.Manifest.appLabel; a.icon = husk.Manifest.appIcon;
            a.targetSdkVersion = husk.Manifest.targetSdk; a.minSdkVersion = husk.Manifest.minSdk;
            a.flags = FLAG_HAS_CODE | FLAG_INSTALLED | FLAG_ALLOW_BACKUP | FLAG_SUPPORTS_RTL | FLAG_HARDWARE_ACCELERATED | (husk.Manifest.debuggable ? FLAG_DEBUGGABLE : 0) | (husk.Manifest.largeHeap ? FLAG_LARGE_HEAP : 0);
            a.metaData = husk.Manifest.appMetaData;
            a.appComponentFactory = husk.Manifest.appComponentFactory;
            a.uid = android.os.Process.myUid();
            sSelf = a;
        }
        return sSelf;
    }
    public boolean isProfileableByShell() { return false; }
    public boolean isResourceOverlay() { return false; }
    public boolean isVirtualPreload() { return false; }
    public int getGwpAsanMode() { return -1; }
    public int describeContents() { return 0; }
    public static CharSequence getCategoryTitle(android.content.Context c, int cat) { return null; }
}
