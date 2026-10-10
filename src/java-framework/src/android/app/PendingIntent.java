package android.app;

import android.content.Context;
import android.content.Intent;

/** A pending intent, held in this process; sending it starts it as the app would. */
public final class PendingIntent implements android.os.Parcelable {
    public static final int FLAG_ONE_SHOT = 0x40000000, FLAG_NO_CREATE = 0x20000000, FLAG_CANCEL_CURRENT = 0x10000000, FLAG_UPDATE_CURRENT = 0x08000000,
        FLAG_IMMUTABLE = 0x04000000, FLAG_MUTABLE = 0x02000000, FLAG_ALLOW_UNSAFE_IMPLICIT_INTENT = 0x01000000;
    public static class CanceledException extends android.util.AndroidException { public CanceledException() {} public CanceledException(String s) { super(s); } public CanceledException(Exception e) { super(e); } }
    public interface OnFinished { void onSendFinished(PendingIntent p, Intent i, int resultCode, String resultData, android.os.Bundle resultExtras); }
    private final Context mContext; private final Intent mIntent; private final int mKind;   /* 0 activity, 1 broadcast, 2 service */
    private PendingIntent(Context c, Intent i, int kind) { mContext = c; mIntent = i; mKind = kind; }
    public static PendingIntent getActivity(Context c, int req, Intent i, int flags) { return new PendingIntent(c, i, 0); }
    public static PendingIntent getActivity(Context c, int req, Intent i, int flags, android.os.Bundle o) { return new PendingIntent(c, i, 0); }
    public static PendingIntent getActivities(Context c, int req, Intent[] i, int flags) { return new PendingIntent(c, i[i.length - 1], 0); }
    public static PendingIntent getBroadcast(Context c, int req, Intent i, int flags) { return new PendingIntent(c, i, 1); }
    public static PendingIntent getService(Context c, int req, Intent i, int flags) { return new PendingIntent(c, i, 2); }
    public static PendingIntent getForegroundService(Context c, int req, Intent i, int flags) { return new PendingIntent(c, i, 2); }
    public void send() throws CanceledException { send(mContext, 0, null); }
    public void send(int code) throws CanceledException { send(mContext, code, null); }
    public void send(Context c, int code, Intent fill) throws CanceledException {
        Intent i = fill != null ? new Intent(mIntent).putExtras(fill) : mIntent;
        if (mKind == 0) mContext.startActivity(i); else if (mKind == 1) mContext.sendBroadcast(i); else mContext.startService(i);
    }
    public void send(int code, OnFinished f, android.os.Handler h) throws CanceledException { send(); }
    public void send(Context c, int code, Intent i, OnFinished f, android.os.Handler h) throws CanceledException { send(c, code, i); }
    public void cancel() {}
    public String getCreatorPackage() { return husk.Native.packageName(); }
    public String getTargetPackage() { return getCreatorPackage(); }
    public int getCreatorUid() { return android.os.Process.myUid(); }
    public android.os.UserHandle getCreatorUserHandle() { return android.os.UserHandle.getUserHandleForUid(0); }
    public android.content.IntentSender getIntentSender() { return new android.content.IntentSender(); }
    public boolean isImmutable() { return true; } public boolean isActivity() { return mKind == 0; } public boolean isBroadcast() { return mKind == 1; } public boolean isService() { return mKind == 2; }
    public int describeContents() { return 0; }
}
