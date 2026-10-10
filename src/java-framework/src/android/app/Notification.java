package android.app;

import android.graphics.drawable.Icon;
import android.os.Bundle;

public class Notification implements android.os.Parcelable {
    @Deprecated public void setLatestEventInfo(android.content.Context context, CharSequence contentTitle, CharSequence contentText, PendingIntent contentIntent) {}
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
    public String huskText() { if (extras == null) return tickerText != null ? tickerText.toString() : ""; CharSequence t = extras.getCharSequence(EXTRA_TITLE), x = extras.getCharSequence(EXTRA_TEXT); return (t != null ? t : "") + (x != null ? ": " + x : ""); }
    public String getChannelId() { return null; } public String getGroup() { return null; } public String getSortKey() { return null; } public Icon getSmallIcon() { return null; } public Icon getLargeIcon() { return null; }
    public long getTimeoutAfter() { return 0; } public int getBadgeIconType() { return 0; } public String getShortcutId() { return null; } public int getGroupAlertBehavior() { return 0; }
    public boolean getAllowSystemGeneratedContextualActions() { return true; } public BubbleMetadata getBubbleMetadata() { return null; } public android.content.LocusId getLocusId() { return null; }
    public int describeContents() { return 0; }
    public static class Action implements android.os.Parcelable {
        public interface Extender { android.app.Notification.Action.Builder extend(android.app.Notification.Action.Builder b); }
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
            public Builder extend(Action.Extender e) { return e != null ? e.extend(this) : this; } public Bundle getExtras() { return new Bundle(); } public Action build() { return new Action(i, t, p); }
        }
        // ---- generated by tools/compat/fillmembers.py (Action): the platform's members this class does not write (signatures only)
        public static android.os.Parcelable.Creator CREATOR;
        public static final java.lang.String EXTRA_IS_ANIMATED = "android.extra.IS_ANIMATED";
        public static final int SEMANTIC_ACTION_ARCHIVE = 5;
        public static final int SEMANTIC_ACTION_CALL = 10;
        public static final int SEMANTIC_ACTION_CONVERSATION_IS_PHISHING = 12;
        public static final int SEMANTIC_ACTION_DELETE = 4;
        public static final int SEMANTIC_ACTION_MARK_AS_READ = 2;
        public static final int SEMANTIC_ACTION_MARK_AS_UNREAD = 3;
        public static final int SEMANTIC_ACTION_MARK_CONVERSATION_AS_PRIORITY = 11;
        public static final int SEMANTIC_ACTION_MUTE = 6;
        public static final int SEMANTIC_ACTION_NONE = 0;
        public static final int SEMANTIC_ACTION_REPLY = 1;
        public static final int SEMANTIC_ACTION_THUMBS_DOWN = 9;
        public static final int SEMANTIC_ACTION_THUMBS_UP = 8;
        public static final int SEMANTIC_ACTION_UNMUTE = 7;
        public android.app.RemoteInput[] getDataOnlyRemoteInputs() { return null; }
        // ---- end of generated members (Action)
    }
    public static final class BubbleMetadata { public static final class Builder { public Builder(PendingIntent p, Icon i) {} public Builder(String s) {} public Builder setDesiredHeight(int h) { return this; } public Builder setAutoExpandBubble(boolean b) { return this; } public Builder setSuppressNotification(boolean b) { return this; } public BubbleMetadata build() { return new BubbleMetadata(); }
        // ---- generated by tools/compat/fillmembers.py (BubbleMetadata.Builder): the platform's members this class does not write (signatures only)
        public android.app.Notification.BubbleMetadata.Builder setDeleteIntent(android.app.PendingIntent p0) { return this; }
        public android.app.Notification.BubbleMetadata.Builder setDesiredHeightResId(int p0) { return this; }
        public android.app.Notification.BubbleMetadata.Builder setFlag(int p0, boolean p1) { return this; }
        public android.app.Notification.BubbleMetadata.Builder setIcon(android.graphics.drawable.Icon p0) { return this; }
        public android.app.Notification.BubbleMetadata.Builder setIntent(android.app.PendingIntent p0) { return this; }
        public android.app.Notification.BubbleMetadata.Builder setSuppressableBubble(boolean p0) { return this; }
        // ---- end of generated members (BubbleMetadata.Builder)
    }
        // ---- generated by tools/compat/fillmembers.py (BubbleMetadata): the platform's members this class does not write (signatures only)
        private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
        public static android.os.Parcelable.Creator CREATOR;
        public static final int FLAG_AUTO_EXPAND_BUBBLE = 1;
        public static final int FLAG_SUPPRESSABLE_BUBBLE = 4;
        public static final int FLAG_SUPPRESS_BUBBLE = 8;
        public static final int FLAG_SUPPRESS_NOTIFICATION = 2;
        public int describeContents() { return 0; }
        public boolean getAutoExpandBubble() { return false; }
        public android.app.PendingIntent getDeleteIntent() { return null; }
        public int getDesiredHeight() { return 0; }
        public int getDesiredHeightResId() { return 0; }
        public int getFlags() { return (huskFill.get("Flags") instanceof Integer ? (Integer) huskFill.get("Flags") : 0); }
        public android.graphics.drawable.Icon getIcon() { return null; }
        public android.app.PendingIntent getIntent() { return null; }
        public java.lang.String getShortcutId() { return null; }
        public boolean isBubbleSuppressable() { return false; }
        public boolean isBubbleSuppressed() { return false; }
        public boolean isNotificationSuppressed() { return false; }
        public void setFlags(int p0) { huskFill.put("Flags", Integer.valueOf(p0)); }
        public void setSuppressBubble(boolean p0) {}
        public void writeToParcel(android.os.Parcel p0, int p1) {}
        // ---- end of generated members (BubbleMetadata)
    }
    public abstract static class Style { public Notification build() { return null; } public void setBuilder(Builder b) {}
        // ---- generated by tools/compat/fillmembers.py (Style): the platform's members this class does not write (signatures only)
        protected android.app.Notification.Builder mBuilder;
        protected java.lang.CharSequence mSummaryText;
        protected boolean mSummaryTextSet;
        public void addExtras(android.os.Bundle p0) {}
        public boolean areNotificationsVisiblyDifferent(android.app.Notification.Style p0) { return false; }
        public android.app.Notification buildStyled(android.app.Notification p0) { return null; }
        protected void checkBuilder() {}
        public boolean displayCustomViewInline() { return false; }
        public java.lang.CharSequence getHeadsUpStatusBarText() { return null; }
        protected android.widget.RemoteViews getStandardView(int p0) { return null; }
        public boolean hasSummaryInHeader() { return false; }
        protected void internalSetBigContentTitle(java.lang.CharSequence p0) {}
        protected void internalSetSummaryText(java.lang.CharSequence p0) {}
        public android.widget.RemoteViews makeCompactHeadsUpContentView() { return null; }
        public android.widget.RemoteViews makeContentView() { return null; }
        public android.widget.RemoteViews makeExpandedContentView() { return null; }
        public android.widget.RemoteViews makeHeadsUpContentView() { return null; }
        public void purgeResources() {}
        public void reduceImageSizes(android.content.Context p0) {}
        protected void restoreFromExtras(android.os.Bundle p0) {}
        public void validate(android.content.Context p0) {}
        // ---- end of generated members (Style)
    }
    public static class BigTextStyle extends Style { public BigTextStyle() {} public BigTextStyle(Builder b) {} public BigTextStyle bigText(CharSequence t) { return this; } public BigTextStyle setBigContentTitle(CharSequence t) { return this; } public BigTextStyle setSummaryText(CharSequence t) { return this; }
        // ---- generated by tools/compat/fillmembers.py (BigTextStyle): the platform's members this class does not write (signatures only)
        public void addExtras(android.os.Bundle p0) {}
        public boolean areNotificationsVisiblyDifferent(android.app.Notification.Style p0) { return false; }
        public java.lang.CharSequence getBigText() { return null; }
        public android.widget.RemoteViews makeExpandedContentView() { return null; }
        protected void restoreFromExtras(android.os.Bundle p0) {}
        // ---- end of generated members (BigTextStyle)
    }
    public static class BigPictureStyle extends Style { public BigPictureStyle() {} public BigPictureStyle(Builder b) {} public BigPictureStyle bigPicture(android.graphics.Bitmap b) { return this; } public BigPictureStyle bigPicture(Icon i) { return this; } public BigPictureStyle bigLargeIcon(android.graphics.Bitmap b) { return this; } public BigPictureStyle bigLargeIcon(Icon i) { return this; } public BigPictureStyle setBigContentTitle(CharSequence t) { return this; } public BigPictureStyle setSummaryText(CharSequence t) { return this; } public BigPictureStyle showBigPictureWhenCollapsed(boolean b) { return this; } public BigPictureStyle setContentDescription(CharSequence c) { return this; }
        // ---- generated by tools/compat/fillmembers.py (BigPictureStyle): the platform's members this class does not write (signatures only)
        public static final int MIN_ASHMEM_BITMAP_SIZE = 131072;
        public static android.graphics.drawable.Icon getPictureIcon(android.os.Bundle p0) { return null; }
        public void addExtras(android.os.Bundle p0) {}
        public boolean areNotificationsVisiblyDifferent(android.app.Notification.Style p0) { return false; }
        public android.graphics.drawable.Icon getBigPicture() { return null; }
        public boolean hasSummaryInHeader() { return false; }
        public android.widget.RemoteViews makeContentView() { return null; }
        public android.widget.RemoteViews makeExpandedContentView() { return null; }
        public android.widget.RemoteViews makeHeadsUpContentView() { return null; }
        public void purgeResources() {}
        public void reduceImageSizes(android.content.Context p0) {}
        protected void restoreFromExtras(android.os.Bundle p0) {}
        // ---- end of generated members (BigPictureStyle)
    }
    public static class InboxStyle extends Style { public InboxStyle() {} public InboxStyle(Builder b) {} public InboxStyle addLine(CharSequence l) { return this; } public InboxStyle setBigContentTitle(CharSequence t) { return this; } public InboxStyle setSummaryText(CharSequence t) { return this; }
        // ---- generated by tools/compat/fillmembers.py (InboxStyle): the platform's members this class does not write (signatures only)
        public void addExtras(android.os.Bundle p0) {}
        public boolean areNotificationsVisiblyDifferent(android.app.Notification.Style p0) { return false; }
        public java.util.ArrayList getLines() { return new java.util.ArrayList(); }
        public android.widget.RemoteViews makeExpandedContentView() { return null; }
        protected void restoreFromExtras(android.os.Bundle p0) {}
        // ---- end of generated members (InboxStyle)
    }
    public static class MessagingStyle extends Style {
        public MessagingStyle(CharSequence name) {} public MessagingStyle(Person p) {} public MessagingStyle addMessage(CharSequence t, long ts, CharSequence s) { return this; } public MessagingStyle addMessage(Message m) { return this; }
        public MessagingStyle addMessage(CharSequence t, long ts, Person s) { return this; } public MessagingStyle setConversationTitle(CharSequence t) { return this; } public MessagingStyle setGroupConversation(boolean g) { return this; }
        public MessagingStyle addHistoricMessage(Message m) { return this; } public CharSequence getConversationTitle() { return null; } public java.util.List<Message> getMessages() { return new java.util.ArrayList<>(); } public Person getUser() { return null; } public boolean isGroupConversation() { return false; }
        public static final class Message { public Message(CharSequence t, long ts, CharSequence s) {} public Message(CharSequence t, long ts, Person s) {} public Message setData(String m, android.net.Uri u) { return this; } public Bundle getExtras() { return new Bundle(); }
            // ---- generated by tools/compat/fillmembers.py (MessagingStyle.Message): the platform's members this class does not write (signatures only)
            public static final java.lang.String KEY_TEXT = "text";
            public static android.app.Notification.MessagingStyle.Message getMessageFromBundle(android.os.Bundle p0) { return null; }
            public static java.util.List getMessagesFromBundleArray(android.os.Parcelable[] p0) { return new java.util.ArrayList(); }
            public void ensureColorContrast(int p0) {}
            public void ensureColorContrastOrStripStyling(int p0) {}
            public java.lang.String getDataMimeType() { return null; }
            public android.net.Uri getDataUri() { return null; }
            public java.lang.CharSequence getSender() { return null; }
            public android.app.Person getSenderPerson() { return null; }
            public java.lang.CharSequence getText() { return null; }
            public long getTimestamp() { return 0L; }
            public boolean isRemoteInputHistory() { return false; }
            public android.os.Bundle toBundle() { return null; }
            public void visitUris(java.util.function.Consumer p0) {}
            // ---- end of generated members (MessagingStyle.Message)
        }
        // ---- generated by tools/compat/fillmembers.py (MessagingStyle): the platform's members this class does not write (signatures only)
        private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
        public static final int CONVERSATION_TYPE_IMPORTANT = 2;
        public static final int CONVERSATION_TYPE_LEGACY = 0;
        public static final int CONVERSATION_TYPE_NORMAL = 1;
        public static final int MAXIMUM_RETAINED_MESSAGES = 25;
        public static android.app.Notification.MessagingStyle.Message findLatestIncomingMessage(java.util.List p0) { return null; }
        public void addExtras(android.os.Bundle p0) {}
        public void addExtras(android.os.Bundle p0, boolean p1, int p2) {}
        public boolean areNotificationsVisiblyDifferent(android.app.Notification.Style p0) { return false; }
        public int getConversationType() { return (huskFill.get("ConversationType") instanceof Integer ? (Integer) huskFill.get("ConversationType") : 0); }
        public java.lang.CharSequence getHeadsUpStatusBarText() { return null; }
        public java.util.List getHistoricMessages() { return new java.util.ArrayList(); }
        public android.graphics.drawable.Icon getShortcutIcon() { return (android.graphics.drawable.Icon) huskFill.get("ShortcutIcon"); }
        public int getUnreadMessageCount() { return (huskFill.get("UnreadMessageCount") instanceof Integer ? (Integer) huskFill.get("UnreadMessageCount") : 0); }
        public java.lang.CharSequence getUserDisplayName() { return null; }
        public android.widget.RemoteViews makeCompactHeadsUpContentView() { return null; }
        public android.widget.RemoteViews makeContentView() { return null; }
        public android.widget.RemoteViews makeExpandedContentView() { return null; }
        public android.widget.RemoteViews makeHeadsUpContentView() { return null; }
        public void reduceImageSizes(android.content.Context p0) {}
        protected void restoreFromExtras(android.os.Bundle p0) {}
        public android.app.Notification.MessagingStyle setConversationType(int p0) { return this; }
        public android.app.Notification.MessagingStyle setShortcutIcon(android.graphics.drawable.Icon p0) { return this; }
        public android.app.Notification.MessagingStyle setUnreadMessageCount(int p0) { return this; }
        public void validate(android.content.Context p0) {}
        // ---- end of generated members (MessagingStyle)
    }
    public static class DecoratedCustomViewStyle extends Style {
        // ---- generated by tools/compat/fillmembers.py (DecoratedCustomViewStyle): the platform's members this class does not write (signatures only)
        public boolean areNotificationsVisiblyDifferent(android.app.Notification.Style p0) { return false; }
        public boolean displayCustomViewInline() { return false; }
        public android.widget.RemoteViews makeContentView() { return null; }
        public android.widget.RemoteViews makeExpandedContentView() { return null; }
        public android.widget.RemoteViews makeHeadsUpContentView() { return null; }
        // ---- end of generated members (DecoratedCustomViewStyle)
    }
    public static class MediaStyle extends Style { public MediaStyle() {} public MediaStyle(Builder b) {} public MediaStyle setShowActionsInCompactView(int... a) { return this; } public MediaStyle setMediaSession(android.media.session.MediaSession.Token t) { return this; }
        // ---- generated by tools/compat/fillmembers.py (MediaStyle): the platform's members this class does not write (signatures only)
        public void addExtras(android.os.Bundle p0) {}
        public boolean areNotificationsVisiblyDifferent(android.app.Notification.Style p0) { return false; }
        public android.app.Notification buildStyled(android.app.Notification p0) { return null; }
        public android.widget.RemoteViews makeContentView() { return null; }
        public android.widget.RemoteViews makeExpandedContentView() { return null; }
        public android.widget.RemoteViews makeHeadsUpContentView() { return null; }
        protected android.widget.RemoteViews makeMediaContentView(android.widget.RemoteViews p0) { return null; }
        protected android.widget.RemoteViews makeMediaExpandedContentView(android.widget.RemoteViews p0) { return null; }
        protected void restoreFromExtras(android.os.Bundle p0) {}
        public android.app.Notification.MediaStyle setRemotePlaybackInfo(java.lang.CharSequence p0, int p1, android.app.PendingIntent p2) { return this; }
        // ---- end of generated members (MediaStyle)
    }
    public static class CallStyle extends Style { public static CallStyle forIncomingCall(Person p, PendingIntent d, PendingIntent a) { return new CallStyle(); } public static CallStyle forOngoingCall(Person p, PendingIntent h) { return new CallStyle(); } public static CallStyle forScreeningCall(Person p, PendingIntent h, PendingIntent a) { return new CallStyle(); } public CallStyle setIsVideo(boolean v) { return this; } public CallStyle setVerificationIcon(Icon i) { return this; } public CallStyle setVerificationText(CharSequence t) { return this; } public CallStyle setAnswerButtonColorHint(int c) { return this; } public CallStyle setDeclineButtonColorHint(int c) { return this; }
        // ---- generated by tools/compat/fillmembers.py (CallStyle): the platform's members this class does not write (signatures only)
        public static final int CALL_TYPE_INCOMING = 1;
        public static final int CALL_TYPE_ONGOING = 2;
        public static final int CALL_TYPE_SCREENING = 3;
        public static final int CALL_TYPE_UNKNOWN = 0;
        public static final boolean DEBUG_NEW_ACTION_LAYOUT = true;
        public void addExtras(android.os.Bundle p0) {}
        public boolean areNotificationsVisiblyDifferent(android.app.Notification.Style p0) { return false; }
        public android.app.Notification buildStyled(android.app.Notification p0) { return null; }
        public boolean displayCustomViewInline() { return false; }
        public java.util.ArrayList getActionsListWithSystemActions() { return new java.util.ArrayList(); }
        public boolean hasSummaryInHeader() { return false; }
        public android.widget.RemoteViews makeCompactHeadsUpContentView() { return null; }
        public android.widget.RemoteViews makeContentView() { return null; }
        public android.widget.RemoteViews makeExpandedContentView() { return null; }
        public android.widget.RemoteViews makeHeadsUpContentView() { return null; }
        public void purgeResources() {}
        public void reduceImageSizes(android.content.Context p0) {}
        protected void restoreFromExtras(android.os.Bundle p0) {}
        // ---- end of generated members (CallStyle)
    }
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
        public Builder extend(Extender e) { return e != null ? e.extend(this) : this; } public Builder setColor(int c) { return this; } public Builder setChannelId(String c) { return this; } public Builder setTimeoutAfter(long t) { return this; }
        public Builder setShortcutId(String s) { return this; } public Builder setLocusId(android.content.LocusId l) { return this; } public Builder setBadgeIconType(int t) { return this; } public Builder setGroupAlertBehavior(int b) { return this; }
        public Builder setBubbleMetadata(BubbleMetadata d) { return this; } public Builder setAllowSystemGeneratedContextualActions(boolean b) { return this; } public Builder setForegroundServiceBehavior(int b) { return this; }
        public Builder setRemoteInputHistory(CharSequence[] h) { return this; } public Builder setSettingsText(CharSequence t) { return this; } public Builder setFlag(int m, boolean v) { return this; }
        public Notification build() { return n; } @Deprecated public Notification getNotification() { return n; }
        // ---- generated by tools/compat/fillmembers.py (Builder): the platform's members this class does not write (signatures only)
        public static final java.lang.String EXTRA_REBUILD_BIG_CONTENT_VIEW_ACTION_COUNT = "android.rebuild.bigViewActionCount";
        public static final java.lang.String EXTRA_REBUILD_CONTENT_VIEW_ACTION_COUNT = "android.rebuild.contentViewActionCount";
        public static final java.lang.String EXTRA_REBUILD_HEADS_UP_CONTENT_VIEW_ACTION_COUNT = "android.rebuild.hudViewActionCount";
        public static int ensureButtonFillContrast(int p0, int p1) { return 0; }
        public static int getContentMarginTop(android.content.Context p0, int p1) { return 0; }
        public static java.lang.Integer getFullLengthSpanColor(java.lang.CharSequence p0) { return null; }
        public static boolean isColorDark(int p0) { return false; }
        public static void makeHeaderExpanded(android.widget.RemoteViews p0) {}
        public static android.app.Notification maybeCloneStrippedForDelivery(android.app.Notification p0) { return null; }
        public static android.app.Notification.Builder recoverBuilder(android.content.Context p0, android.app.Notification p1) { return null; }
        public android.app.Notification buildInto(android.app.Notification p0) { return null; }
        public android.app.Notification buildUnstyled() { return null; }
        public android.widget.RemoteViews createBigContentView() { return null; }
        public android.widget.RemoteViews createCompactHeadsUpContentView() { return null; }
        public android.widget.RemoteViews createContentView() { return null; }
        public android.widget.RemoteViews createHeadsUpContentView() { return null; }
        public java.lang.CharSequence ensureColorSpanContrastOrStripStyling(java.lang.CharSequence p0, int p1) { return null; }
        public int getBackgroundColor(boolean p0) { return 0; }
        public java.lang.CharSequence getHeadsUpStatusBarText(boolean p0) { return null; }
        public int getSmallIconColor(boolean p0) { return 0; }
        public android.app.Notification.Style getStyle() { return null; }
        public java.lang.String loadHeaderAppName() { return null; }
        public android.widget.RemoteViews makeLowPriorityContentView(boolean p0) { return null; }
        public android.widget.RemoteViews makeNotificationGroupHeader() { return null; }
        public android.widget.RemoteViews makePublicContentView(boolean p0) { return null; }
        public android.app.Notification.Builder setChannel(java.lang.String p0) { return this; }
        public android.app.Notification.Builder setHasSummarizedContent(boolean p0) { return this; }
        public android.app.Notification.Builder setHideSmartReplies(boolean p0) { return this; }
        public android.app.Notification.Builder setRequestPromotedOngoing(boolean p0) { return this; }
        public android.app.Notification.Builder setShortCriticalText(java.lang.String p0) { return this; }
        public android.app.Notification.Builder setShowRemoteInputSpinner(boolean p0) { return this; }
        public android.app.Notification.Builder setSilent(boolean p0) { return this; }
        public android.app.Notification.Builder setSummarizedContent(java.lang.CharSequence p0) { return this; }
        public android.app.Notification.Builder setTicker(java.lang.CharSequence p0, android.widget.RemoteViews p1) { return this; }
        public android.app.Notification.Builder setTimeout(long p0) { return this; }
        public boolean usesStandardHeader() { return false; }
        public boolean usesTemplate() { return false; }
        // ---- end of generated members (Builder)
    }
    // ---- platform API stubs (tools/compat/genstubs.py)
    public interface Extender {
        android.app.Notification.Builder extend(android.app.Notification.Builder p0);
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public static android.media.AudioAttributes AUDIO_ATTRIBUTES_DEFAULT;
    public static final java.lang.String CATEGORY_CAR_EMERGENCY = "car_emergency";
    public static final java.lang.String CATEGORY_CAR_INFORMATION = "car_information";
    public static final java.lang.String CATEGORY_CAR_WARNING = "car_warning";
    public static final java.lang.String CATEGORY_LOCATION_SHARING = "location_sharing";
    public static final java.lang.String CATEGORY_MISSED_CALL = "missed_call";
    public static final java.lang.String CATEGORY_STOPWATCH = "stopwatch";
    public static final java.lang.String CATEGORY_VOICEMAIL = "voicemail";
    public static final java.lang.String CATEGORY_WORKOUT = "workout";
    public static final int COLOR_INVALID = 1;
    public static final java.lang.String EXTRA_ALLOW_DURING_SETUP = "android.allowDuringSetup";
    public static final java.lang.String EXTRA_ANSWER_COLOR = "android.answerColor";
    public static final java.lang.String EXTRA_ANSWER_INTENT = "android.answerIntent";
    public static final java.lang.String EXTRA_APP_SUMMARIZATION = "android.app.extra.app_summarization";
    public static final java.lang.String EXTRA_AUDIO_CONTENTS_URI = "android.audioContents";
    public static final java.lang.String EXTRA_BACKGROUND_IMAGE_URI = "android.backgroundImageUri";
    public static final java.lang.String EXTRA_BUILDER_APPLICATION_INFO = "android.appInfo";
    public static final java.lang.String EXTRA_CALL_IS_VIDEO = "android.callIsVideo";
    public static final java.lang.String EXTRA_CALL_PERSON = "android.callPerson";
    public static final java.lang.String EXTRA_CALL_TYPE = "android.callType";
    public static final java.lang.String EXTRA_CHANNEL_GROUP_ID = "android.intent.extra.CHANNEL_GROUP_ID";
    public static final java.lang.String EXTRA_CHRONOMETER_COUNT_DOWN = "android.chronometerCountDown";
    public static final java.lang.String EXTRA_COLORIZED = "android.colorized";
    public static final java.lang.String EXTRA_COMPACT_ACTIONS = "android.compactActions";
    public static final java.lang.String EXTRA_CONTAINS_CUSTOM_VIEW = "android.contains.customView";
    public static final java.lang.String EXTRA_CONTAINS_SUMMARIZATION = "android.app.extra.contains_summarization";
    public static final java.lang.String EXTRA_CONVERSATION_ICON = "android.conversationIcon";
    public static final java.lang.String EXTRA_CONVERSATION_TITLE = "android.conversationTitle";
    public static final java.lang.String EXTRA_CONVERSATION_UNREAD_MESSAGE_COUNT = "android.conversationUnreadMessageCount";
    public static final java.lang.String EXTRA_DECLINE_COLOR = "android.declineColor";
    public static final java.lang.String EXTRA_DECLINE_INTENT = "android.declineIntent";
    public static final java.lang.String EXTRA_FOREGROUND_APPS = "android.foregroundApps";
    public static final java.lang.String EXTRA_HANG_UP_INTENT = "android.hangUpIntent";
    public static final java.lang.String EXTRA_HIDE_SMART_REPLIES = "android.hideSmartReplies";
    public static final java.lang.String EXTRA_HIDE_STATUS_BAR_NOTIFICATION = "android.hideStatusBarNotification";
    public static final java.lang.String EXTRA_HISTORIC_MESSAGES = "android.messages.historic";
    public static final java.lang.String EXTRA_INFO_TEXT = "android.infoText";
    public static final java.lang.String EXTRA_IS_GROUP_CONVERSATION = "android.isGroupConversation";
    public static final java.lang.String EXTRA_LARGE_ICON = "android.largeIcon";
    public static final java.lang.String EXTRA_LARGE_ICON_BIG = "android.largeIcon.big";
    public static final java.lang.String EXTRA_MEDIA_REMOTE_DEVICE = "android.mediaRemoteDevice";
    public static final java.lang.String EXTRA_MEDIA_REMOTE_ICON = "android.mediaRemoteIcon";
    public static final java.lang.String EXTRA_MEDIA_REMOTE_INTENT = "android.mediaRemoteIntent";
    public static final java.lang.String EXTRA_MEDIA_SESSION = "android.mediaSession";
    public static final java.lang.String EXTRA_MESSAGES = "android.messages";
    public static final java.lang.String EXTRA_MESSAGING_PERSON = "android.messagingUser";
    public static final java.lang.String EXTRA_NOTIFICATION_TAG = "android.intent.extra.NOTIFICATION_TAG";
    public static final java.lang.String EXTRA_PEOPLE = "android.people";
    public static final java.lang.String EXTRA_PEOPLE_LIST = "android.people.list";
    public static final java.lang.String EXTRA_PICTURE = "android.picture";
    public static final java.lang.String EXTRA_PICTURE_CONTENT_DESCRIPTION = "android.pictureContentDescription";
    public static final java.lang.String EXTRA_PICTURE_ICON = "android.pictureIcon";
    public static final java.lang.String EXTRA_PREFER_SMALL_ICON = "android.app.preferSmallIcon";
    public static final java.lang.String EXTRA_PROGRESS_END_ICON = "android.progressEndIcon";
    public static final java.lang.String EXTRA_PROGRESS_INDETERMINATE = "android.progressIndeterminate";
    public static final java.lang.String EXTRA_PROGRESS_POINTS = "android.progressPoints";
    public static final java.lang.String EXTRA_PROGRESS_SEGMENTS = "android.progressSegments";
    public static final java.lang.String EXTRA_PROGRESS_START_ICON = "android.progressStartIcon";
    public static final java.lang.String EXTRA_PROGRESS_TRACKER_ICON = "android.progressTrackerIcon";
    public static final java.lang.String EXTRA_REDUCED_IMAGES = "android.reduced.images";
    public static final java.lang.String EXTRA_REMOTE_INPUT_DRAFT = "android.remoteInputDraft";
    public static final java.lang.String EXTRA_REMOTE_INPUT_HISTORY = "android.remoteInputHistory";
    public static final java.lang.String EXTRA_REMOTE_INPUT_HISTORY_ITEMS = "android.remoteInputHistoryItems";
    public static final java.lang.String EXTRA_REQUEST_PROMOTED_ONGOING = "android.requestPromotedOngoing";
    public static final java.lang.String EXTRA_SELF_DISPLAY_NAME = "android.selfDisplayName";
    public static final java.lang.String EXTRA_SHORT_CRITICAL_TEXT = "android.shortCriticalText";
    public static final java.lang.String EXTRA_SHOW_BIG_PICTURE_WHEN_COLLAPSED = "android.showBigPictureWhenCollapsed";
    public static final java.lang.String EXTRA_SHOW_CHRONOMETER = "android.showChronometer";
    public static final java.lang.String EXTRA_SHOW_REMOTE_INPUT_SPINNER = "android.remoteInputSpinner";
    public static final java.lang.String EXTRA_SHOW_WHEN = "android.showWhen";
    public static final java.lang.String EXTRA_SMALL_ICON = "android.icon";
    public static final java.lang.String EXTRA_STYLED_BY_PROGRESS = "android.styledByProgress";
    public static final java.lang.String EXTRA_SUBSTITUTE_APP_NAME = "android.substName";
    public static final java.lang.String EXTRA_SUMMARIZED_CONTENT = "android.summarization";
    public static final java.lang.String EXTRA_SUMMARY_TEXT = "android.summaryText";
    public static final java.lang.String EXTRA_TEMPLATE = "android.template";
    public static final java.lang.String EXTRA_TEXT_LINES = "android.textLines";
    public static final java.lang.String EXTRA_TITLE_BIG = "android.title.big";
    public static final java.lang.String EXTRA_VERIFICATION_ICON = "android.verificationIcon";
    public static final java.lang.String EXTRA_VERIFICATION_TEXT = "android.verificationText";
    public static final int FLAG_AUTOGROUP_SUMMARY = 1024;
    public static final int FLAG_BUBBLE = 4096;
    public static final int FLAG_CAN_COLORIZE = 2048;
    public static final int FLAG_FSI_REQUESTED_BUT_DENIED = 16384;
    public static final int FLAG_LIFETIME_EXTENDED_BY_DIRECT_REPLY = 65536;
    public static final int FLAG_NO_DISMISS = 8192;
    public static final int FLAG_PROMOTED_ONGOING = 262144;
    public static final int FLAG_SILENT = 131072;
    public static final int FLAG_USER_INITIATED_JOB = 32768;
    public static final java.lang.String GROUP_KEY_SILENT = "silent";
    public static final int MAX_ACTION_BUTTONS = 3;
    public static final int PROGRESS_STATE_COMPLETE = 2;
    public static final int PROGRESS_STATE_NONE = 0;
    public static final int PROGRESS_STATE_ONGOING = 1;
    public static android.os.IBinder processAllowlistToken;
    public static java.util.function.LongSupplier sElapsedRealtimeClock;
    public android.util.ArraySet allPendingIntents;
    public long creationTime;
    public android.widget.RemoteViews tickerView;
    public Notification(android.content.Context p0, int p1, java.lang.CharSequence p2, long p3, java.lang.CharSequence p4, java.lang.CharSequence p5, android.content.Intent p6) { this(); }
    public Notification(android.os.Parcel p0) { this(); }
    public static void addFieldsFromContext(android.content.Context p0, android.app.Notification p1) {}
    public static void addFieldsFromContext(android.content.pm.ApplicationInfo p0, android.app.Notification p1) {}
    public static boolean areActionsVisiblyDifferent(android.app.Notification p0, android.app.Notification p1) { return false; }
    public static boolean areIconsDifferent(android.app.Notification p0, android.app.Notification p1) { return false; }
    public static boolean areRemoteViewsChanged(android.app.Notification.Builder p0, android.app.Notification.Builder p1) { return false; }
    public static boolean areStyledNotificationsVisiblyDifferent(android.app.Notification.Builder p0, android.app.Notification.Builder p1) { return false; }
    public static java.lang.String defaultsToString(int p0) { return null; }
    public static java.lang.String flagsToString(int p0) { return null; }
    public static java.lang.Class getNotificationStyleClass(java.lang.String p0) { return null; }
    public static android.graphics.Bitmap getProfileBadge(android.content.Context p0) { return null; }
    public static java.lang.String priorityToString(int p0) { return null; }
    public static java.lang.CharSequence safeCharSequence(java.lang.CharSequence p0) { return null; }
    public static java.lang.String safeCharSequenceToString(java.lang.CharSequence p0) { return null; }
    public static java.lang.String safeString(java.lang.String p0) { return null; }
    public static java.lang.String visibilityToString(int p0) { return null; }
    public void cloneInto(android.app.Notification p0, boolean p1) {}
    public boolean containsCustomViews() { return false; }
    public void dumpDebug(android.util.proto.ProtoOutputStream p0, long p1) {}
    public android.util.Pair findRemoteInputActionPair(boolean p0) { return null; }
    public void fixSilentGroup() {}
    public android.os.IBinder getAllowlistToken() { return null; }
    public java.lang.String getChannel() { return null; }
    public java.util.List getContextualActions() { return new java.util.ArrayList(); }
    public java.lang.Class getNotificationStyle() { return null; }
    public int getProgressState() { return 0; }
    public java.lang.CharSequence getSettingsText() { return null; }
    public java.lang.String getShortCriticalText() { return null; }
    public java.lang.CharSequence getSummarizedContent() { return null; }
    public long getTimeout() { return 0L; }
    public long getWhen() { return 0L; }
    public boolean hasAppProvidedWhen() { return false; }
    public boolean hasColorizedPermission() { return false; }
    public boolean hasCompletedProgress() { return false; }
    public boolean hasImage() { return false; }
    public boolean hasPromotableCharacteristics() { return false; }
    public boolean hasPromotableStyle() { return false; }
    public boolean hasSummarizedContent() { return false; }
    public boolean hasTitle() { return false; }
    public boolean isBubbleNotification() { return false; }
    public boolean isColorized() { return false; }
    public java.lang.Boolean isCustomNotification() { return null; }
    public boolean isFgsOrUij() { return false; }
    public boolean isForegroundDisplayForceDeferred() { return false; }
    public boolean isForegroundService() { return false; }
    public boolean isGroupChild() { return false; }
    public boolean isGroupSummary() { return false; }
    public boolean isMediaNotification() { return false; }
    public boolean isOngoingEvent() { return false; }
    public boolean isPromotedOngoing() { return false; }
    public boolean isRequestPromotedOngoing() { return false; }
    public boolean isSilent() { return false; }
    public boolean isStyle(java.lang.Class p0) { return false; }
    public boolean isUserInitiatedJob() { return false; }
    public void lightenPayload() {}
    public java.lang.String loadHeaderAppName(android.content.Context p0) { return null; }
    public void overrideAllowlistToken(android.os.IBinder p0) {}
    public boolean shouldShowForegroundImmediately() { return false; }
    public boolean showsChronometer() { return false; }
    public boolean showsTime() { return false; }
    public boolean suppressAlertingDueToGrouping() { return false; }
    // ---- end of generated members
}
