package android.content.pm;
public class PackageInfo implements android.os.Parcelable {
    public static final int INSTALL_LOCATION_AUTO = 0, INSTALL_LOCATION_INTERNAL_ONLY = 1, INSTALL_LOCATION_PREFER_EXTERNAL = 2, REQUESTED_PERMISSION_GRANTED = 2;
    public String packageName, versionName = "1.0", sharedUserId;
    public String[] splitNames = new String[0], requestedPermissions;
    public int versionCode = 1, baseRevisionCode, sharedUserLabel, installLocation;
    public int[] gids, requestedPermissionsFlags, splitRevisionCodes;
    public long firstInstallTime, lastUpdateTime;
    public ApplicationInfo applicationInfo = ApplicationInfo.self();
    public ActivityInfo[] activities, receivers;
    public ServiceInfo[] services;
    public ProviderInfo[] providers;
    public Signature[] signatures = new Signature[] { new Signature("") };
    public SigningInfo signingInfo = new SigningInfo();
    public FeatureInfo[] reqFeatures;
    public long getLongVersionCode() { return versionCode; }
    public void setLongVersionCode(long v) { versionCode = (int) v; }
    public int describeContents() { return 0; }
}
