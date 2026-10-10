package android.content.pm;
public class PackageInfo {
    public String packageName, versionName = "1.0";
    public int versionCode = 1;
    public long firstInstallTime, lastUpdateTime;
    public ApplicationInfo applicationInfo = ApplicationInfo.self();
    public Signature[] signatures = new Signature[0];
    public long getLongVersionCode() { return versionCode; }
}
