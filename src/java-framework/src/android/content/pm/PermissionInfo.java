package android.content.pm;
public class PermissionInfo extends PackageItemInfo { public static final int PROTECTION_NORMAL = 0, PROTECTION_DANGEROUS = 1, PROTECTION_SIGNATURE = 2; public int protectionLevel; public String group; public int getProtection() { return protectionLevel; } }
