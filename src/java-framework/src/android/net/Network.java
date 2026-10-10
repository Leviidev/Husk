package android.net;
public class Network implements android.os.Parcelable {
    public java.net.InetAddress[] getAllByName(String host) throws java.net.UnknownHostException { return java.net.InetAddress.getAllByName(host); }
    public java.net.InetAddress getByName(String host) throws java.net.UnknownHostException { return java.net.InetAddress.getByName(host); }
    public java.net.URLConnection openConnection(java.net.URL u) throws java.io.IOException { return u.openConnection(); }
    public javax.net.SocketFactory getSocketFactory() { return javax.net.SocketFactory.getDefault(); }
    public void bindSocket(java.net.Socket s) {} public long getNetworkHandle() { return 100; }
    public int describeContents() { return 0; }
}
