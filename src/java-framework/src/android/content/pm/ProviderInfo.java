package android.content.pm;
public final class ProviderInfo extends ComponentInfo implements android.os.Parcelable {
    public String authority, readPermission, writePermission;
    public boolean grantUriPermissions, forceUriPermissions, multiprocess, isSyncable;
    public int initOrder, flags;
    public ProviderInfo() {}
    public ProviderInfo(ProviderInfo o) { super(o); authority = o.authority; readPermission = o.readPermission; writePermission = o.writePermission; }
    public int describeContents() { return 0; }
}
