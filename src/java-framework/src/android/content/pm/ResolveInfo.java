package android.content.pm;
public class ResolveInfo implements android.os.Parcelable {
    public ActivityInfo activityInfo; public ServiceInfo serviceInfo; public ProviderInfo providerInfo;
    public android.content.IntentFilter filter; public int priority, preferredOrder, match, specificIndex = -1, labelRes, icon; public boolean isDefault; public CharSequence nonLocalizedLabel; public String resolvePackageName;
    public CharSequence loadLabel(PackageManager pm) { return activityInfo != null ? activityInfo.loadLabel(pm) : serviceInfo != null ? serviceInfo.loadLabel(pm) : nonLocalizedLabel; }
    public android.graphics.drawable.Drawable loadIcon(PackageManager pm) { return activityInfo != null ? activityInfo.loadIcon(pm) : null; }
    public final int getIconResource() { return icon; }
    public int describeContents() { return 0; }
}
