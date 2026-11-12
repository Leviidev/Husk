import android.net.ConnectivityManager;
import android.net.NetworkInfo;

/** Legacy callers query both transports repeatedly without null checks. */
public class LegacyNetworkTest {
    private static void check(boolean condition) {
        if (!condition) throw new AssertionError("legacy network state");
    }
    public static void main(String[] args) {
        ConnectivityManager manager = new ConnectivityManager();
        for (int i = 0; i < 1000; i++) {
            NetworkInfo mobile = manager.getNetworkInfo(ConnectivityManager.TYPE_MOBILE);
            NetworkInfo wifi = manager.getNetworkInfo(ConnectivityManager.TYPE_WIFI);
            check(!mobile.isConnectedOrConnecting() && wifi.isConnectedOrConnecting());
            check(mobile.getType() == ConnectivityManager.TYPE_MOBILE && "MOBILE".equals(mobile.getTypeName()));
            check(mobile.getState() == NetworkInfo.State.DISCONNECTED && !mobile.isAvailable());
            check(wifi.getType() == ConnectivityManager.TYPE_WIFI && wifi.isConnected() && wifi.isAvailable());
        }
        check(manager.getActiveNetworkInfo().getType() == ConnectivityManager.TYPE_WIFI);
        check(manager.getAllNetworkInfo().length == 2);
        check(manager.getNetworkInfo(-99) == null);
        NetworkInfo changed = manager.getNetworkInfo(ConnectivityManager.TYPE_MOBILE);
        changed.setDetailedState(NetworkInfo.DetailedState.CONNECTING, "test", "carrier");
        NetworkInfo copy = new NetworkInfo(changed);
        check(copy.isConnectedOrConnecting() && !copy.isConnected() && "test".equals(copy.getReason()));
        changed.setDetailedState(NetworkInfo.DetailedState.DISCONNECTED, null, null);
        check(copy.isConnectedOrConnecting());
        check(!manager.getNetworkInfo(ConnectivityManager.TYPE_MOBILE).isConnected());
        System.out.println("PASS: repeated legacy mobile/Wi-Fi queries, disconnected transport, independent snapshots");
    }
}
