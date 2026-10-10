package android.net;
public class NetworkRequest implements android.os.Parcelable {
    public int describeContents() { return 0; }
    public boolean hasCapability(int c) { return false; } public boolean hasTransport(int t) { return false; }
    public static class Builder { public Builder addCapability(int c) { return this; } public Builder removeCapability(int c) { return this; } public Builder addTransportType(int t) { return this; } public Builder removeTransportType(int t) { return this; } public Builder clearCapabilities() { return this; } public NetworkRequest build() { return new NetworkRequest(); } }
}
