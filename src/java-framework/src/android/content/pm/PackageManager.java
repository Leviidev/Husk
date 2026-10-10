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
        if ("android".equals(pkg)) {
            // the platform itself (okhttp, Play libraries and WebView checks read its version)
            PackageInfo p = new PackageInfo();
            p.packageName = "android"; p.versionCode = android.os.Build.VERSION.SDK_INT; p.versionName = android.os.Build.VERSION.RELEASE;
            ApplicationInfo a = new ApplicationInfo();
            a.packageName = a.processName = "android"; a.flags = ApplicationInfo.FLAG_SYSTEM; a.targetSdkVersion = a.minSdkVersion = android.os.Build.VERSION.SDK_INT; a.enabled = true;
            p.applicationInfo = a;
            p.signatures = new Signature[] { new Signature("") };
            return p;
        }
        if (husk.GooglePackages.is(pkg)) return husk.GooglePackages.packageInfo(pkg);
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
    public ApplicationInfo getApplicationInfo(String pkg, int flags) throws NameNotFoundException {
        if (husk.GooglePackages.is(pkg)) return husk.GooglePackages.applicationInfo(pkg);
        if (!mine(pkg)) throw new NameNotFoundException(pkg);
        return ApplicationInfo.self();
    }
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
    public ResolveInfo resolveService(Intent i, int flags) { List<ResolveInfo> l = queryIntentServices(i, flags); return l.isEmpty() ? null : l.get(0); }
    /** The app's own services the intent reaches; and an intent meant for Google Play services (push registration, its APIs)
     *  resolves to it, as it is installed (husk.GooglePackages) -- apps take the first answer without checking for none. */
    public List<ResolveInfo> queryIntentServices(Intent i, int flags) {
        ArrayList<ResolveInfo> l = new ArrayList<>();
        for (husk.Manifest.Component c : husk.Manifest.match(husk.Manifest.services, i)) { ResolveInfo r = new ResolveInfo(); r.serviceInfo = service(c); l.add(r); }
        if (l.isEmpty() && googleService(i)) {
            ResolveInfo r = new ResolveInfo(); ServiceInfo s = new ServiceInfo();
            s.packageName = "com.google.android.gms"; s.name = "com.google.android.gms.chimera.GmsIntentOperationService"; s.exported = true; s.enabled = true;
            try { s.applicationInfo = husk.GooglePackages.applicationInfo(s.packageName); } catch (Exception e) {}
            r.serviceInfo = s; l.add(r);
        }
        return l;
    }
    private static boolean googleService(Intent i) {
        String p = i.getComponent() != null ? i.getComponent().getPackageName() : i.getPackage();
        String a = i.getAction();
        return "com.google.android.gms".equals(p) || (a != null && (a.startsWith("com.google.android.c2dm.intent.") || a.startsWith("com.google.android.gms.") || a.startsWith("com.google.firebase.")));
    }
    public List<ResolveInfo> queryBroadcastReceivers(Intent i, int flags) {
        ArrayList<ResolveInfo> l = new ArrayList<>();
        for (husk.Manifest.Component c : husk.Manifest.match(husk.Manifest.receivers, i)) { ResolveInfo r = new ResolveInfo(); r.activityInfo = activity(c); l.add(r); }
        return l;
    }
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
    public int getPackageUid(String pkg, int flags) throws NameNotFoundException { if (husk.GooglePackages.is(pkg)) return husk.GooglePackages.applicationInfo(pkg).uid; if (!mine(pkg)) throw new NameNotFoundException(pkg); return android.os.Process.myUid(); }
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
    public android.content.pm.ChangedPackages getChangedPackages(int seq) { return null; }
    public void addPermissionHusk() {}
    public Bundle_ getSuspendedPackageAppExtrasHusk() { return null; }
    interface Bundle_ {}
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
    public static final java.lang.String ACTION_REQUEST_PERMISSIONS = "android.content.pm.action.REQUEST_PERMISSIONS";
    public static final java.lang.String ACTION_REQUEST_PERMISSIONS_FOR_OTHER = "android.content.pm.action.REQUEST_PERMISSIONS_FOR_OTHER";
    public static final java.lang.String ACTION_VERIFY_DEVELOPER = "android.content.pm.action.VERIFY_DEVELOPER";
    public static final boolean APPLY_DEFAULT_TO_DEVICE_PROTECTED_STORAGE = true;
    public static java.lang.String APP_DETAILS_ACTIVITY_CLASS_NAME;
    public static final boolean APP_ENUMERATION_ENABLED_BY_DEFAULT = true;
    public static final int APP_METADATA_SOURCE_APK = 1;
    public static final int APP_METADATA_SOURCE_INSTALLER = 2;
    public static final int APP_METADATA_SOURCE_SYSTEM_IMAGE = 3;
    public static final int APP_METADATA_SOURCE_UNKNOWN = 0;
    public static final int CERT_INPUT_RAW_X509 = 0;
    public static final int DELETE_ALL_USERS = 2;
    public static final int DELETE_ARCHIVE = 16;
    public static final int DELETE_CHATTY = -2147483648;
    public static final int DELETE_DONT_KILL_APP = 8;
    public static final int DELETE_FAILED_ABORTED = -5;
    public static final int DELETE_FAILED_APP_PINNED = -7;
    public static final int DELETE_FAILED_DEVICE_POLICY_MANAGER = -2;
    public static final int DELETE_FAILED_FOR_CHILD_PROFILE = -8;
    public static final int DELETE_FAILED_INTERNAL_ERROR = -1;
    public static final int DELETE_FAILED_OWNER_BLOCKED = -4;
    public static final int DELETE_FAILED_USED_SHARED_LIBRARY = -6;
    public static final int DELETE_FAILED_USER_RESTRICTED = -3;
    public static final int DELETE_KEEP_DATA = 1;
    public static final int DELETE_SUCCEEDED = 1;
    public static final int DELETE_SYSTEM_APP = 4;
    public static final boolean ENABLE_SHARED_UID_MIGRATION = true;
    public static final java.lang.String EXTRA_FAILURE_EXISTING_PACKAGE = "android.content.pm.extra.FAILURE_EXISTING_PACKAGE";
    public static final java.lang.String EXTRA_FAILURE_EXISTING_PERMISSION = "android.content.pm.extra.FAILURE_EXISTING_PERMISSION";
    public static final java.lang.String EXTRA_INTENT_FILTER_VERIFICATION_HOSTS = "android.content.pm.extra.INTENT_FILTER_VERIFICATION_HOSTS";
    public static final java.lang.String EXTRA_INTENT_FILTER_VERIFICATION_ID = "android.content.pm.extra.INTENT_FILTER_VERIFICATION_ID";
    public static final java.lang.String EXTRA_INTENT_FILTER_VERIFICATION_PACKAGE_NAME = "android.content.pm.extra.INTENT_FILTER_VERIFICATION_PACKAGE_NAME";
    public static final java.lang.String EXTRA_INTENT_FILTER_VERIFICATION_URI_SCHEME = "android.content.pm.extra.INTENT_FILTER_VERIFICATION_URI_SCHEME";
    public static final java.lang.String EXTRA_MOVE_ID = "android.content.pm.extra.MOVE_ID";
    public static final java.lang.String EXTRA_PACKAGE_MONITOR_CALLBACK_RESULT = "android.content.pm.extra.EXTRA_PACKAGE_MONITOR_CALLBACK_RESULT";
    public static final java.lang.String EXTRA_REQUEST_PERMISSIONS_DEVICE_ID = "android.content.pm.extra.REQUEST_PERMISSIONS_DEVICE_ID";
    public static final java.lang.String EXTRA_REQUEST_PERMISSIONS_LEGACY_ACCESS_PERMISSION_NAMES = "android.content.pm.extra.REQUEST_PERMISSIONS_LEGACY_ACCESS_PERMISSION_NAMES";
    public static final java.lang.String EXTRA_REQUEST_PERMISSIONS_NAMES = "android.content.pm.extra.REQUEST_PERMISSIONS_NAMES";
    public static final java.lang.String EXTRA_REQUEST_PERMISSIONS_RESULTS = "android.content.pm.extra.REQUEST_PERMISSIONS_RESULTS";
    public static final java.lang.String EXTRA_USER_ACTION_REQUIRED = "android.content.pm.extra.USER_ACTION_REQUIRED";
    public static final java.lang.String EXTRA_VERIFICATION_INSTALLER_PACKAGE = "android.content.pm.extra.VERIFICATION_INSTALLER_PACKAGE";
    public static final java.lang.String EXTRA_VERIFICATION_INSTALLER_UID = "android.content.pm.extra.VERIFICATION_INSTALLER_UID";
    public static final java.lang.String EXTRA_VERIFICATION_INSTALL_FLAGS = "android.content.pm.extra.VERIFICATION_INSTALL_FLAGS";
    public static final java.lang.String EXTRA_VERIFICATION_LONG_VERSION_CODE = "android.content.pm.extra.VERIFICATION_LONG_VERSION_CODE";
    public static final java.lang.String EXTRA_VERIFICATION_PACKAGE_NAME = "android.content.pm.extra.VERIFICATION_PACKAGE_NAME";
    public static final java.lang.String EXTRA_VERIFICATION_RESULT = "android.content.pm.extra.VERIFICATION_RESULT";
    public static final java.lang.String EXTRA_VERIFICATION_ROOT_HASH = "android.content.pm.extra.VERIFICATION_ROOT_HASH";
    public static final java.lang.String EXTRA_VERIFICATION_URI = "android.content.pm.extra.VERIFICATION_URI";
    public static final java.lang.String EXTRA_VERIFICATION_VERSION_CODE = "android.content.pm.extra.VERIFICATION_VERSION_CODE";
    public static final java.lang.String FEATURE_ADOPTABLE_STORAGE = "android.software.adoptable_storage";
    public static final java.lang.String FEATURE_APP_COMPAT_OVERRIDES = "android.software.app_compat_overrides";
    public static final java.lang.String FEATURE_APP_ENUMERATION = "android.software.app_enumeration";
    public static final java.lang.String FEATURE_ASSIST_GESTURE = "android.hardware.sensor.assist";
    public static final java.lang.String FEATURE_AUDIO_LOW_LATENCY = "android.hardware.audio.low_latency";
    public static final java.lang.String FEATURE_AUDIO_PRO = "android.hardware.audio.pro";
    public static final java.lang.String FEATURE_AUDIO_SPATIAL_HEADTRACKING_LOW_LATENCY = "android.hardware.audio.spatial.headtracking.low_latency";
    public static final java.lang.String FEATURE_AUTOFILL = "android.software.autofill";
    public static final java.lang.String FEATURE_BLUETOOTH_LE_CHANNEL_SOUNDING = "android.hardware.bluetooth_le.channel_sounding";
    public static final java.lang.String FEATURE_BROADCAST_RADIO = "android.hardware.broadcastradio";
    public static final java.lang.String FEATURE_CAMERA_AR = "android.hardware.camera.ar";
    public static final java.lang.String FEATURE_CAMERA_CAPABILITY_MANUAL_POST_PROCESSING = "android.hardware.camera.capability.manual_post_processing";
    public static final java.lang.String FEATURE_CAMERA_CAPABILITY_MANUAL_SENSOR = "android.hardware.camera.capability.manual_sensor";
    public static final java.lang.String FEATURE_CAMERA_CAPABILITY_RAW = "android.hardware.camera.capability.raw";
    public static final java.lang.String FEATURE_CAMERA_CONCURRENT = "android.hardware.camera.concurrent";
    public static final java.lang.String FEATURE_CAMERA_EXTERNAL = "android.hardware.camera.external";
    public static final java.lang.String FEATURE_CAMERA_LEVEL_FULL = "android.hardware.camera.level.full";
    public static final java.lang.String FEATURE_CANT_SAVE_STATE = "android.software.cant_save_state";
    public static final java.lang.String FEATURE_CAR_DISPLAY_COMPATIBILITY = "android.software.car.display_compatibility";
    public static final java.lang.String FEATURE_CAR_SPLITSCREEN_MULTITASKING = "android.software.car.splitscreen_multitasking";
    public static final java.lang.String FEATURE_CAR_TEMPLATES_HOST = "android.software.car.templates_host";
    public static final java.lang.String FEATURE_CAR_TEMPLATES_HOST_MEDIA = "android.software.car.templates_host.media";
    public static final java.lang.String FEATURE_COMMUNAL_MODE = "android.software.communal_mode";
    public static final java.lang.String FEATURE_COMPANION_DEVICE_SETUP = "android.software.companion_device_setup";
    public static final java.lang.String FEATURE_CONNECTION_SERVICE = "android.software.connectionservice";
    public static final java.lang.String FEATURE_CONSUMER_IR = "android.hardware.consumerir";
    public static final java.lang.String FEATURE_CONTEXTUAL_SEARCH_HELPER = "android.software.contextualsearch";
    public static final java.lang.String FEATURE_CONTEXT_HUB = "android.hardware.context_hub";
    public static final java.lang.String FEATURE_CONTROLS = "android.software.controls";
    public static final java.lang.String FEATURE_CREDENTIALS = "android.software.credentials";
    public static final java.lang.String FEATURE_CTS = "android.software.cts";
    public static final java.lang.String FEATURE_DEVICE_ID_ATTESTATION = "android.software.device_id_attestation";
    public static final java.lang.String FEATURE_DEVICE_LOCK = "android.software.device_lock";
    public static final java.lang.String FEATURE_DEVICE_UNIQUE_ATTESTATION = "android.hardware.device_unique_attestation";
    public static final java.lang.String FEATURE_DREAM_OVERLAY = "android.software.dream_overlay";
    public static final java.lang.String FEATURE_EROFS = "android.software.erofs";
    public static final java.lang.String FEATURE_EROFS_LEGACY = "android.software.erofs_legacy";
    public static final java.lang.String FEATURE_ETHERNET = "android.hardware.ethernet";
    public static final java.lang.String FEATURE_EXPANDED_PICTURE_IN_PICTURE = "android.software.expanded_picture_in_picture";
    public static final java.lang.String FEATURE_FACE = "android.hardware.biometrics.face";
    public static final java.lang.String FEATURE_FAKETOUCH_MULTITOUCH_DISTINCT = "android.hardware.faketouch.multitouch.distinct";
    public static final java.lang.String FEATURE_FAKETOUCH_MULTITOUCH_JAZZHAND = "android.hardware.faketouch.multitouch.jazzhand";
    public static final java.lang.String FEATURE_FELICA = "android.hardware.felica";
    public static final java.lang.String FEATURE_FILE_BASED_ENCRYPTION = "android.software.file_based_encryption";
    public static final java.lang.String FEATURE_GAME_SERVICE = "android.software.game_service";
    public static final java.lang.String FEATURE_HDMI_CEC = "android.hardware.hdmi.cec";
    public static final java.lang.String FEATURE_HIFI_SENSORS = "android.hardware.sensor.hifi_sensors";
    public static final java.lang.String FEATURE_IDENTITY_CREDENTIAL_HARDWARE = "android.hardware.identity_credential";
    public static final java.lang.String FEATURE_IDENTITY_CREDENTIAL_HARDWARE_DIRECT_ACCESS = "android.hardware.identity_credential_direct_access";
    public static final java.lang.String FEATURE_INCREMENTAL_DELIVERY = "android.software.incremental_delivery";
    public static final java.lang.String FEATURE_IPSEC_TUNNELS = "android.software.ipsec_tunnels";
    public static final java.lang.String FEATURE_IPSEC_TUNNEL_MIGRATION = "android.software.ipsec_tunnel_migration";
    public static final java.lang.String FEATURE_IRIS = "android.hardware.biometrics.iris";
    public static final java.lang.String FEATURE_KEYSTORE_APP_ATTEST_KEY = "android.hardware.keystore.app_attest_key";
    public static final java.lang.String FEATURE_KEYSTORE_LIMITED_USE_KEY = "android.hardware.keystore.limited_use_key";
    public static final java.lang.String FEATURE_KEYSTORE_SINGLE_USE_KEY = "android.hardware.keystore.single_use_key";
    public static final java.lang.String FEATURE_LEANBACK_ONLY = "android.software.leanback_only";
    public static final java.lang.String FEATURE_LIVE_TV = "android.software.live_tv";
    public static final java.lang.String FEATURE_LOCATION_NETWORK = "android.hardware.location.network";
    public static final java.lang.String FEATURE_LOWPAN = "android.hardware.lowpan";
    public static final java.lang.String FEATURE_MANAGED_PROFILES = "android.software.managed_users";
    public static final java.lang.String FEATURE_MIDI = "android.software.midi";
    public static final java.lang.String FEATURE_NFC_ANY = "android.hardware.nfc.any";
    public static final java.lang.String FEATURE_NFC_BEAM = "android.sofware.nfc.beam";
    public static final java.lang.String FEATURE_NFC_CHARGING = "android.hardware.nfc.charging";
    public static final java.lang.String FEATURE_NFC_HCE = "android.hardware.nfc.hce";
    public static final java.lang.String FEATURE_NFC_HOST_CARD_EMULATION = "android.hardware.nfc.hce";
    public static final java.lang.String FEATURE_NFC_HOST_CARD_EMULATION_NFCF = "android.hardware.nfc.hcef";
    public static final java.lang.String FEATURE_NFC_OFF_HOST_CARD_EMULATION_ESE = "android.hardware.nfc.ese";
    public static final java.lang.String FEATURE_NFC_OFF_HOST_CARD_EMULATION_UICC = "android.hardware.nfc.uicc";
    public static final java.lang.String FEATURE_OPENGLES_DEQP_LEVEL = "android.software.opengles.deqp.level";
    public static final java.lang.String FEATURE_RAM_LOW = "android.hardware.ram.low";
    public static final java.lang.String FEATURE_RAM_NORMAL = "android.hardware.ram.normal";
    public static final java.lang.String FEATURE_REBOOT_ESCROW = "android.hardware.reboot_escrow";
    public static final java.lang.String FEATURE_ROTARY_ENCODER_LOW_RES = "android.hardware.rotaryencoder.lowres";
    public static final java.lang.String FEATURE_SDK_SANDBOX_WORK_PROFILE_INSTALL = "android.software.sdksandbox.sdk_install_work_profile";
    public static final java.lang.String FEATURE_SECURELY_REMOVES_USERS = "android.software.securely_removes_users";
    public static final java.lang.String FEATURE_SECURITY_MODEL_COMPATIBLE = "android.hardware.security.model.compatible";
    public static final java.lang.String FEATURE_SENSOR_ACCELEROMETER_LIMITED_AXES = "android.hardware.sensor.accelerometer_limited_axes";
    public static final java.lang.String FEATURE_SENSOR_ACCELEROMETER_LIMITED_AXES_UNCALIBRATED = "android.hardware.sensor.accelerometer_limited_axes_uncalibrated";
    public static final java.lang.String FEATURE_SENSOR_AMBIENT_TEMPERATURE = "android.hardware.sensor.ambient_temperature";
    public static final java.lang.String FEATURE_SENSOR_BAROMETER = "android.hardware.sensor.barometer";
    public static final java.lang.String FEATURE_SENSOR_DYNAMIC_HEAD_TRACKER = "android.hardware.sensor.dynamic.head_tracker";
    public static final java.lang.String FEATURE_SENSOR_GYROSCOPE_LIMITED_AXES = "android.hardware.sensor.gyroscope_limited_axes";
    public static final java.lang.String FEATURE_SENSOR_GYROSCOPE_LIMITED_AXES_UNCALIBRATED = "android.hardware.sensor.gyroscope_limited_axes_uncalibrated";
    public static final java.lang.String FEATURE_SENSOR_HEADING = "android.hardware.sensor.heading";
    public static final java.lang.String FEATURE_SENSOR_HEART_RATE = "android.hardware.sensor.heartrate";
    public static final java.lang.String FEATURE_SENSOR_HEART_RATE_ECG = "android.hardware.sensor.heartrate.ecg";
    public static final java.lang.String FEATURE_SENSOR_HINGE_ANGLE = "android.hardware.sensor.hinge_angle";
    public static final java.lang.String FEATURE_SENSOR_LIGHT = "android.hardware.sensor.light";
    public static final java.lang.String FEATURE_SENSOR_PROXIMITY = "android.hardware.sensor.proximity";
    public static final java.lang.String FEATURE_SENSOR_RELATIVE_HUMIDITY = "android.hardware.sensor.relative_humidity";
    public static final java.lang.String FEATURE_SENSOR_STEP_COUNTER = "android.hardware.sensor.stepcounter";
    public static final java.lang.String FEATURE_SENSOR_STEP_DETECTOR = "android.hardware.sensor.stepdetector";
    public static final java.lang.String FEATURE_SE_OMAPI_ESE = "android.hardware.se.omapi.ese";
    public static final java.lang.String FEATURE_SE_OMAPI_SD = "android.hardware.se.omapi.sd";
    public static final java.lang.String FEATURE_SE_OMAPI_UICC = "android.hardware.se.omapi.uicc";
    public static final java.lang.String FEATURE_SIP = "android.software.sip";
    public static final java.lang.String FEATURE_SIP_VOIP = "android.software.sip.voip";
    public static final java.lang.String FEATURE_SLICES_DISABLED = "android.software.slices_disabled";
    public static final java.lang.String FEATURE_TELECOM = "android.software.telecom";
    public static final java.lang.String FEATURE_TELEPHONY_CALLING = "android.hardware.telephony.calling";
    public static final java.lang.String FEATURE_TELEPHONY_CARRIERLOCK = "android.hardware.telephony.carrierlock";
    public static final java.lang.String FEATURE_TELEPHONY_CDMA = "android.hardware.telephony.cdma";
    public static final java.lang.String FEATURE_TELEPHONY_DATA = "android.hardware.telephony.data";
    public static final java.lang.String FEATURE_TELEPHONY_EUICC = "android.hardware.telephony.euicc";
    public static final java.lang.String FEATURE_TELEPHONY_EUICC_MEP = "android.hardware.telephony.euicc.mep";
    public static final java.lang.String FEATURE_TELEPHONY_GSM = "android.hardware.telephony.gsm";
    public static final java.lang.String FEATURE_TELEPHONY_IMS = "android.hardware.telephony.ims";
    public static final java.lang.String FEATURE_TELEPHONY_IMS_SINGLE_REGISTRATION = "android.hardware.telephony.ims.singlereg";
    public static final java.lang.String FEATURE_TELEPHONY_MBMS = "android.hardware.telephony.mbms";
    public static final java.lang.String FEATURE_TELEPHONY_MESSAGING = "android.hardware.telephony.messaging";
    public static final java.lang.String FEATURE_TELEPHONY_RADIO_ACCESS = "android.hardware.telephony.radio.access";
    public static final java.lang.String FEATURE_TELEPHONY_SATELLITE = "android.hardware.telephony.satellite";
    public static final java.lang.String FEATURE_TELEPHONY_SUBSCRIPTION = "android.hardware.telephony.subscription";
    public static final java.lang.String FEATURE_THREAD_NETWORK = "android.hardware.thread_network";
    public static final java.lang.String FEATURE_TUNER = "android.hardware.tv.tuner";
    public static final java.lang.String FEATURE_USB_ACCESSORY = "android.hardware.usb.accessory";
    public static final java.lang.String FEATURE_UWB = "android.hardware.uwb";
    public static final java.lang.String FEATURE_VERIFIED_BOOT = "android.software.verified_boot";
    public static final java.lang.String FEATURE_VIRTUALIZATION_FRAMEWORK = "android.software.virtualization_framework";
    public static final java.lang.String FEATURE_VOICE_RECOGNIZERS = "android.software.voice_recognizers";
    public static final java.lang.String FEATURE_VR_HEADTRACKING = "android.hardware.vr.headtracking";
    public static final java.lang.String FEATURE_VR_MODE = "android.software.vr.mode";
    public static final java.lang.String FEATURE_VR_MODE_HIGH_PERFORMANCE = "android.hardware.vr.high_performance";
    public static final java.lang.String FEATURE_VULKAN_DEQP_LEVEL = "android.software.vulkan.deqp.level";
    public static final java.lang.String FEATURE_VULKAN_HARDWARE_COMPUTE = "android.hardware.vulkan.compute";
    public static final java.lang.String FEATURE_WALLET_LOCATION_BASED_SUGGESTIONS = "android.software.wallet_location_based_suggestions";
    public static final java.lang.String FEATURE_WIFI_AWARE = "android.hardware.wifi.aware";
    public static final java.lang.String FEATURE_WIFI_DIRECT = "android.hardware.wifi.direct";
    public static final java.lang.String FEATURE_WIFI_PASSPOINT = "android.hardware.wifi.passpoint";
    public static final java.lang.String FEATURE_WIFI_RTT = "android.hardware.wifi.rtt";
    public static final java.lang.String FEATURE_WINDOW_MAGNIFICATION = "android.software.window_magnification";
    public static final java.lang.String FEATURE_XR_API_OPENXR = "android.software.xr.api.openxr";
    public static final java.lang.String FEATURE_XR_API_SPATIAL = "android.software.xr.api.spatial";
    public static final java.lang.String FEATURE_XR_INPUT_CONTROLLER = "android.hardware.xr.input.controller";
    public static final java.lang.String FEATURE_XR_INPUT_EYE_TRACKING = "android.hardware.xr.input.eye_tracking";
    public static final java.lang.String FEATURE_XR_INPUT_HAND_TRACKING = "android.hardware.xr.input.hand_tracking";
    public static final java.lang.String FEATURE_XR_PERIPHERAL = "android.hardware.type.xr_peripheral";
    public static final long FILTER_APPLICATION_QUERY = 135549675L;
    public static final int FLAGS_PERMISSION_RESERVED_PERMISSION_CONTROLLER = -268435456;
    public static final int FLAGS_PERMISSION_RESTRICTION_ANY_EXEMPT = 14336;
    public static final int FLAG_PERMISSION_APPLY_RESTRICTION = 16384;
    public static final int FLAG_PERMISSION_AUTO_REVOKED = 131072;
    public static final int FLAG_PERMISSION_GRANTED_BY_DEFAULT = 32;
    public static final int FLAG_PERMISSION_GRANTED_BY_ROLE = 32768;
    public static final int FLAG_PERMISSION_ONE_TIME = 65536;
    public static final int FLAG_PERMISSION_POLICY_FIXED = 4;
    public static final int FLAG_PERMISSION_RESTRICTION_INSTALLER_EXEMPT = 2048;
    public static final int FLAG_PERMISSION_RESTRICTION_SYSTEM_EXEMPT = 4096;
    public static final int FLAG_PERMISSION_RESTRICTION_UPGRADE_EXEMPT = 8192;
    public static final int FLAG_PERMISSION_REVIEW_REQUIRED = 64;
    public static final int FLAG_PERMISSION_REVOKED_COMPAT = 8;
    public static final int FLAG_PERMISSION_REVOKE_ON_UPGRADE = 8;
    public static final int FLAG_PERMISSION_REVOKE_WHEN_REQUESTED = 128;
    public static final int FLAG_PERMISSION_SELECTED_LOCATION_ACCURACY = 524288;
    public static final int FLAG_PERMISSION_SYSTEM_FIXED = 16;
    public static final int FLAG_PERMISSION_USER_FIXED = 2;
    public static final int FLAG_PERMISSION_USER_SENSITIVE_WHEN_DENIED = 512;
    public static final int FLAG_PERMISSION_USER_SENSITIVE_WHEN_GRANTED = 256;
    public static final int FLAG_PERMISSION_USER_SET = 1;
    public static final int FLAG_PERMISSION_WHITELIST_INSTALLER = 2;
    public static final int FLAG_PERMISSION_WHITELIST_SYSTEM = 1;
    public static final int FLAG_PERMISSION_WHITELIST_UPGRADE = 4;
    public static final int FLAG_SUSPEND_QUARANTINED = 1;
    public static final int GET_ATTRIBUTIONS = -2147483648;
    public static final long GET_ATTRIBUTIONS_LONG = 2147483648L;
    public static final int GET_DISABLED_UNTIL_USED_COMPONENTS = 32768;
    public static final int INSTALL_ACTIVATION_FAILED = -128;
    public static final int INSTALL_ALLOCATE_AGGRESSIVE = 32768;
    public static final int INSTALL_ALLOW_DOWNGRADE = 1048576;
    public static final int INSTALL_ALLOW_TEST = 4;
    public static final int INSTALL_ALL_USERS = 64;
    public static final int INSTALL_ALL_WHITELIST_RESTRICTED_PERMISSIONS = 4194304;
    public static final int INSTALL_APEX = 131072;
    public static final int INSTALL_ARCHIVED = 134217728;
    public static final int INSTALL_BYPASS_LOW_TARGET_SDK_BLOCK = 16777216;
    public static final int INSTALL_DEVELOPMENT_FORCE_NON_STAGED_APEX_UPDATE = 1;
    public static final int INSTALL_DISABLE_ALLOWED_APEX_UPDATE_CHECK = 8388608;
    public static final int INSTALL_DISABLE_VERIFICATION = 524288;
    public static final int INSTALL_DONT_KILL_APP = 4096;
    public static final int INSTALL_ENABLE_ROLLBACK = 262144;
    public static final int INSTALL_FAILED_ABORTED = -115;
    public static final int INSTALL_FAILED_ALREADY_EXISTS = -1;
    public static final int INSTALL_FAILED_BAD_DEX_METADATA = -117;
    public static final int INSTALL_FAILED_BAD_PERMISSION_GROUP = -127;
    public static final int INSTALL_FAILED_BAD_SIGNATURE = -118;
    public static final int INSTALL_FAILED_CONFLICTING_PROVIDER = -13;
    public static final int INSTALL_FAILED_CONTAINER_ERROR = -18;
    public static final int INSTALL_FAILED_CPU_ABI_INCOMPATIBLE = -16;
    public static final int INSTALL_FAILED_DEPRECATED_SDK_VERSION = -29;
    public static final int INSTALL_FAILED_DEXOPT = -11;
    public static final int INSTALL_FAILED_DUPLICATE_PACKAGE = -5;
    public static final int INSTALL_FAILED_DUPLICATE_PERMISSION = -112;
    public static final int INSTALL_FAILED_DUPLICATE_PERMISSION_GROUP = -126;
    public static final int INSTALL_FAILED_INSUFFICIENT_STORAGE = -4;
    public static final int INSTALL_FAILED_INTERNAL_ERROR = -110;
    public static final int INSTALL_FAILED_INVALID_APK = -2;
    public static final int INSTALL_FAILED_INVALID_INSTALL_LOCATION = -19;
    public static final int INSTALL_FAILED_INVALID_URI = -3;
    public static final int INSTALL_FAILED_MEDIA_UNAVAILABLE = -20;
    public static final int INSTALL_FAILED_MISSING_FEATURE = -17;
    public static final int INSTALL_FAILED_MISSING_SHARED_LIBRARY = -9;
    public static final int INSTALL_FAILED_MISSING_SPLIT = -28;
    public static final int INSTALL_FAILED_MULTIPACKAGE_INCONSISTENCY = -120;
    public static final int INSTALL_FAILED_MULTI_ARCH_NOT_MATCH_ALL_NATIVE_ABIS = -131;
    public static final int INSTALL_FAILED_NEWER_SDK = -14;
    public static final int INSTALL_FAILED_NO_MATCHING_ABIS = -113;
    public static final int INSTALL_FAILED_NO_SHARED_USER = -6;
    public static final int INSTALL_FAILED_OLDER_SDK = -12;
    public static final int INSTALL_FAILED_OTHER_STAGED_SESSION_IN_PROGRESS = -119;
    public static final int INSTALL_FAILED_PACKAGE_CHANGED = -23;
    public static final int INSTALL_FAILED_PERMISSION_MODEL_DOWNGRADE = -26;
    public static final int INSTALL_FAILED_PRE_APPROVAL_NOT_AVAILABLE = -129;
    public static final int INSTALL_FAILED_PROCESS_NOT_DEFINED = -122;
    public static final int INSTALL_FAILED_REPLACE_COULDNT_DELETE = -10;
    public static final int INSTALL_FAILED_SANDBOX_VERSION_DOWNGRADE = -27;
    public static final int INSTALL_FAILED_SESSION_INVALID = -116;
    public static final int INSTALL_FAILED_SHARED_LIBRARY_BAD_CERTIFICATE_DIGEST = -130;
    public static final int INSTALL_FAILED_SHARED_USER_INCOMPATIBLE = -8;
    public static final int INSTALL_FAILED_TEST_ONLY = -15;
    public static final int INSTALL_FAILED_UID_CHANGED = -24;
    public static final int INSTALL_FAILED_UPDATE_INCOMPATIBLE = -7;
    public static final int INSTALL_FAILED_USER_RESTRICTED = -111;
    public static final int INSTALL_FAILED_VERIFICATION_FAILURE = -22;
    public static final int INSTALL_FAILED_VERIFICATION_TIMEOUT = -21;
    public static final int INSTALL_FAILED_VERSION_DOWNGRADE = -25;
    public static final int INSTALL_FAILED_WRONG_INSTALLED_VERSION = -121;
    public static final int INSTALL_FORCE_PERMISSION_PROMPT = 1024;
    public static final int INSTALL_FORCE_VOLUME_UUID = 512;
    public static final int INSTALL_FROM_ADB = 32;
    public static final int INSTALL_FROM_MANAGED_USER_OR_PROFILE = 67108864;
    public static final int INSTALL_FULL_APP = 16384;
    public static final int INSTALL_GRANT_ALL_REQUESTED_PERMISSIONS = 256;
    public static final int INSTALL_IGNORE_DEXOPT_PROFILE = 268435456;
    public static final int INSTALL_INSTANT_APP = 2048;
    public static final int INSTALL_INTERNAL = 16;
    public static final int INSTALL_PARSE_FAILED_BAD_MANIFEST = -101;
    public static final int INSTALL_PARSE_FAILED_BAD_PACKAGE_NAME = -106;
    public static final int INSTALL_PARSE_FAILED_BAD_SHARED_USER_ID = -107;
    public static final int INSTALL_PARSE_FAILED_CERTIFICATE_ENCODING = -105;
    public static final int INSTALL_PARSE_FAILED_INCONSISTENT_CERTIFICATES = -104;
    public static final int INSTALL_PARSE_FAILED_MANIFEST_EMPTY = -109;
    public static final int INSTALL_PARSE_FAILED_MANIFEST_MALFORMED = -108;
    public static final int INSTALL_PARSE_FAILED_NOT_APK = -100;
    public static final int INSTALL_PARSE_FAILED_NO_CERTIFICATES = -103;
    public static final int INSTALL_PARSE_FAILED_RESOURCES_ARSC_COMPRESSED = -124;
    public static final int INSTALL_PARSE_FAILED_SKIPPED = -125;
    public static final int INSTALL_PARSE_FAILED_UNEXPECTED_EXCEPTION = -102;
    public static final int INSTALL_REASON_DEVICE_RESTORE = 2;
    public static final int INSTALL_REASON_DEVICE_SETUP = 3;
    public static final int INSTALL_REASON_POLICY = 1;
    public static final int INSTALL_REASON_ROLLBACK = 5;
    public static final int INSTALL_REASON_USER = 4;
    public static final int INSTALL_REPLACE_EXISTING = 2;
    public static final int INSTALL_REQUEST_DOWNGRADE = 128;
    public static final int INSTALL_REQUEST_UPDATE_OWNERSHIP = 33554432;
    public static final int INSTALL_SCENARIO_BULK = 2;
    public static final int INSTALL_SCENARIO_BULK_SECONDARY = 3;
    public static final int INSTALL_SCENARIO_DEFAULT = 0;
    public static final int INSTALL_SCENARIO_FAST = 1;
    public static final int INSTALL_STAGED = 2097152;
    public static final int INSTALL_SUCCEEDED = 1;
    public static final int INSTALL_UNARCHIVE = 1073741824;
    public static final int INSTALL_UNARCHIVE_DRAFT = 536870912;
    public static final int INSTALL_UNKNOWN = 0;
    public static final int INSTALL_VIRTUAL_PRELOAD = 65536;
    public static final int INTENT_FILTER_DOMAIN_VERIFICATION_STATUS_ALWAYS = 2;
    public static final int INTENT_FILTER_DOMAIN_VERIFICATION_STATUS_ALWAYS_ASK = 4;
    public static final int INTENT_FILTER_DOMAIN_VERIFICATION_STATUS_ASK = 1;
    public static final int INTENT_FILTER_DOMAIN_VERIFICATION_STATUS_NEVER = 3;
    public static final int INTENT_FILTER_DOMAIN_VERIFICATION_STATUS_UNDEFINED = 0;
    public static final int INTENT_FILTER_VERIFICATION_FAILURE = -1;
    public static final int INTENT_FILTER_VERIFICATION_SUCCESS = 1;
    public static final int MASK_PERMISSION_FLAGS = 255;
    public static final int MASK_PERMISSION_FLAGS_ALL = 261119;
    public static final int MATCH_ANY_USER = 4194304;
    public static final long MATCH_ARCHIVED_PACKAGES = 4294967296L;
    public static final int MATCH_CLONE_PROFILE = 536870912;
    public static final long MATCH_CLONE_PROFILE_LONG = 17179869184L;
    public static final int MATCH_DEBUG_TRIAGED_MISSING = 268435456;
    public static final int MATCH_EXPLICITLY_VISIBLE_ONLY = 33554432;
    public static final int MATCH_FACTORY_ONLY = 2097152;
    public static final int MATCH_HIDDEN_UNTIL_INSTALLED_COMPONENTS = 536870912;
    public static final int MATCH_INSTANT = 8388608;
    public static final int MATCH_KNOWN_PACKAGES = 4202496;
    public static final long MATCH_QUARANTINED_COMPONENTS = 8589934592L;
    public static final int MATCH_STATIC_SHARED_AND_SDK_LIBRARIES = 67108864;
    public static final int MATCH_VISIBLE_TO_INSTANT_APP_ONLY = 16777216;
    public static final long MAXIMUM_VERIFICATION_TIMEOUT = 3600000L;
    public static final int MODULE_APEX_NAME = 1;
    public static final int MOVE_EXTERNAL_MEDIA = 2;
    public static final int MOVE_FAILED_3RD_PARTY_NOT_ALLOWED_ON_INTERNAL = -9;
    public static final int MOVE_FAILED_DEVICE_ADMIN = -8;
    public static final int MOVE_FAILED_DOESNT_EXIST = -2;
    public static final int MOVE_FAILED_INSUFFICIENT_STORAGE = -1;
    public static final int MOVE_FAILED_INTERNAL_ERROR = -6;
    public static final int MOVE_FAILED_INVALID_LOCATION = -5;
    public static final int MOVE_FAILED_LOCKED_USER = -10;
    public static final int MOVE_FAILED_OPERATION_PENDING = -7;
    public static final int MOVE_FAILED_SYSTEM_PACKAGE = -3;
    public static final int MOVE_INTERNAL = 1;
    public static final int MOVE_SUCCEEDED = -100;
    public static final int NOTIFY_PACKAGE_USE_ACTIVITY = 0;
    public static final int NOTIFY_PACKAGE_USE_BACKUP = 5;
    public static final int NOTIFY_PACKAGE_USE_BROADCAST_RECEIVER = 3;
    public static final int NOTIFY_PACKAGE_USE_CONTENT_PROVIDER = 4;
    public static final int NOTIFY_PACKAGE_USE_CROSS_PACKAGE = 6;
    public static final int NOTIFY_PACKAGE_USE_FOREGROUND_SERVICE = 2;
    public static final int NOTIFY_PACKAGE_USE_INSTRUMENTATION = 7;
    public static final int NOTIFY_PACKAGE_USE_REASONS_COUNT = 8;
    public static final int NOTIFY_PACKAGE_USE_SERVICE = 1;
    public static final int NO_NATIVE_LIBRARIES = -114;
    public static final int ONLY_IF_NO_MATCH_FOUND = 4;
    public static final java.lang.String PROPERTY_ALLOW_ADB_BACKUP = "android.backup.ALLOW_ADB_BACKUP";
    public static final java.lang.String PROPERTY_ANDROID_SAFETY_LABEL = "android.content.PROPERTY_ANDROID_SAFETY_LABEL";
    public static final java.lang.String PROPERTY_COMPAT_OVERRIDE_LANDSCAPE_TO_PORTRAIT = "android.camera.PROPERTY_COMPAT_OVERRIDE_LANDSCAPE_TO_PORTRAIT";
    public static final java.lang.String PROPERTY_LEGACY_UPDATE_OWNERSHIP_DENYLIST = "android.app.PROPERTY_LEGACY_UPDATE_OWNERSHIP_DENYLIST";
    public static final java.lang.String PROPERTY_MEDIA_CAPABILITIES = "android.media.PROPERTY_MEDIA_CAPABILITIES";
    public static final java.lang.String PROPERTY_NO_APP_DATA_STORAGE = "android.internal.PROPERTY_NO_APP_DATA_STORAGE";
    public static final java.lang.String PROPERTY_SELF_CERTIFIED_NETWORK_CAPABILITIES = "android.net.PROPERTY_SELF_CERTIFIED_NETWORK_CAPABILITIES";
    public static final java.lang.String PROPERTY_SPECIAL_USE_FGS_SUBTYPE = "android.app.PROPERTY_SPECIAL_USE_FGS_SUBTYPE";
    public static final java.lang.String PROPERTY_USE_RESTRICTED_BACKUP_MODE = "android.app.backup.PROPERTY_USE_RESTRICTED_BACKUP_MODE";
    public static final int RESTRICTION_HIDE_FROM_SUGGESTIONS = 1;
    public static final int RESTRICTION_HIDE_NOTIFICATIONS = 2;
    public static final int RESTRICTION_NONE = 0;
    public static final int ROLLBACK_DATA_POLICY_RESTORE = 0;
    public static final int ROLLBACK_DATA_POLICY_RETAIN = 2;
    public static final int ROLLBACK_DATA_POLICY_WIPE = 1;
    public static final int ROLLBACK_USER_IMPACT_HIGH = 1;
    public static final int ROLLBACK_USER_IMPACT_LOW = 0;
    public static final int ROLLBACK_USER_IMPACT_ONLY_MANUAL = 2;
    public static final int SDK_FEATURE_COUNT = 188;
    public static final int SIGNATURE_FIRST_NOT_SIGNED = -1;
    public static final int SIGNATURE_NEITHER_SIGNED = 1;
    public static final int SIGNATURE_SECOND_NOT_SIGNED = -2;
    public static final int SKIP_CURRENT_PROFILE = 2;
    public static final int SYSTEM_APP_STATE_HIDDEN_UNTIL_INSTALLED_HIDDEN = 0;
    public static final int SYSTEM_APP_STATE_HIDDEN_UNTIL_INSTALLED_VISIBLE = 1;
    public static final int SYSTEM_APP_STATE_INSTALLED = 2;
    public static final int SYSTEM_APP_STATE_UNINSTALLED = 3;
    public static final java.lang.String SYSTEM_SHARED_LIBRARY_SERVICES = "android.ext.services";
    public static final java.lang.String SYSTEM_SHARED_LIBRARY_SHARED = "android.ext.shared";
    public static java.util.List TRUST_ALL;
    public static java.util.List TRUST_NONE;
    public static final int TYPE_ACTIVITY = 1;
    public static final int TYPE_APPLICATION = 5;
    public static final int TYPE_PROVIDER = 4;
    public static final int TYPE_RECEIVER = 2;
    public static final int TYPE_SERVICE = 3;
    public static final int TYPE_UNKNOWN = 0;
    public static final int UNINSTALL_REASON_UNKNOWN = 0;
    public static final int UNINSTALL_REASON_USER_TYPE = 1;
    public static final int USER_MIN_ASPECT_RATIO_16_9 = 4;
    public static final int USER_MIN_ASPECT_RATIO_3_2 = 5;
    public static final int USER_MIN_ASPECT_RATIO_4_3 = 3;
    public static final int USER_MIN_ASPECT_RATIO_APP_DEFAULT = 7;
    public static final int USER_MIN_ASPECT_RATIO_DISPLAY_SIZE = 2;
    public static final int USER_MIN_ASPECT_RATIO_FULLSCREEN = 6;
    public static final int USER_MIN_ASPECT_RATIO_SPLIT_SCREEN = 1;
    public static final int USER_MIN_ASPECT_RATIO_UNSET = 0;
    public static final int VERIFICATION_ALLOW_WITHOUT_SUFFICIENT = 2;
    public static final int VERIFICATION_REJECT = -1;
    public static final int VERSION_CODE_HIGHEST = -1;
    public static void corkPackageInfoCache() {}
    public static int deleteStatusToPublicStatus(int p0) { return 0; }
    public static java.lang.String deleteStatusToString(int p0) { return null; }
    public static java.lang.String deleteStatusToString(int p0, java.lang.String p1) { return null; }
    public static void disableApplicationInfoCache() {}
    public static void disablePackageInfoCache() {}
    public static android.content.pm.ApplicationInfo getApplicationInfoAsUserCached(java.lang.String p0, long p1, int p2) { return null; }
    public static android.content.pm.PackageInfo getPackageInfoAsUserCached(java.lang.String p0, long p1, int p2) { return null; }
    public static android.content.pm.SigningInfo getVerifiedSigningInfo(java.lang.String p0, int p1) { return null; }
    public static int installStatusToPublicStatus(int p0) { return 0; }
    public static java.lang.String installStatusToString(int p0) { return null; }
    public static java.lang.String installStatusToString(int p0, java.lang.String p1) { return null; }
    public static void invalidatePackageInfoCache() {}
    public static boolean isMoveStatusFinished(int p0) { return false; }
    public static int maybeGetSdkFeatureIndex(java.lang.String p0) { return 0; }
    public static java.lang.String permissionFlagToString(int p0) { return null; }
    public static void uncorkPackageInfoCache() {}
    public void addCrossProfileIntentFilter(android.content.IntentFilter p0, int p1, int p2, int p3) {}
    public void addOnPermissionsChangeListener(android.content.pm.PackageManager.OnPermissionsChangedListener p0) {}
    public void addPackageToPreferred(java.lang.String p0) {}
    public boolean addPermission(android.content.pm.PermissionInfo p0) { return false; }
    public boolean addPermissionAsync(android.content.pm.PermissionInfo p0) { return false; }
    public void addPreferredActivity(android.content.IntentFilter p0, int p1, android.content.ComponentName[] p2, android.content.ComponentName p3) {}
    public void addPreferredActivityAsUser(android.content.IntentFilter p0, int p1, android.content.ComponentName[] p2, android.content.ComponentName p3, int p4) {}
    public void addUniquePreferredActivity(android.content.IntentFilter p0, int p1, android.content.ComponentName[] p2, android.content.ComponentName p3) {}
    public boolean addWhitelistedRestrictedPermission(java.lang.String p0, java.lang.String p1, int p2) { return false; }
    public boolean arePermissionsIndividuallyControlled() { return false; }
    public android.content.Intent buildRequestPermissionsIntent(java.lang.String[] p0) { return null; }
    public boolean canPackageQuery(java.lang.String p0, java.lang.String p1) { return false; }
    public boolean[] canPackageQuery(java.lang.String p0, java.lang.String[] p1) { return null; }
    public boolean canUserUninstall(java.lang.String p0, android.os.UserHandle p1) { return false; }
    public java.lang.String[] canonicalToCurrentPackageNames(java.lang.String[] p0) { return null; }
    public void clearCrossProfileIntentFilters(int p0) {}
    public void clearInstantAppCookie() {}
    public void clearPackagePreferredActivities(java.lang.String p0) {}
    public java.lang.String[] currentToCanonicalPackageNames(java.lang.String[] p0) { return null; }
    public void deletePackage(java.lang.String p0, android.content.pm.IPackageDeleteObserver p1, int p2) {}
    public void deletePackageAsUser(java.lang.String p0, android.content.pm.IPackageDeleteObserver p1, int p2, int p3) {}
    public void extendVerificationTimeout(int p0, int p1, long p2) {}
    public android.content.res.TypedArray extractPackageItemInfoAttributes(android.content.pm.PackageItemInfo p0, java.lang.String p1, java.lang.String p2, int[] p3) { return null; }
    public void flushPackageRestrictionsAsUser(int p0) {}
    public void freeStorage(long p0, android.content.IntentSender p1) {}
    public void freeStorage(java.lang.String p0, long p1, android.content.IntentSender p2) {}
    public android.graphics.drawable.Drawable getActivityBanner(android.content.ComponentName p0) { return null; }
    public android.graphics.drawable.Drawable getActivityBanner(android.content.Intent p0) { return null; }
    public android.graphics.drawable.Drawable getActivityIcon(android.content.Intent p0) { return null; }
    public android.graphics.drawable.Drawable getActivityLogo(android.content.ComponentName p0) { return null; }
    public android.graphics.drawable.Drawable getActivityLogo(android.content.Intent p0) { return null; }
    public java.util.List getAllIntentFilters(java.lang.String p0) { return new java.util.ArrayList(); }
    public java.util.List getAllPermissionGroups(int p0) { return new java.util.ArrayList(); }
    public android.os.PersistableBundle getAppMetadata(java.lang.String p0) { return null; }
    public int getAppMetadataSource(java.lang.String p0) { return 0; }
    public java.lang.String getAppPredictionServicePackageName() { return null; }
    public android.graphics.drawable.Drawable getApplicationBanner(android.content.pm.ApplicationInfo p0) { return null; }
    public android.graphics.drawable.Drawable getApplicationBanner(java.lang.String p0) { return null; }
    public boolean getApplicationHiddenSettingAsUser(java.lang.String p0, android.os.UserHandle p1) { return false; }
    public android.content.pm.ApplicationInfo getApplicationInfoAsUser(java.lang.String p0, int p1, int p2) { return null; }
    public android.content.pm.ApplicationInfo getApplicationInfoAsUser(java.lang.String p0, int p1, android.os.UserHandle p2) { return null; }
    public android.content.pm.ApplicationInfo getApplicationInfoAsUser(java.lang.String p0, android.content.pm.PackageManager.ApplicationInfoFlags p1, int p2) { return null; }
    public android.content.pm.ApplicationInfo getApplicationInfoAsUser(java.lang.String p0, android.content.pm.PackageManager.ApplicationInfoFlags p1, android.os.UserHandle p2) { return null; }
    public android.graphics.drawable.Drawable getApplicationLogo(java.lang.String p0) { return null; }
    public android.content.pm.ArchivedPackageInfo getArchivedPackage(java.lang.String p0) { return null; }
    public java.lang.String getAttentionServicePackageName() { return null; }
    public java.lang.CharSequence getBackgroundPermissionOptionLabel() { return null; }
    public android.content.Intent getCarLaunchIntentForPackage(java.lang.String p0) { return null; }
    public java.lang.String getContentCaptureServicePackageName() { return null; }
    public java.util.List getDeclaredSharedLibraries(java.lang.String p0, int p1) { return new java.util.ArrayList(); }
    public java.util.List getDeclaredSharedLibraries(java.lang.String p0, android.content.pm.PackageManager.PackageInfoFlags p1) { return new java.util.ArrayList(); }
    public java.lang.String getDefaultBrowserPackageNameAsUser(int p0) { return null; }
    public java.lang.String getDefaultTextClassifierPackageName() { return null; }
    public void getGroupOfPlatformPermission(java.lang.String p0, java.util.concurrent.Executor p1, java.util.function.Consumer p2) {}
    public java.lang.CharSequence getHarmfulAppWarning(java.lang.String p0) { return null; }
    public android.os.IBinder getHoldLockToken() { return null; }
    public android.content.ComponentName getHomeActivities(java.util.List p0) { return null; }
    public java.lang.String getIncidentReportApproverPackageName() { return null; }
    public int getInstallReason(java.lang.String p0, android.os.UserHandle p1) { return 0; }
    public java.util.List getInstalledApplications(android.content.pm.PackageManager.ApplicationInfoFlags p0) { return new java.util.ArrayList(); }
    public java.util.List getInstalledApplicationsAsUser(int p0, int p1) { return new java.util.ArrayList(); }
    public java.util.List getInstalledApplicationsAsUser(android.content.pm.PackageManager.ApplicationInfoFlags p0, int p1) { return new java.util.ArrayList(); }
    public java.util.List getInstalledModules(int p0) { return new java.util.ArrayList(); }
    public java.util.List getInstalledPackagesAsUser(int p0, int p1) { return new java.util.ArrayList(); }
    public java.util.List getInstalledPackagesAsUser(android.content.pm.PackageManager.PackageInfoFlags p0, int p1) { return new java.util.ArrayList(); }
    public java.lang.String getInstantAppAndroidId(java.lang.String p0, android.os.UserHandle p1) { return null; }
    public byte[] getInstantAppCookie() { return (byte[]) huskFill.get("InstantAppCookie"); }
    public int getInstantAppCookieMaxBytes() { return 0; }
    public int getInstantAppCookieMaxSize() { return 0; }
    public android.graphics.drawable.Drawable getInstantAppIcon(java.lang.String p0) { return null; }
    public android.content.ComponentName getInstantAppInstallerComponent() { return null; }
    public android.content.ComponentName getInstantAppResolverSettingsComponent() { return null; }
    public java.util.List getInstantApps() { return new java.util.ArrayList(); }
    public android.content.pm.InstrumentationInfo getInstrumentationInfo(android.content.ComponentName p0, int p1) { return null; }
    public java.util.List getIntentFilterVerifications(java.lang.String p0) { return new java.util.ArrayList(); }
    public int getIntentVerificationStatusAsUser(java.lang.String p0, int p1) { return 0; }
    public android.content.Intent getLaunchIntentForPackage(java.lang.String p0, boolean p1) { return null; }
    public android.content.IntentSender getLaunchIntentSenderForPackage(java.lang.String p0) { return null; }
    public java.util.Set getMimeGroup(java.lang.String p0) { return new java.util.HashSet(); }
    public int getMoveStatus(int p0) { return 0; }
    public java.lang.String[] getNamesForUids(int[] p0) { return null; }
    public android.content.pm.PackageInfo getPackageArchiveInfo(java.lang.String p0, int p1) { return null; }
    public android.content.pm.PackageInfo getPackageArchiveInfo(java.lang.String p0, android.content.pm.PackageManager.PackageInfoFlags p1) { return null; }
    public java.util.List getPackageCandidateVolumes(android.content.pm.ApplicationInfo p0) { return new java.util.ArrayList(); }
    public int[] getPackageGids(java.lang.String p0) { return null; }
    public int[] getPackageGids(java.lang.String p0, int p1) { return null; }
    public int[] getPackageGids(java.lang.String p0, android.content.pm.PackageManager.PackageInfoFlags p1) { return null; }
    public android.content.pm.PackageInfo getPackageInfo(android.content.pm.VersionedPackage p0, android.content.pm.PackageManager.PackageInfoFlags p1) { return null; }
    public android.content.pm.PackageInfo getPackageInfoAsUser(java.lang.String p0, int p1, int p2) { return null; }
    public android.content.pm.PackageInfo getPackageInfoAsUser(java.lang.String p0, android.content.pm.PackageManager.PackageInfoFlags p1, int p2) { return null; }
    public int getPackageUid(java.lang.String p0, android.content.pm.PackageManager.PackageInfoFlags p1) { return 0; }
    public int getPackageUidAsUser(java.lang.String p0, int p1) { return 0; }
    public int getPackageUidAsUser(java.lang.String p0, int p1, int p2) { return 0; }
    public int getPackageUidAsUser(java.lang.String p0, android.content.pm.PackageManager.PackageInfoFlags p1, int p2) { return 0; }
    public java.util.List getPackagesHoldingPermissions(java.lang.String[] p0, int p1) { return new java.util.ArrayList(); }
    public java.util.List getPackagesHoldingPermissions(java.lang.String[] p0, android.content.pm.PackageManager.PackageInfoFlags p1) { return new java.util.ArrayList(); }
    public java.lang.String getPageSizeCompatWarningMessage(java.lang.String p0) { return null; }
    public java.lang.String getPermissionControllerPackageName() { return null; }
    public int getPermissionFlags(java.lang.String p0, java.lang.String p1, android.os.UserHandle p2) { return 0; }
    public android.content.pm.PermissionGroupInfo getPermissionGroupInfo(java.lang.String p0, int p1) { return null; }
    public void getPlatformPermissionsForGroup(java.lang.String p0, java.util.concurrent.Executor p1, java.util.function.Consumer p2) {}
    public int getPreferredActivities(java.util.List p0, java.util.List p1, java.lang.String p2) { return 0; }
    public java.util.List getPreferredPackages(int p0) { return new java.util.ArrayList(); }
    public java.util.List getPrimaryStorageCandidateVolumes() { return new java.util.ArrayList(); }
    public android.content.pm.PackageManager.Property getProperty(java.lang.String p0, android.content.ComponentName p1) { return null; }
    public android.content.pm.PackageManager.Property getProperty(java.lang.String p0, java.lang.String p1) { return null; }
    public android.content.pm.PackageManager.Property getPropertyAsUser(java.lang.String p0, java.lang.String p1, java.lang.String p2, int p3) { return null; }
    public android.content.pm.ProviderInfo getProviderInfo(android.content.ComponentName p0, android.content.pm.PackageManager.ComponentInfoFlags p1) { return null; }
    public android.content.pm.ActivityInfo getReceiverInfo(android.content.ComponentName p0, android.content.pm.PackageManager.ComponentInfoFlags p1) { return null; }
    public android.content.res.Resources getResourcesForApplication(android.content.pm.ApplicationInfo p0, android.content.res.Configuration p1) { return null; }
    public android.content.res.Resources getResourcesForApplicationAsUser(java.lang.String p0, int p1) { return null; }
    public java.lang.String getRotationResolverPackageName() { return null; }
    public java.lang.String getSdkSandboxPackageName() { return null; }
    public java.lang.String getServicesSystemSharedLibraryPackageName() { return null; }
    public java.lang.String getSetupWizardPackageName() { return null; }
    public java.util.List getSharedLibraries(android.content.pm.PackageManager.PackageInfoFlags p0) { return new java.util.ArrayList(); }
    public java.util.List getSharedLibrariesAsUser(int p0, int p1) { return new java.util.ArrayList(); }
    public java.util.List getSharedLibrariesAsUser(android.content.pm.PackageManager.PackageInfoFlags p0, int p1) { return new java.util.ArrayList(); }
    public java.lang.String getSharedSystemSharedLibraryPackageName() { return null; }
    public android.os.Bundle getSuspendedPackageAppExtras() { return null; }
    public java.lang.String getSuspendingPackage(java.lang.String p0) { return null; }
    public boolean getSyntheticAppDetailsActivityEnabled(java.lang.String p0) { return false; }
    public java.lang.String getSystemCaptionsServicePackageName() { return null; }
    public java.lang.String getSystemTextClassifierPackageName() { return null; }
    public int getTargetSdkVersion(java.lang.String p0) { return 0; }
    public int getUidForSharedUser(java.lang.String p0) { return 0; }
    public java.lang.String[] getUnsuspendablePackages(java.lang.String[] p0) { return null; }
    public android.graphics.drawable.Drawable getUserBadgeForDensity(android.os.UserHandle p0, int p1) { return null; }
    public android.graphics.drawable.Drawable getUserBadgeForDensityNoBackground(android.os.UserHandle p0, int p1) { return null; }
    public android.graphics.drawable.Drawable getUserBadgedDrawableForDensity(android.graphics.drawable.Drawable p0, android.os.UserHandle p1, android.graphics.Rect p2, int p3) { return null; }
    public int getUserId() { return 0; }
    public java.lang.String getWellbeingPackageName() { return null; }
    public java.util.Set getWhitelistedRestrictedPermissions(java.lang.String p0, int p1) { return new java.util.HashSet(); }
    public void grantRuntimePermission(java.lang.String p0, java.lang.String p1, android.os.UserHandle p2) {}
    public boolean hasSigningCertificate(int p0, byte[] p1, int p2) { return false; }
    public void holdLock(android.os.IBinder p0, int p1) {}
    public int installExistingPackage(java.lang.String p0) { return 0; }
    public int installExistingPackage(java.lang.String p0, int p1) { return 0; }
    public int installExistingPackageAsUser(java.lang.String p0, int p1) { return 0; }
    public boolean isAppArchivable(java.lang.String p0) { return false; }
    public boolean isAutoRevokeWhitelisted() { return (huskFill.get("AutoRevokeWhitelisted") instanceof Boolean ? (Boolean) huskFill.get("AutoRevokeWhitelisted") : false); }
    public boolean isAutoRevokeWhitelisted(java.lang.String p0) { return false; }
    public boolean isDefaultApplicationIcon(android.graphics.drawable.Drawable p0) { return false; }
    public boolean isDeviceUpgrading() { return false; }
    public boolean isPackageAvailable(java.lang.String p0) { return false; }
    public boolean isPackageQuarantined(java.lang.String p0) { return false; }
    public boolean isPackageStateProtected(java.lang.String p0, int p1) { return false; }
    public boolean isPackageStopped(java.lang.String p0) { return false; }
    public boolean isPackageSuspended(java.lang.String p0) { return false; }
    public boolean isPackageSuspendedForUser(java.lang.String p0, int p1) { return false; }
    public boolean isPageSizeCompatEnabled(java.lang.String p0) { return false; }
    public boolean isUpgrade() { return false; }
    public boolean isWirelessConsentModeEnabled() { return false; }
    public android.graphics.drawable.Drawable loadItemIcon(android.content.pm.PackageItemInfo p0, android.content.pm.ApplicationInfo p1) { return null; }
    public android.graphics.drawable.Drawable loadUnbadgedItemIcon(android.content.pm.PackageItemInfo p0, android.content.pm.ApplicationInfo p1) { return null; }
    public void makeProviderVisible(int p0, java.lang.String p1) {}
    public void makeUidVisible(int p0, int p1) {}
    public java.lang.Object parseAndroidManifest(android.os.ParcelFileDescriptor p0, java.util.function.Function p1) { return null; }
    public java.lang.Object parseAndroidManifest(java.io.File p0, java.util.function.Function p1) { return null; }
    public java.util.List queryActivityProperty(java.lang.String p0) { return new java.util.ArrayList(); }
    public java.util.List queryApplicationProperty(java.lang.String p0) { return new java.util.ArrayList(); }
    public java.util.List queryBroadcastReceivers(android.content.Intent p0, int p1, int p2) { return new java.util.ArrayList(); }
    public java.util.List queryBroadcastReceivers(android.content.Intent p0, android.content.pm.PackageManager.ResolveInfoFlags p1) { return new java.util.ArrayList(); }
    public java.util.List queryBroadcastReceiversAsUser(android.content.Intent p0, int p1, int p2) { return new java.util.ArrayList(); }
    public java.util.List queryBroadcastReceiversAsUser(android.content.Intent p0, int p1, android.os.UserHandle p2) { return new java.util.ArrayList(); }
    public java.util.List queryBroadcastReceiversAsUser(android.content.Intent p0, android.content.pm.PackageManager.ResolveInfoFlags p1, int p2) { return new java.util.ArrayList(); }
    public java.util.List queryBroadcastReceiversAsUser(android.content.Intent p0, android.content.pm.PackageManager.ResolveInfoFlags p1, android.os.UserHandle p2) { return new java.util.ArrayList(); }
    public java.util.List queryContentProviders(java.lang.String p0, int p1, int p2, java.lang.String p3) { return new java.util.ArrayList(); }
    public java.util.List queryContentProviders(java.lang.String p0, int p1, android.content.pm.PackageManager.ComponentInfoFlags p2) { return new java.util.ArrayList(); }
    public java.util.List queryContentProviders(java.lang.String p0, int p1, android.content.pm.PackageManager.ComponentInfoFlags p2, java.lang.String p3) { return new java.util.ArrayList(); }
    public java.util.List queryInstrumentation(java.lang.String p0, int p1) { return new java.util.ArrayList(); }
    public java.util.List queryIntentActivitiesAsUser(android.content.Intent p0, int p1, int p2) { return new java.util.ArrayList(); }
    public java.util.List queryIntentActivitiesAsUser(android.content.Intent p0, int p1, android.os.UserHandle p2) { return new java.util.ArrayList(); }
    public java.util.List queryIntentActivitiesAsUser(android.content.Intent p0, android.content.pm.PackageManager.ResolveInfoFlags p1, int p2) { return new java.util.ArrayList(); }
    public java.util.List queryIntentActivitiesAsUser(android.content.Intent p0, android.content.pm.PackageManager.ResolveInfoFlags p1, android.os.UserHandle p2) { return new java.util.ArrayList(); }
    public java.util.List queryIntentActivityOptions(android.content.ComponentName p0, java.util.List p1, android.content.Intent p2, android.content.pm.PackageManager.ResolveInfoFlags p3) { return new java.util.ArrayList(); }
    public java.util.List queryIntentActivityOptions(android.content.ComponentName p0, android.content.Intent[] p1, android.content.Intent p2, int p3) { return new java.util.ArrayList(); }
    public java.util.List queryIntentContentProviders(android.content.Intent p0, android.content.pm.PackageManager.ResolveInfoFlags p1) { return new java.util.ArrayList(); }
    public java.util.List queryIntentContentProvidersAsUser(android.content.Intent p0, int p1, int p2) { return new java.util.ArrayList(); }
    public java.util.List queryIntentContentProvidersAsUser(android.content.Intent p0, int p1, android.os.UserHandle p2) { return new java.util.ArrayList(); }
    protected java.util.List queryIntentContentProvidersAsUser(android.content.Intent p0, android.content.pm.PackageManager.ResolveInfoFlags p1, int p2) { return new java.util.ArrayList(); }
    public java.util.List queryIntentContentProvidersAsUser(android.content.Intent p0, android.content.pm.PackageManager.ResolveInfoFlags p1, android.os.UserHandle p2) { return new java.util.ArrayList(); }
    public java.util.List queryIntentServices(android.content.Intent p0, android.content.pm.PackageManager.ResolveInfoFlags p1) { return new java.util.ArrayList(); }
    public java.util.List queryIntentServicesAsUser(android.content.Intent p0, int p1, int p2) { return new java.util.ArrayList(); }
    public java.util.List queryIntentServicesAsUser(android.content.Intent p0, int p1, android.os.UserHandle p2) { return new java.util.ArrayList(); }
    public java.util.List queryIntentServicesAsUser(android.content.Intent p0, android.content.pm.PackageManager.ResolveInfoFlags p1, int p2) { return new java.util.ArrayList(); }
    public java.util.List queryIntentServicesAsUser(android.content.Intent p0, android.content.pm.PackageManager.ResolveInfoFlags p1, android.os.UserHandle p2) { return new java.util.ArrayList(); }
    public java.util.List queryPermissionsByGroup(java.lang.String p0, int p1) { return new java.util.ArrayList(); }
    public java.util.List queryProviderProperty(java.lang.String p0) { return new java.util.ArrayList(); }
    public java.util.List queryReceiverProperty(java.lang.String p0) { return new java.util.ArrayList(); }
    public java.util.List queryServiceProperty(java.lang.String p0) { return new java.util.ArrayList(); }
    public void registerDexModule(java.lang.String p0, android.content.pm.PackageManager.DexModuleRegisterCallback p1) {}
    public void registerMoveCallback(android.content.pm.PackageManager.MoveCallback p0, android.os.Handler p1) {}
    public void registerPackageMonitorCallback(android.os.IRemoteCallback p0, int p1) {}
    public void relinquishUpdateOwnership(java.lang.String p0) {}
    public boolean removeCrossProfileIntentFilter(android.content.IntentFilter p0, int p1, int p2, int p3) { return false; }
    public void removeOnPermissionsChangeListener(android.content.pm.PackageManager.OnPermissionsChangedListener p0) {}
    public void removePackageFromPreferred(java.lang.String p0) {}
    public void removePermission(java.lang.String p0) {}
    public boolean removeWhitelistedRestrictedPermission(java.lang.String p0, java.lang.String p1, int p2) { return false; }
    public void replacePreferredActivity(android.content.IntentFilter p0, int p1, java.util.List p2, android.content.ComponentName p3) {}
    public void replacePreferredActivity(android.content.IntentFilter p0, int p1, android.content.ComponentName[] p2, android.content.ComponentName p3) {}
    public void replacePreferredActivityAsUser(android.content.IntentFilter p0, int p1, android.content.ComponentName[] p2, android.content.ComponentName p3, int p4) {}
    public void requestChecksums(java.lang.String p0, boolean p1, int p2, java.util.List p3, android.content.pm.PackageManager.OnChecksumsReadyListener p4) {}
    public android.content.pm.ResolveInfo resolveActivityAsUser(android.content.Intent p0, int p1, int p2) { return null; }
    public android.content.pm.ResolveInfo resolveActivityAsUser(android.content.Intent p0, android.content.pm.PackageManager.ResolveInfoFlags p1, int p2) { return null; }
    public android.content.pm.ResolveInfo resolveActivityAsUser(android.content.Intent p0, java.lang.String p1, int p2, int p3) { return null; }
    public android.content.pm.ResolveInfo resolveActivityAsUser(android.content.Intent p0, java.lang.String p1, android.content.pm.PackageManager.ResolveInfoFlags p2, int p3) { return null; }
    public android.content.pm.ProviderInfo resolveContentProvider(java.lang.String p0, android.content.pm.PackageManager.ComponentInfoFlags p1) { return null; }
    public android.content.pm.ProviderInfo resolveContentProviderAsUser(java.lang.String p0, int p1, int p2) { return null; }
    public android.content.pm.ProviderInfo resolveContentProviderAsUser(java.lang.String p0, android.content.pm.PackageManager.ComponentInfoFlags p1, int p2) { return null; }
    public android.content.pm.ProviderInfo resolveContentProviderForUid(java.lang.String p0, android.content.pm.PackageManager.ComponentInfoFlags p1, int p2) { return null; }
    public android.content.pm.ResolveInfo resolveService(android.content.Intent p0, android.content.pm.PackageManager.ResolveInfoFlags p1) { return null; }
    public android.content.pm.ResolveInfo resolveServiceAsUser(android.content.Intent p0, int p1, int p2) { return null; }
    public android.content.pm.ResolveInfo resolveServiceAsUser(android.content.Intent p0, android.content.pm.PackageManager.ResolveInfoFlags p1, int p2) { return null; }
    public void revokeRuntimePermission(java.lang.String p0, java.lang.String p1, android.os.UserHandle p2) {}
    public void revokeRuntimePermission(java.lang.String p0, java.lang.String p1, android.os.UserHandle p2, java.lang.String p3) {}
    public void sendDeviceCustomizationReadyBroadcast() {}
    public void setApplicationCategoryHint(java.lang.String p0, int p1) {}
    public boolean setApplicationHiddenSettingAsUser(java.lang.String p0, boolean p1, android.os.UserHandle p2) { return false; }
    public boolean setAutoRevokeWhitelisted(java.lang.String p0, boolean p1) { return false; }
    public void setComponentEnabledSettings(java.util.List p0) {}
    public boolean setDefaultBrowserPackageNameAsUser(java.lang.String p0, int p1) { return false; }
    public java.lang.String[] setDistractingPackageRestrictions(java.lang.String[] p0, int p1) { return null; }
    public void setHarmfulAppWarning(java.lang.String p0, java.lang.CharSequence p1) {}
    public void setInstallerPackageName(java.lang.String p0, java.lang.String p1) {}
    public boolean setInstantAppCookie(byte[] p0) { return false; }
    public void setKeepUninstalledPackages(java.util.List p0) {}
    public void setMimeGroup(java.lang.String p0, java.util.Set p1) {}
    public java.lang.String[] setPackagesSuspended(java.lang.String[] p0, boolean p1, android.os.PersistableBundle p2, android.os.PersistableBundle p3, java.lang.String p4) { return null; }
    public void setPageSizeAppCompatFlagsSettingsOverride(java.lang.String p0, boolean p1) {}
    public void setSyntheticAppDetailsActivityEnabled(java.lang.String p0, boolean p1) {}
    public void setSystemAppState(java.lang.String p0, int p1) {}
    public void setUpdateAvailable(java.lang.String p0, boolean p1) {}
    public boolean shouldShowNewAppInstalledNotification() { return false; }
    public boolean shouldShowRequestPermissionRationale(java.lang.String p0) { return false; }
    public void unregisterMoveCallback(android.content.pm.PackageManager.MoveCallback p0) {}
    public void unregisterPackageMonitorCallback(android.os.IRemoteCallback p0) {}
    public void updateInstantAppCookie(byte[] p0) {}
    public boolean updateIntentVerificationStatusAsUser(java.lang.String p0, int p1, int p2) { return false; }
    public void updatePermissionFlags(java.lang.String p0, java.lang.String p1, int p2, int p3, android.os.UserHandle p4) {}
    public void verifyIntentFilter(int p0, int p1, java.util.List p2) {}
    public void verifyPendingInstall(int p0, int p1) {}
    // ---- end of generated members
    // ---- generated by tools/compat/genstubs.py: the platform's nested classes this class does not write
    public static final class ComponentEnabledSetting implements android.os.Parcelable {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public static android.os.Parcelable.Creator CREATOR;
        public ComponentEnabledSetting(android.content.ComponentName p0, int p1, int p2) {}
        public ComponentEnabledSetting(java.lang.String p0, int p1, int p2) {}
        public int describeContents() { return 0; }
        public java.lang.String getClassName() { return (java.lang.String) huskProps.get("ClassName"); }
        public android.content.ComponentName getComponentName() { return (android.content.ComponentName) huskProps.get("ComponentName"); }
        public int getEnabledFlags() { return (huskProps.get("EnabledFlags") instanceof Integer ? (Integer) huskProps.get("EnabledFlags") : 0); }
        public int getEnabledState() { return (huskProps.get("EnabledState") instanceof Integer ? (Integer) huskProps.get("EnabledState") : 0); }
        public java.lang.String getPackageName() { return (java.lang.String) huskProps.get("PackageName"); }
        public boolean isComponent() { return (huskProps.get("Component") instanceof Boolean ? (Boolean) huskProps.get("Component") : false); }
        public void writeToParcel(android.os.Parcel p0, int p1) {}
        ComponentEnabledSetting() { this((android.content.ComponentName) null, (int) 0, (int) 0); }
    }
    public static abstract class DexModuleRegisterCallback {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public DexModuleRegisterCallback() {}
        public abstract void onDexModuleRegistered(java.lang.String p0, boolean p1, java.lang.String p2);
    }
    public static class Flags {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        protected Flags(long p0) {}
        public long getValue() { return (huskProps.get("Value") instanceof Long ? (Long) huskProps.get("Value") : 0L); }
        Flags() { this((long) 0L); }
    }
    public static abstract class LegacyPackageDeleteObserver extends android.app.PackageDeleteObserver {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public LegacyPackageDeleteObserver(android.content.pm.IPackageDeleteObserver p0) { super(); }
        public void onPackageDeleted(java.lang.String p0, int p1, java.lang.String p2) {}
        LegacyPackageDeleteObserver() { this((android.content.pm.IPackageDeleteObserver) null); }
    }
    public static abstract class MoveCallback {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public MoveCallback() {}
        public void onCreated(int p0, android.os.Bundle p1) {}
        public abstract void onStatusChanged(int p0, int p1, long p2);
    }
    public interface OnChecksumsReadyListener {
        void onChecksumsReady(java.util.List p0);
    }
    public interface OnPermissionsChangedListener {
        void onPermissionsChanged(int p0);
        default void onPermissionsChanged(int p0, java.lang.String p1) {}
    }
    public static final class Property implements android.os.Parcelable {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public static android.os.Parcelable.Creator CREATOR;
        public Property(java.lang.String p0, float p1, java.lang.String p2, java.lang.String p3) {}
        public Property(java.lang.String p0, int p1, java.lang.String p2, java.lang.String p3) {}
        public Property(java.lang.String p0, int p1, boolean p2, java.lang.String p3, java.lang.String p4) {}
        public Property(java.lang.String p0, java.lang.String p1, java.lang.String p2, java.lang.String p3) {}
        public Property(java.lang.String p0, boolean p1, java.lang.String p2, java.lang.String p3) {}
        public int describeContents() { return 0; }
        public boolean getBoolean() { return (huskProps.get("Boolean") instanceof Boolean ? (Boolean) huskProps.get("Boolean") : false); }
        public java.lang.String getClassName() { return (java.lang.String) huskProps.get("ClassName"); }
        public float getFloat() { return (huskProps.get("Float") instanceof Float ? (Float) huskProps.get("Float") : 0f); }
        public int getInteger() { return (huskProps.get("Integer") instanceof Integer ? (Integer) huskProps.get("Integer") : 0); }
        public java.lang.String getName() { return (java.lang.String) huskProps.get("Name"); }
        public java.lang.String getPackageName() { return (java.lang.String) huskProps.get("PackageName"); }
        public int getResourceId() { return (huskProps.get("ResourceId") instanceof Integer ? (Integer) huskProps.get("ResourceId") : 0); }
        public java.lang.String getString() { return (java.lang.String) huskProps.get("String"); }
        public int getType() { return (huskProps.get("Type") instanceof Integer ? (Integer) huskProps.get("Type") : 0); }
        public boolean isBoolean() { return (huskProps.get("Boolean") instanceof Boolean ? (Boolean) huskProps.get("Boolean") : false); }
        public boolean isFloat() { return (huskProps.get("Float") instanceof Boolean ? (Boolean) huskProps.get("Float") : false); }
        public boolean isInteger() { return (huskProps.get("Integer") instanceof Boolean ? (Boolean) huskProps.get("Integer") : false); }
        public boolean isResourceId() { return (huskProps.get("ResourceId") instanceof Boolean ? (Boolean) huskProps.get("ResourceId") : false); }
        public boolean isString() { return (huskProps.get("String") instanceof Boolean ? (Boolean) huskProps.get("String") : false); }
        public android.os.Bundle toBundle(android.os.Bundle p0) { return null; }
        public void writeToParcel(android.os.Parcel p0, int p1) {}
        Property() { this((java.lang.String) null, (float) 0f, (java.lang.String) null, (java.lang.String) null); }
    }
    public static final class UninstallCompleteCallback implements android.os.Parcelable {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public static android.os.Parcelable.Creator CREATOR;
        public UninstallCompleteCallback(android.os.IBinder p0) {}
        public int describeContents() { return 0; }
        public void onUninstallComplete(java.lang.String p0, int p1, java.lang.String p2) {}
        public void writeToParcel(android.os.Parcel p0, int p1) {}
        UninstallCompleteCallback() { this((android.os.IBinder) null); }
    }
    // ---- end of generated nested classes
}
