package android.net;
public final class LinkProperties implements android.os.Parcelable {
    public String getInterfaceName() { return "en0"; } public java.util.List<java.net.InetAddress> getDnsServers() { return new java.util.ArrayList<>(); } public String getDomains() { return null; }
    public java.util.List<LinkAddress> getLinkAddresses() { return new java.util.ArrayList<>(); } public boolean isPrivateDnsActive() { return false; } public String getPrivateDnsServerName() { return null; }
    public ProxyInfo getHttpProxy() { return null; } public int getMtu() { return 1500; }
    public int describeContents() { return 0; }
}
