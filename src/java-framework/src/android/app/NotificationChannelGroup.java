package android.app;
public final class NotificationChannelGroup implements android.os.Parcelable {
    private final String mId; private final CharSequence mName;
    public NotificationChannelGroup(String id, CharSequence name) { mId = id; mName = name; }
    public String getId() { return mId; } public CharSequence getName() { return mName; } public void setDescription(String d) {}
    public java.util.List<NotificationChannel> getChannels() { return new java.util.ArrayList<>(); } public boolean isBlocked() { return false; }
    public int describeContents() { return 0; }
}
