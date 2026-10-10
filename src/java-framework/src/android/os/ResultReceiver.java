package android.os;
public class ResultReceiver implements Parcelable {
    private final Handler mHandler;
    public ResultReceiver(Handler h) { mHandler = h; }
    public void send(int code, Bundle data) { if (mHandler != null) mHandler.post(() -> onReceiveResult(code, data)); else onReceiveResult(code, data); }
    protected void onReceiveResult(int code, Bundle data) {}
    public int describeContents() { return 0; }
}
