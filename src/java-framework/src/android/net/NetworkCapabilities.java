package android.net;
public final class NetworkCapabilities implements android.os.Parcelable {
    public static final int NET_CAPABILITY_MMS = 0, NET_CAPABILITY_SUPL = 1, NET_CAPABILITY_INTERNET = 12, NET_CAPABILITY_NOT_METERED = 11, NET_CAPABILITY_NOT_RESTRICTED = 13,
        NET_CAPABILITY_TRUSTED = 14, NET_CAPABILITY_NOT_VPN = 15, NET_CAPABILITY_VALIDATED = 16, NET_CAPABILITY_CAPTIVE_PORTAL = 17, NET_CAPABILITY_NOT_ROAMING = 18,
        NET_CAPABILITY_FOREGROUND = 19, NET_CAPABILITY_NOT_CONGESTED = 20, NET_CAPABILITY_NOT_SUSPENDED = 21, NET_CAPABILITY_TEMPORARILY_NOT_METERED = 25;
    public static final int TRANSPORT_CELLULAR = 0, TRANSPORT_WIFI = 1, TRANSPORT_BLUETOOTH = 2, TRANSPORT_ETHERNET = 3, TRANSPORT_VPN = 4, TRANSPORT_WIFI_AWARE = 5, TRANSPORT_LOWPAN = 6, TRANSPORT_USB = 8;
    public boolean hasCapability(int c) { return c == NET_CAPABILITY_INTERNET || c == NET_CAPABILITY_VALIDATED || c == NET_CAPABILITY_NOT_METERED || c == NET_CAPABILITY_NOT_RESTRICTED || c == NET_CAPABILITY_TRUSTED || c == NET_CAPABILITY_NOT_VPN || c == NET_CAPABILITY_NOT_ROAMING || c == NET_CAPABILITY_FOREGROUND || c == NET_CAPABILITY_NOT_CONGESTED || c == NET_CAPABILITY_NOT_SUSPENDED; }
    public boolean hasTransport(int t) { return t == TRANSPORT_WIFI; }
    public int[] getCapabilities() { return new int[] { NET_CAPABILITY_INTERNET, NET_CAPABILITY_VALIDATED, NET_CAPABILITY_NOT_METERED }; }
    public int getLinkDownstreamBandwidthKbps() { return 100000; } public int getLinkUpstreamBandwidthKbps() { return 50000; } public int getSignalStrength() { return -50; }
    public android.net.TransportInfo getTransportInfo() { return null; }
    public int describeContents() { return 0; }
}
