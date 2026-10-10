package android.net;
/** The network as the app sees it: always connected, over Wi-Fi (Husk does not watch the iPhone's network). */
public class ConnectivityManager {
    public static final int TYPE_NONE = -1, TYPE_MOBILE = 0, TYPE_WIFI = 1, TYPE_ETHERNET = 9, TYPE_VPN = 17, RESTRICT_BACKGROUND_STATUS_DISABLED = 1;
    public static final String CONNECTIVITY_ACTION = "android.net.conn.CONNECTIVITY_CHANGE", EXTRA_NO_CONNECTIVITY = "noConnectivity", EXTRA_NETWORK_INFO = "networkInfo";
    public static class NetworkCallback {
        public NetworkCallback() {} public NetworkCallback(int flags) {}
        public void onAvailable(Network n) {} public void onLosing(Network n, int ms) {} public void onLost(Network n) {} public void onUnavailable() {}
        public void onCapabilitiesChanged(Network n, NetworkCapabilities c) {} public void onLinkPropertiesChanged(Network n, LinkProperties p) {} public void onBlockedStatusChanged(Network n, boolean b) {}
    }
    public interface OnNetworkActiveListener { void onNetworkActive(); }
    private static final Network sNet = new Network();
    public NetworkInfo getActiveNetworkInfo() { return new NetworkInfo(); }
    public NetworkInfo getNetworkInfo(int type) { return type == TYPE_WIFI ? new NetworkInfo() : null; }
    public NetworkInfo getNetworkInfo(Network n) { return new NetworkInfo(); }
    public NetworkInfo[] getAllNetworkInfo() { return new NetworkInfo[] { new NetworkInfo() }; }
    public Network getActiveNetwork() { return sNet; }
    public Network[] getAllNetworks() { return new Network[] { sNet }; }
    public NetworkCapabilities getNetworkCapabilities(Network n) { return new NetworkCapabilities(); }
    public LinkProperties getLinkProperties(Network n) { return new LinkProperties(); }
    public boolean isActiveNetworkMetered() { return false; }
    public boolean isDefaultNetworkActive() { return true; }
    public int getRestrictBackgroundStatus() { return RESTRICT_BACKGROUND_STATUS_DISABLED; }
    public boolean bindProcessToNetwork(Network n) { return true; }
    public Network getBoundNetworkForProcess() { return null; }
    private void announce(NetworkCallback cb) { new android.os.Handler(android.os.Looper.getMainLooper()).post(() -> { cb.onAvailable(sNet); cb.onCapabilitiesChanged(sNet, new NetworkCapabilities()); cb.onLinkPropertiesChanged(sNet, new LinkProperties()); }); }
    public void registerNetworkCallback(NetworkRequest r, NetworkCallback cb) { announce(cb); }
    public void registerNetworkCallback(NetworkRequest r, NetworkCallback cb, android.os.Handler h) { announce(cb); }
    public void registerDefaultNetworkCallback(NetworkCallback cb) { announce(cb); }
    public void registerDefaultNetworkCallback(NetworkCallback cb, android.os.Handler h) { announce(cb); }
    public void requestNetwork(NetworkRequest r, NetworkCallback cb) { announce(cb); }
    public void requestNetwork(NetworkRequest r, NetworkCallback cb, int timeout) { announce(cb); }
    public void unregisterNetworkCallback(NetworkCallback cb) {}
    public void addDefaultNetworkActiveListener(OnNetworkActiveListener l) {}
    public void removeDefaultNetworkActiveListener(OnNetworkActiveListener l) {}
    public static boolean isNetworkTypeValid(int t) { return t >= 0; }
}
