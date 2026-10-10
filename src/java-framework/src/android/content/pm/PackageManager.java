package android.content.pm;

import android.content.ComponentName;
import android.content.Intent;
import java.util.ArrayList;
import java.util.List;

/** The one package there is (the app), answered from its manifest. */
public class PackageManager {
    public static final int PERMISSION_GRANTED = 0, PERMISSION_DENIED = -1, SIGNATURE_MATCH = 0, SIGNATURE_NO_MATCH = -3, SIGNATURE_UNKNOWN_PACKAGE = -4;
    public static final int GET_ACTIVITIES = 1, GET_RECEIVERS = 2, GET_SERVICES = 4, GET_PROVIDERS = 8, GET_INSTRUMENTATION = 16, GET_INTENT_FILTERS = 32, GET_SIGNATURES = 64,
        GET_RESOLVED_FILTER = 64, GET_META_DATA = 128, GET_GIDS = 256, GET_DISABLED_COMPONENTS = 512, GET_SHARED_LIBRARY_FILES = 1024, GET_URI_PERMISSION_PATTERNS = 2048,
        GET_PERMISSIONS = 4096, MATCH_UNINSTALLED_PACKAGES = 8192, GET_UNINSTALLED_PACKAGES = 8192, GET_CONFIGURATIONS = 16384, MATCH_DISABLED_UNTIL_USED_COMPONENTS = 32768,
        MATCH_DEFAULT_ONLY = 65536, MATCH_ALL = 131072, MATCH_DIRECT_BOOT_UNAWARE = 262144, MATCH_DIRECT_BOOT_AWARE = 524288, MATCH_SYSTEM_ONLY = 1048576,
        MATCH_DISABLED_COMPONENTS = 512, MATCH_APEX = 1073741824, GET_SIGNING_CERTIFICATES = 134217728, MATCH_DIRECT_BOOT_AUTO = 268435456;
    public static final int COMPONENT_ENABLED_STATE_DEFAULT = 0, COMPONENT_ENABLED_STATE_ENABLED = 1, COMPONENT_ENABLED_STATE_DISABLED = 2, COMPONENT_ENABLED_STATE_DISABLED_USER = 3,
        COMPONENT_ENABLED_STATE_DISABLED_UNTIL_USED = 4, DONT_KILL_APP = 1, SYNCHRONOUS = 2, INSTALL_REASON_UNKNOWN = 0, VERIFICATION_ALLOW = 1, CERT_INPUT_SHA256 = 1;
    public static final String FEATURE_TOUCHSCREEN = "android.hardware.touchscreen", FEATURE_TOUCHSCREEN_MULTITOUCH = "android.hardware.touchscreen.multitouch",
        FEATURE_TOUCHSCREEN_MULTITOUCH_DISTINCT = "android.hardware.touchscreen.multitouch.distinct", FEATURE_TOUCHSCREEN_MULTITOUCH_JAZZHAND = "android.hardware.touchscreen.multitouch.jazzhand",
        FEATURE_CAMERA = "android.hardware.camera", FEATURE_CAMERA_ANY = "android.hardware.camera.any", FEATURE_CAMERA_FRONT = "android.hardware.camera.front", FEATURE_CAMERA_FLASH = "android.hardware.camera.flash",
        FEATURE_CAMERA_AUTOFOCUS = "android.hardware.camera.autofocus", FEATURE_TELEPHONY = "android.hardware.telephony", FEATURE_WIFI = "android.hardware.wifi", FEATURE_BLUETOOTH = "android.hardware.bluetooth",
        FEATURE_BLUETOOTH_LE = "android.hardware.bluetooth_le", FEATURE_NFC = "android.hardware.nfc", FEATURE_LOCATION = "android.hardware.location", FEATURE_LOCATION_GPS = "android.hardware.location.gps",
        FEATURE_MICROPHONE = "android.hardware.microphone", FEATURE_SENSOR_ACCELEROMETER = "android.hardware.sensor.accelerometer", FEATURE_SENSOR_GYROSCOPE = "android.hardware.sensor.gyroscope",
        FEATURE_SENSOR_COMPASS = "android.hardware.sensor.compass", FEATURE_LEANBACK = "android.software.leanback", FEATURE_TELEVISION = "android.hardware.type.television",
        FEATURE_WATCH = "android.hardware.type.watch", FEATURE_AUTOMOTIVE = "android.hardware.type.automotive", FEATURE_PC = "android.hardware.type.pc", FEATURE_FINGERPRINT = "android.hardware.fingerprint",
        FEATURE_VULKAN_HARDWARE_LEVEL = "android.hardware.vulkan.level", FEATURE_VULKAN_HARDWARE_VERSION = "android.hardware.vulkan.version", FEATURE_OPENGLES_EXTENSION_PACK = "android.hardware.opengles.aep",
        FEATURE_WEBVIEW = "android.software.webview", FEATURE_FAKETOUCH = "android.hardware.faketouch", FEATURE_SCREEN_PORTRAIT = "android.hardware.screen.portrait",
        FEATURE_SCREEN_LANDSCAPE = "android.hardware.screen.landscape", FEATURE_GAMEPAD = "android.hardware.gamepad", FEATURE_AUDIO_OUTPUT = "android.hardware.audio.output",
        FEATURE_PICTURE_IN_PICTURE = "android.software.picture_in_picture", FEATURE_FREEFORM_WINDOW_MANAGEMENT = "android.software.freeform_window_management",
        FEATURE_EMBEDDED = "android.hardware.type.embedded", FEATURE_ACTIVITIES_ON_SECONDARY_DISPLAYS = "android.software.activities_on_secondary_displays",
        FEATURE_HARDWARE_KEYSTORE = "android.hardware.hardware_keystore", FEATURE_STRONGBOX_KEYSTORE = "android.hardware.strongbox_keystore", FEATURE_USB_HOST = "android.hardware.usb.host",
        FEATURE_SECURE_LOCK_SCREEN = "android.software.secure_lock_screen", FEATURE_DEVICE_ADMIN = "android.software.device_admin", FEATURE_MANAGED_USERS = "android.software.managed_users",
        FEATURE_APP_WIDGETS = "android.software.app_widgets", FEATURE_HOME_SCREEN = "android.software.home_screen", FEATURE_INPUT_METHODS = "android.software.input_methods",
        FEATURE_LIVE_WALLPAPER = "android.software.live_wallpaper", FEATURE_PRINTING = "android.software.print", FEATURE_BACKUP = "android.software.backup";
    public static final String EXTRA_VERIFICATION_ID = "android.content.pm.extra.VERIFICATION_ID";
    public static class NameNotFoundException extends android.util.AndroidException { public NameNotFoundException() {} public NameNotFoundException(String s) { super(s); } }
    private static PackageManager sSelf;
    public static synchronized PackageManager self() { if (sSelf == null) sSelf = new PackageManager(); return sSelf; }
    private static boolean mine(String pkg) { return pkg != null && pkg.equals(husk.Native.packageName()); }
    private static ActivityInfo activity(husk.Manifest.Component c) {
        ActivityInfo a = new ActivityInfo();
        a.name = c.name; a.packageName = husk.Native.packageName(); a.theme = c.theme; a.labelRes = c.label; a.icon = c.icon; a.screenOrientation = c.screenOrientation;
        a.configChanges = c.configChanges; a.launchMode = c.launchMode; a.exported = c.exported; a.enabled = c.enabled; a.metaData = c.metaData; a.parentActivityName = c.parentActivity;
        a.softInputMode = c.windowSoftInputMode; a.processName = a.packageName; a.taskAffinity = a.packageName;
        return a;
    }
    private static ServiceInfo service(husk.Manifest.Component c) { ServiceInfo s = new ServiceInfo(); s.name = c.name; s.packageName = husk.Native.packageName(); s.exported = c.exported; s.enabled = c.enabled; s.metaData = c.metaData; s.permission = c.permission; return s; }
    private static ProviderInfo provider(husk.Manifest.Component c) { ProviderInfo p = new ProviderInfo(); p.name = c.name; p.packageName = husk.Native.packageName(); p.authority = c.authorities; p.exported = c.exported; p.metaData = c.metaData; return p; }
    public PackageInfo getPackageInfo(String pkg, int flags) throws NameNotFoundException {
        if (!mine(pkg)) throw new NameNotFoundException(pkg);
        husk.Manifest.read();
        PackageInfo p = new PackageInfo();
        p.packageName = pkg; p.versionCode = husk.Manifest.versionCode; p.versionName = husk.Manifest.versionName != null ? husk.Manifest.versionName : "1.0";
        p.firstInstallTime = p.lastUpdateTime = new java.io.File(husk.Native.apkPath()).lastModified();
        p.requestedPermissions = husk.Manifest.permissions.toArray(new String[0]);
        p.requestedPermissionsFlags = new int[p.requestedPermissions.length];
        java.util.Arrays.fill(p.requestedPermissionsFlags, PackageInfo.REQUESTED_PERMISSION_GRANTED);
        if ((flags & GET_ACTIVITIES) != 0) { p.activities = new ActivityInfo[husk.Manifest.activities.size()]; for (int i = 0; i < p.activities.length; i++) p.activities[i] = activity(husk.Manifest.activities.get(i)); }
        if ((flags & GET_RECEIVERS) != 0) { p.receivers = new ActivityInfo[husk.Manifest.receivers.size()]; for (int i = 0; i < p.receivers.length; i++) p.receivers[i] = activity(husk.Manifest.receivers.get(i)); }
        if ((flags & GET_SERVICES) != 0) { p.services = new ServiceInfo[husk.Manifest.services.size()]; for (int i = 0; i < p.services.length; i++) p.services[i] = service(husk.Manifest.services.get(i)); }
        if ((flags & GET_PROVIDERS) != 0) { p.providers = new ProviderInfo[husk.Manifest.providers.size()]; for (int i = 0; i < p.providers.length; i++) p.providers[i] = provider(husk.Manifest.providers.get(i)); }
        return p;
    }
    public PackageInfo getPackageInfo(String pkg, PackageInfoFlags flags) throws NameNotFoundException { return getPackageInfo(pkg, (int) flags.getValue()); }
    public PackageInfo getPackageInfo(android.content.pm.VersionedPackage v, int flags) throws NameNotFoundException { return getPackageInfo(v.getPackageName(), flags); }
    public static final class PackageInfoFlags { private final long v; private PackageInfoFlags(long v) { this.v = v; } public static PackageInfoFlags of(long v) { return new PackageInfoFlags(v); } public long getValue() { return v; } }
    public static final class ApplicationInfoFlags { private final long v; private ApplicationInfoFlags(long v) { this.v = v; } public static ApplicationInfoFlags of(long v) { return new ApplicationInfoFlags(v); } public long getValue() { return v; } }
    public static final class ComponentInfoFlags { private final long v; private ComponentInfoFlags(long v) { this.v = v; } public static ComponentInfoFlags of(long v) { return new ComponentInfoFlags(v); } public long getValue() { return v; } }
    public static final class ResolveInfoFlags { private final long v; private ResolveInfoFlags(long v) { this.v = v; } public static ResolveInfoFlags of(long v) { return new ResolveInfoFlags(v); } public long getValue() { return v; } }
    public ApplicationInfo getApplicationInfo(String pkg, int flags) throws NameNotFoundException { if (!mine(pkg)) throw new NameNotFoundException(pkg); return ApplicationInfo.self(); }
    public ApplicationInfo getApplicationInfo(String pkg, ApplicationInfoFlags flags) throws NameNotFoundException { return getApplicationInfo(pkg, 0); }
    public ActivityInfo getActivityInfo(ComponentName c, int flags) throws NameNotFoundException {
        husk.Manifest.Component m = husk.Manifest.activity(c.getClassName());
        if (m == null) for (husk.Manifest.Component a : husk.Manifest.activities) if (a.name.endsWith(c.getClassName())) m = a;
        if (m == null) throw new NameNotFoundException(c.toString());
        return activity(m);
    }
    public ActivityInfo getActivityInfo(ComponentName c, ComponentInfoFlags flags) throws NameNotFoundException { return getActivityInfo(c, 0); }
    public ActivityInfo getReceiverInfo(ComponentName c, int flags) throws NameNotFoundException { for (husk.Manifest.Component r : husk.Manifest.receivers) if (r.name.equals(c.getClassName())) return activity(r); throw new NameNotFoundException(c.toString()); }
    public ServiceInfo getServiceInfo(ComponentName c, int flags) throws NameNotFoundException { for (husk.Manifest.Component s : husk.Manifest.services) if (s.name.equals(c.getClassName())) return service(s); throw new NameNotFoundException(c.toString()); }
    public ServiceInfo getServiceInfo(ComponentName c, ComponentInfoFlags flags) throws NameNotFoundException { return getServiceInfo(c, 0); }
    public ProviderInfo getProviderInfo(ComponentName c, int flags) throws NameNotFoundException { for (husk.Manifest.Component p : husk.Manifest.providers) if (p.name.equals(c.getClassName())) return provider(p); throw new NameNotFoundException(c.toString()); }
    public ProviderInfo resolveContentProvider(String authority, int flags) { for (husk.Manifest.Component p : husk.Manifest.providers) if (p.authorities != null && java.util.Arrays.asList(p.authorities.split(";")).contains(authority)) return provider(p); return null; }
    public List<ProviderInfo> queryContentProviders(String process, int uid, int flags) { ArrayList<ProviderInfo> l = new ArrayList<>(); for (husk.Manifest.Component p : husk.Manifest.providers) l.add(provider(p)); return l; }
    public boolean hasSystemFeature(String name) { return hasSystemFeature(name, 0); }
    public boolean hasSystemFeature(String name, int version) {
        if (name == null) return false;
        if (name.startsWith("android.hardware.touchscreen") || name.equals(FEATURE_FAKETOUCH) || name.equals(FEATURE_SCREEN_PORTRAIT) || name.equals(FEATURE_SCREEN_LANDSCAPE)) return true;
        if (name.equals(FEATURE_WIFI) || name.equals(FEATURE_AUDIO_OUTPUT) || name.equals(FEATURE_OPENGLES_EXTENSION_PACK) || name.equals(FEATURE_SENSOR_ACCELEROMETER)) return true;
        if (name.equals(FEATURE_INPUT_METHODS) || name.equals(FEATURE_GAMEPAD)) return true;
        return false;
    }
    public FeatureInfo[] getSystemAvailableFeatures() { return new FeatureInfo[0]; }
    public String[] getSystemSharedLibraryNames() { return new String[0]; }
    public int checkPermission(String perm, String pkg) { return PERMISSION_GRANTED; }
    public boolean isPermissionRevokedByPolicy(String perm, String pkg) { return false; }
    public PermissionInfo getPermissionInfo(String name, int flags) throws NameNotFoundException { PermissionInfo p = new PermissionInfo(); p.name = name; return p; }
    public int checkSignatures(String a, String b) { return SIGNATURE_MATCH; }
    public int checkSignatures(int a, int b) { return SIGNATURE_MATCH; }
    public boolean hasSigningCertificate(String pkg, byte[] cert, int type) { return true; }
    public CharSequence getApplicationLabel(ApplicationInfo a) { return a.loadLabel(this); }
    public android.graphics.drawable.Drawable getApplicationIcon(ApplicationInfo a) { return a.loadIcon(this); }
    public android.graphics.drawable.Drawable getApplicationIcon(String pkg) throws NameNotFoundException { return getApplicationIcon(getApplicationInfo(pkg, 0)); }
    public android.graphics.drawable.Drawable getApplicationLogo(ApplicationInfo a) { return null; }
    public android.graphics.drawable.Drawable getActivityIcon(ComponentName c) throws NameNotFoundException { return getActivityInfo(c, 0).loadIcon(this); }
    public android.graphics.drawable.Drawable getDefaultActivityIcon() { return new android.graphics.drawable.ColorDrawable(0); }
    public android.graphics.drawable.Drawable getDrawable(String pkg, int res, ApplicationInfo a) { try { return husk.ContextImpl.app().getResources().getDrawable(res); } catch (Exception e) { return null; } }
    public CharSequence getText(String pkg, int res, ApplicationInfo a) { try { return husk.ContextImpl.app().getResources().getText(res); } catch (Exception e) { return null; } }
    public android.content.res.Resources getResourcesForApplication(String pkg) throws NameNotFoundException { if (!mine(pkg) && !"android".equals(pkg)) throw new NameNotFoundException(pkg); return husk.ContextImpl.app().getResources(); }
    public android.content.res.Resources getResourcesForApplication(ApplicationInfo a) throws NameNotFoundException { return getResourcesForApplication(a.packageName); }
    public android.content.res.Resources getResourcesForActivity(ComponentName c) { return husk.ContextImpl.app().getResources(); }
    public String getInstallerPackageName(String pkg) { return "com.android.vending"; }
    public InstallSourceInfo getInstallSourceInfo(String pkg) throws NameNotFoundException { return new InstallSourceInfo(); }
    public List<ResolveInfo> queryIntentActivities(Intent i, int flags) {
        ArrayList<ResolveInfo> l = new ArrayList<>();
        husk.Manifest.Component c = husk.Manifest.resolve(i);
        if (c != null) { ResolveInfo r = new ResolveInfo(); r.activityInfo = activity(c); l.add(r); }
        else if (i.getData() != null && ("http".equals(i.getData().getScheme()) || "https".equals(i.getData().getScheme()) || "mailto".equals(i.getData().getScheme()))) {
            ResolveInfo r = new ResolveInfo(); r.activityInfo = new ActivityInfo(); r.activityInfo.name = "com.android.browser.Browser"; r.activityInfo.packageName = "com.android.browser"; r.nonLocalizedLabel = "Browser"; l.add(r);
        }
        return l;
    }
    public List<ResolveInfo> queryIntentActivities(Intent i, ResolveInfoFlags flags) { return queryIntentActivities(i, 0); }
    public ResolveInfo resolveActivity(Intent i, int flags) { List<ResolveInfo> l = queryIntentActivities(i, flags); return l.isEmpty() ? null : l.get(0); }
    public ResolveInfo resolveActivity(Intent i, ResolveInfoFlags flags) { return resolveActivity(i, 0); }
    public ResolveInfo resolveService(Intent i, int flags) { return null; }
    public List<ResolveInfo> queryIntentServices(Intent i, int flags) { return new ArrayList<>(); }
    public List<ResolveInfo> queryBroadcastReceivers(Intent i, int flags) { return new ArrayList<>(); }
    public List<ResolveInfo> queryIntentContentProviders(Intent i, int flags) { return new ArrayList<>(); }
    public Intent getLaunchIntentForPackage(String pkg) {
        if (!mine(pkg)) return null;
        husk.Manifest.Component c = husk.Manifest.launcher();
        return c == null ? null : new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER).setClassName(pkg, c.name);
    }
    public Intent getLeanbackLaunchIntentForPackage(String pkg) { return null; }
    public List<PackageInfo> getInstalledPackages(int flags) { ArrayList<PackageInfo> l = new ArrayList<>(); try { l.add(getPackageInfo(husk.Native.packageName(), flags)); } catch (NameNotFoundException e) {} return l; }
    public List<PackageInfo> getInstalledPackages(PackageInfoFlags flags) { return getInstalledPackages(0); }
    public List<ApplicationInfo> getInstalledApplications(int flags) { ArrayList<ApplicationInfo> l = new ArrayList<>(); l.add(ApplicationInfo.self()); return l; }
    public String[] getPackagesForUid(int uid) { return new String[] { husk.Native.packageName() }; }
    public String getNameForUid(int uid) { return husk.Native.packageName(); }
    public int getPackageUid(String pkg, int flags) throws NameNotFoundException { if (!mine(pkg)) throw new NameNotFoundException(pkg); return android.os.Process.myUid(); }
    public void setComponentEnabledSetting(ComponentName c, int state, int flags) {}
    public int getComponentEnabledSetting(ComponentName c) { return COMPONENT_ENABLED_STATE_DEFAULT; }
    public void setApplicationEnabledSetting(String pkg, int state, int flags) {}
    public int getApplicationEnabledSetting(String pkg) { return COMPONENT_ENABLED_STATE_DEFAULT; }
    public boolean isSafeMode() { return false; }
    public boolean isInstantApp() { return false; }
    public boolean isInstantApp(String pkg) { return false; }
    public boolean canRequestPackageInstalls() { return false; }
    public PackageInstaller getPackageInstaller() { return null; }
    public android.content.pm.ModuleInfo getModuleInfo(String pkg, int flags) throws NameNotFoundException { throw new NameNotFoundException(pkg); }
    public List<SharedLibraryInfo> getSharedLibraries(int flags) { return new ArrayList<>(); }
    public boolean isPackageSuspended() { return false; }
    public CharSequence getUserBadgedLabel(CharSequence l, android.os.UserHandle u) { return l; }
    public android.graphics.drawable.Drawable getUserBadgedIcon(android.graphics.drawable.Drawable d, android.os.UserHandle u) { return d; }
    public android.content.res.XmlResourceParser getXml(String pkg, int res, ApplicationInfo a) { return husk.ContextImpl.app().getResources().getXml(res); }
    public ChangedPackages getChangedPackages(int seq) { return null; }
    public static final class ChangedPackages {}
    public void addPermissionHusk() {}
    public Bundle_ getSuspendedPackageAppExtrasHusk() { return null; }
    interface Bundle_ {}
}
