package android.app;
public class NotificationManager {
    public static final int IMPORTANCE_UNSPECIFIED = -1000, IMPORTANCE_NONE = 0, IMPORTANCE_MIN = 1, IMPORTANCE_LOW = 2, IMPORTANCE_DEFAULT = 3, IMPORTANCE_HIGH = 4, IMPORTANCE_MAX = 5,
        INTERRUPTION_FILTER_ALL = 1, INTERRUPTION_FILTER_PRIORITY = 2, INTERRUPTION_FILTER_NONE = 3, INTERRUPTION_FILTER_ALARMS = 4, INTERRUPTION_FILTER_UNKNOWN = 0;
    public static final String ACTION_NOTIFICATION_POLICY_ACCESS_GRANTED_CHANGED = "android.app.action.NOTIFICATION_POLICY_ACCESS_GRANTED_CHANGED", ACTION_APP_BLOCK_STATE_CHANGED = "android.app.action.APP_BLOCK_STATE_CHANGED";
    private final java.util.HashMap<String, NotificationChannel> mChannels = new java.util.HashMap<>();
    private final java.util.HashMap<String, NotificationChannelGroup> mGroups = new java.util.HashMap<>();
    public void notify(int id, Notification n) { notify(null, id, n); }
    public void notify(String tag, int id, Notification n) { android.util.Log.i("Husk", "notification " + id + ": " + (n != null ? n.huskText() : "")); }
    public void cancel(int id) {} public void cancel(String tag, int id) {} public void cancelAll() {}
    public void createNotificationChannel(NotificationChannel c) { mChannels.put(c.getId(), c); }
    public void createNotificationChannels(java.util.List<NotificationChannel> l) { for (NotificationChannel c : l) createNotificationChannel(c); }
    public void createNotificationChannelGroup(NotificationChannelGroup g) { mGroups.put(g.getId(), g); }
    public void createNotificationChannelGroups(java.util.List<NotificationChannelGroup> l) { for (NotificationChannelGroup g : l) createNotificationChannelGroup(g); }
    public NotificationChannel getNotificationChannel(String id) { return mChannels.get(id); }
    public java.util.List<NotificationChannel> getNotificationChannels() { return new java.util.ArrayList<>(mChannels.values()); }
    public java.util.List<NotificationChannelGroup> getNotificationChannelGroups() { return new java.util.ArrayList<>(mGroups.values()); }
    public void deleteNotificationChannel(String id) { mChannels.remove(id); }
    public void deleteNotificationChannelGroup(String id) { mGroups.remove(id); }
    public boolean areNotificationsEnabled() { return true; }
    public boolean areNotificationsPaused() { return false; }
    public int getImportance() { return IMPORTANCE_DEFAULT; }
    public android.service.notification.StatusBarNotification[] getActiveNotifications() { return new android.service.notification.StatusBarNotification[0]; }
    public boolean isNotificationPolicyAccessGranted() { return false; }
    public int getCurrentInterruptionFilter() { return INTERRUPTION_FILTER_ALL; }
    public boolean canNotifyAsPackage(String p) { return false; }
    public boolean canUseFullScreenIntent() { return true; }
}
