package android.content;
public class IntentSender implements android.os.Parcelable {
    public static class SendIntentException extends android.util.AndroidException { public SendIntentException() {} public SendIntentException(String s) { super(s); } public SendIntentException(Exception e) { super(e); } }
    public interface OnFinished { void onSendFinished(IntentSender s, Intent i, int code, String data, android.os.Bundle extras); }
    public IntentSender() {}
    public void sendIntent(Context c, int code, Intent i, OnFinished f, android.os.Handler h) throws SendIntentException {}
    public String getCreatorPackage() { return husk.Native.packageName(); }
    public int describeContents() { return 0; }
}
