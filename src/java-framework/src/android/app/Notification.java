package android.app;

import android.graphics.drawable.Icon;
import android.os.Bundle;

public class Notification implements android.os.Parcelable {
    public static final int DEFAULT_ALL = -1, DEFAULT_SOUND = 1, DEFAULT_VIBRATE = 2, DEFAULT_LIGHTS = 4, FLAG_SHOW_LIGHTS = 1, FLAG_ONGOING_EVENT = 2, FLAG_INSISTENT = 4,
        FLAG_ONLY_ALERT_ONCE = 8, FLAG_AUTO_CANCEL = 16, FLAG_NO_CLEAR = 32, FLAG_FOREGROUND_SERVICE = 64, FLAG_HIGH_PRIORITY = 128, FLAG_LOCAL_ONLY = 256, FLAG_GROUP_SUMMARY = 512,
        PRIORITY_DEFAULT = 0, PRIORITY_LOW = -1, PRIORITY_MIN = -2, PRIORITY_HIGH = 1, PRIORITY_MAX = 2, VISIBILITY_PUBLIC = 1, VISIBILITY_PRIVATE = 0, VISIBILITY_SECRET = -1,
        BADGE_ICON_NONE = 0, BADGE_ICON_SMALL = 1, BADGE_ICON_LARGE = 2, GROUP_ALERT_ALL = 0, GROUP_ALERT_SUMMARY = 1, GROUP_ALERT_CHILDREN = 2,
        FOREGROUND_SERVICE_DEFAULT = 0, FOREGROUND_SERVICE_IMMEDIATE = 1, FOREGROUND_SERVICE_DEFERRED = 2, COLOR_DEFAULT = 0, STREAM_DEFAULT = -1;
    public static final String CATEGORY_CALL = "call", CATEGORY_MESSAGE = "msg", CATEGORY_EMAIL = "email", CATEGORY_EVENT = "event", CATEGORY_PROMO = "promo", CATEGORY_ALARM = "alarm",
        CATEGORY_PROGRESS = "progress", CATEGORY_SOCIAL = "social", CATEGORY_ERROR = "err", CATEGORY_TRANSPORT = "transport", CATEGORY_SYSTEM = "sys", CATEGORY_SERVICE = "service",
        CATEGORY_RECOMMENDATION = "recommendation", CATEGORY_STATUS = "status", CATEGORY_REMINDER = "reminder", CATEGORY_NAVIGATION = "navigation",
        EXTRA_TITLE = "android.title", EXTRA_TEXT = "android.text", EXTRA_SUB_TEXT = "android.subText", EXTRA_BIG_TEXT = "android.bigText", EXTRA_PROGRESS = "android.progress",
        EXTRA_PROGRESS_MAX = "android.progressMax", EXTRA_NOTIFICATION_ID = "android.intent.extra.NOTIFICATION_ID", EXTRA_CHANNEL_ID = "android.intent.extra.CHANNEL_ID",
        INTENT_CATEGORY_NOTIFICATION_PREFERENCES = "android.intent.category.NOTIFICATION_PREFERENCES";
    public int icon, iconLevel, number, flags, defaults, priority, visibility, color, audioStreamType, ledARGB, ledOnMS, ledOffMS;
    public long when;
    public CharSequence tickerText;
    public PendingIntent contentIntent, deleteIntent, fullScreenIntent;
    public Bundle extras = new Bundle();
    public String category;
    public Action[] actions;
    public android.net.Uri sound;
    public long[] vibrate;
    public android.media.AudioAttributes audioAttributes;
    public android.graphics.Bitmap largeIcon;
    public android.widget.RemoteViews contentView, bigContentView, headsUpContentView;
    public Notification publicVersion;
    public Notification() {}
    public Notification(int icon, CharSequence ticker, long when) { this.icon = icon; tickerText = ticker; this.when = when; }
    public String huskText() { CharSequence t = extras.getCharSequence(EXTRA_TITLE), x = extras.getCharSequence(EXTRA_TEXT); return (t != null ? t : "") + (x != null ? ": " + x : ""); }
    public String getChannelId() { return null; } public String getGroup() { return null; } public String getSortKey() { return null; } public Icon getSmallIcon() { return null; } public Icon getLargeIcon() { return null; }
    public long getTimeoutAfter() { return 0; } public int getBadgeIconType() { return 0; } public String getShortcutId() { return null; } public int getGroupAlertBehavior() { return 0; }
    public boolean getAllowSystemGeneratedContextualActions() { return true; } public BubbleMetadata getBubbleMetadata() { return null; } public android.content.LocusId getLocusId() { return null; }
    public int describeContents() { return 0; }
    public static class Action implements android.os.Parcelable {
        public int icon; public CharSequence title; public PendingIntent actionIntent;
        public Action(int icon, CharSequence title, PendingIntent intent) { this.icon = icon; this.title = title; actionIntent = intent; }
        public Bundle getExtras() { return new Bundle(); } public Icon getIcon() { return null; } public RemoteInput[] getRemoteInputs() { return null; }
        public boolean getAllowGeneratedReplies() { return false; } public int getSemanticAction() { return 0; } public boolean isContextual() { return false; } public boolean isAuthenticationRequired() { return false; }
        public int describeContents() { return 0; }
        public static final class Builder {
            private final int i; private final CharSequence t; private final PendingIntent p;
            public Builder(int icon, CharSequence title, PendingIntent intent) { i = icon; t = title; p = intent; } public Builder(Icon icon, CharSequence title, PendingIntent intent) { this(0, title, intent); } public Builder(Action a) { this(a.icon, a.title, a.actionIntent); }
            public Builder addExtras(Bundle b) { return this; } public Builder addRemoteInput(RemoteInput r) { return this; } public Builder setAllowGeneratedReplies(boolean b) { return this; }
            public Builder setSemanticAction(int a) { return this; } public Builder setContextual(boolean c) { return this; } public Builder setAuthenticationRequired(boolean b) { return this; }
            public Builder extend(Object e) { return this; } public Bundle getExtras() { return new Bundle(); } public Action build() { return new Action(i, t, p); }
        }
    }
    public static final class BubbleMetadata { public static final class Builder { public Builder(PendingIntent p, Icon i) {} public Builder(String s) {} public Builder setDesiredHeight(int h) { return this; } public Builder setAutoExpandBubble(boolean b) { return this; } public Builder setSuppressNotification(boolean b) { return this; } public BubbleMetadata build() { return new BubbleMetadata(); } } }
    public abstract static class Style { public Notification build() { return null; } public void setBuilder(Builder b) {} }
    public static class BigTextStyle extends Style { public BigTextStyle() {} public BigTextStyle(Builder b) {} public BigTextStyle bigText(CharSequence t) { return this; } public BigTextStyle setBigContentTitle(CharSequence t) { return this; } public BigTextStyle setSummaryText(CharSequence t) { return this; } }
    public static class BigPictureStyle extends Style { public BigPictureStyle() {} public BigPictureStyle(Builder b) {} public BigPictureStyle bigPicture(android.graphics.Bitmap b) { return this; } public BigPictureStyle bigPicture(Icon i) { return this; } public BigPictureStyle bigLargeIcon(android.graphics.Bitmap b) { return this; } public BigPictureStyle bigLargeIcon(Icon i) { return this; } public BigPictureStyle setBigContentTitle(CharSequence t) { return this; } public BigPictureStyle setSummaryText(CharSequence t) { return this; } public BigPictureStyle showBigPictureWhenCollapsed(boolean b) { return this; } public BigPictureStyle setContentDescription(CharSequence c) { return this; } }
    public static class InboxStyle extends Style { public InboxStyle() {} public InboxStyle(Builder b) {} public InboxStyle addLine(CharSequence l) { return this; } public InboxStyle setBigContentTitle(CharSequence t) { return this; } public InboxStyle setSummaryText(CharSequence t) { return this; } }
    public static class MessagingStyle extends Style {
        public MessagingStyle(CharSequence name) {} public MessagingStyle(Person p) {} public MessagingStyle addMessage(CharSequence t, long ts, CharSequence s) { return this; } public MessagingStyle addMessage(Message m) { return this; }
        public MessagingStyle addMessage(CharSequence t, long ts, Person s) { return this; } public MessagingStyle setConversationTitle(CharSequence t) { return this; } public MessagingStyle setGroupConversation(boolean g) { return this; }
        public MessagingStyle addHistoricMessage(Message m) { return this; } public CharSequence getConversationTitle() { return null; } public java.util.List<Message> getMessages() { return new java.util.ArrayList<>(); } public Person getUser() { return null; } public boolean isGroupConversation() { return false; }
        public static final class Message { public Message(CharSequence t, long ts, CharSequence s) {} public Message(CharSequence t, long ts, Person s) {} public Message setData(String m, android.net.Uri u) { return this; } public Bundle getExtras() { return new Bundle(); } }
    }
    public static class DecoratedCustomViewStyle extends Style {}
    public static class MediaStyle extends Style { public MediaStyle() {} public MediaStyle(Builder b) {} public MediaStyle setShowActionsInCompactView(int... a) { return this; } public MediaStyle setMediaSession(Object t) { return this; } }
    public static class CallStyle extends Style { public static CallStyle forIncomingCall(Person p, PendingIntent d, PendingIntent a) { return new CallStyle(); } public static CallStyle forOngoingCall(Person p, PendingIntent h) { return new CallStyle(); } public static CallStyle forScreeningCall(Person p, PendingIntent h, PendingIntent a) { return new CallStyle(); } public CallStyle setIsVideo(boolean v) { return this; } public CallStyle setVerificationIcon(Icon i) { return this; } public CallStyle setVerificationText(CharSequence t) { return this; } public CallStyle setAnswerButtonColorHint(int c) { return this; } public CallStyle setDeclineButtonColorHint(int c) { return this; } }
    public static class Builder {
        private final Notification n = new Notification();
        public Builder(android.content.Context c) {} public Builder(android.content.Context c, String channel) {}
        public Builder setWhen(long w) { n.when = w; return this; } public Builder setShowWhen(boolean s) { return this; } public Builder setUsesChronometer(boolean b) { return this; } public Builder setChronometerCountDown(boolean b) { return this; }
        public Builder setSmallIcon(int i) { n.icon = i; return this; } public Builder setSmallIcon(int i, int l) { n.icon = i; return this; } public Builder setSmallIcon(Icon i) { return this; }
        public Builder setContentTitle(CharSequence t) { n.extras.putCharSequence(EXTRA_TITLE, t); return this; } public Builder setContentText(CharSequence t) { n.extras.putCharSequence(EXTRA_TEXT, t); return this; }
        public Builder setSubText(CharSequence t) { return this; } public Builder setNumber(int x) { n.number = x; return this; } public Builder setContentInfo(CharSequence i) { return this; }
        public Builder setProgress(int max, int p, boolean ind) { return this; } public Builder setContent(android.widget.RemoteViews v) { return this; } public Builder setCustomContentView(android.widget.RemoteViews v) { return this; }
        public Builder setCustomBigContentView(android.widget.RemoteViews v) { return this; } public Builder setCustomHeadsUpContentView(android.widget.RemoteViews v) { return this; }
        public Builder setContentIntent(PendingIntent i) { n.contentIntent = i; return this; } public Builder setDeleteIntent(PendingIntent i) { return this; } public Builder setFullScreenIntent(PendingIntent i, boolean h) { return this; }
        public Builder setTicker(CharSequence t) { return this; } public Builder setLargeIcon(android.graphics.Bitmap b) { return this; } public Builder setLargeIcon(Icon i) { return this; }
        public Builder setSound(android.net.Uri u) { return this; } public Builder setSound(android.net.Uri u, int s) { return this; } public Builder setSound(android.net.Uri u, android.media.AudioAttributes a) { return this; }
        public Builder setVibrate(long[] p) { return this; } public Builder setLights(int a, int on, int off) { return this; } public Builder setOngoing(boolean o) { return this; } public Builder setColorized(boolean c) { return this; }
        public Builder setOnlyAlertOnce(boolean o) { return this; } public Builder setAutoCancel(boolean a) { return this; } public Builder setLocalOnly(boolean l) { return this; } public Builder setDefaults(int d) { return this; }
        public Builder setPriority(int p) { return this; } public Builder setCategory(String c) { n.category = c; return this; } public Builder addPerson(String p) { return this; } public Builder addPerson(Person p) { return this; }
        public Builder setGroup(String g) { return this; } public Builder setGroupSummary(boolean s) { return this; } public Builder setSortKey(String k) { return this; } public Builder addExtras(Bundle b) { n.extras.putAll(b); return this; }
        public Builder setExtras(Bundle b) { n.extras = b; return this; } public Bundle getExtras() { return n.extras; } public Builder addAction(int i, CharSequence t, PendingIntent p) { return this; } public Builder addAction(Action a) { return this; }
        public Builder setActions(Action... a) { return this; } public Builder setStyle(Style s) { return this; } public Builder setVisibility(int v) { return this; } public Builder setPublicVersion(Notification p) { return this; }
        public Builder extend(Object e) { return this; } public Builder setColor(int c) { return this; } public Builder setChannelId(String c) { return this; } public Builder setTimeoutAfter(long t) { return this; }
        public Builder setShortcutId(String s) { return this; } public Builder setLocusId(android.content.LocusId l) { return this; } public Builder setBadgeIconType(int t) { return this; } public Builder setGroupAlertBehavior(int b) { return this; }
        public Builder setBubbleMetadata(BubbleMetadata d) { return this; } public Builder setAllowSystemGeneratedContextualActions(boolean b) { return this; } public Builder setForegroundServiceBehavior(int b) { return this; }
        public Builder setRemoteInputHistory(CharSequence[] h) { return this; } public Builder setSettingsText(CharSequence t) { return this; } public Builder setFlag(int m, boolean v) { return this; }
        public Notification build() { return n; } @Deprecated public Notification getNotification() { return n; }
    }
}
