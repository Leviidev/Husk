package android.net;
public class NetworkInfo implements android.os.Parcelable {
    public enum State { CONNECTING, CONNECTED, SUSPENDED, DISCONNECTING, DISCONNECTED, UNKNOWN }
    public enum DetailedState { IDLE, SCANNING, CONNECTING, AUTHENTICATING, OBTAINING_IPADDR, CONNECTED, SUSPENDED, DISCONNECTING, DISCONNECTED, FAILED, BLOCKED, VERIFYING_POOR_LINK, CAPTIVE_PORTAL_CHECK }
    public int getType() { return ConnectivityManager.TYPE_WIFI; } public int getSubtype() { return 0; } public String getTypeName() { return "WIFI"; } public String getSubtypeName() { return ""; }
    public boolean isConnectedOrConnecting() { return true; } public boolean isConnected() { return true; } public boolean isAvailable() { return true; } public boolean isFailover() { return false; } public boolean isRoaming() { return false; }
    public State getState() { return State.CONNECTED; } public DetailedState getDetailedState() { return DetailedState.CONNECTED; } public String getReason() { return null; } public String getExtraInfo() { return null; }
    public int describeContents() { return 0; }
}
