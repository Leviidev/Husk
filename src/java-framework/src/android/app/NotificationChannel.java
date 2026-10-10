package android.app;
public final class NotificationChannel implements android.os.Parcelable {
    public static final String DEFAULT_CHANNEL_ID = "miscellaneous";
    private final String mId; private CharSequence mName; private int mImportance; private String mDesc, mGroup;
    public NotificationChannel(String id, CharSequence name, int importance) { mId = id; mName = name; mImportance = importance; }
    public String getId() { return mId; } public CharSequence getName() { return mName; } public void setName(CharSequence n) { mName = n; }
    public int getImportance() { return mImportance; } public void setImportance(int i) { mImportance = i; }
    public String getDescription() { return mDesc; } public void setDescription(String d) { mDesc = d; }
    public String getGroup() { return mGroup; } public void setGroup(String g) { mGroup = g; }
    public void setShowBadge(boolean b) {} public void setSound(android.net.Uri u, android.media.AudioAttributes a) {} public void enableLights(boolean b) {} public void setLightColor(int c) {}
    public void enableVibration(boolean b) {} public void setVibrationPattern(long[] p) {} public void setLockscreenVisibility(int v) {} public void setBypassDnd(boolean b) {}
    public boolean canShowBadge() { return true; } public android.net.Uri getSound() { return null; } public boolean shouldVibrate() { return false; } public int getLockscreenVisibility() { return 0; }
    public void setAllowBubbles(boolean b) {} public void setBlockable(boolean b) {} public void setConversationId(String p, String c) {}
    public int describeContents() { return 0; }
}
