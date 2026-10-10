package android.content.pm;
public class ComponentInfo extends PackageItemInfo {
    public ApplicationInfo applicationInfo = ApplicationInfo.self();
    public String processName, splitName;
    public int descriptionRes;
    public boolean enabled = true, exported, directBootAware;
    public ComponentInfo() {}
    public ComponentInfo(ComponentInfo o) { super(o); applicationInfo = o.applicationInfo; processName = o.processName; enabled = o.enabled; exported = o.exported; }
    public boolean isEnabled() { return enabled; }
    public final int getIconResource() { return icon != 0 ? icon : applicationInfo.icon; }
}
