package android.content;

import android.net.Uri;
import android.os.Bundle;

public class Intent implements Cloneable, android.os.Parcelable {
    public static final String ACTION_VIEW = "android.intent.action.VIEW", ACTION_MAIN = "android.intent.action.MAIN", ACTION_EDIT = "android.intent.action.EDIT",
        ACTION_PICK = "android.intent.action.PICK", ACTION_SEND = "android.intent.action.SEND", ACTION_SENDTO = "android.intent.action.SENDTO",
        ACTION_SEND_MULTIPLE = "android.intent.action.SEND_MULTIPLE", ACTION_DIAL = "android.intent.action.DIAL", ACTION_CALL = "android.intent.action.CALL",
        ACTION_GET_CONTENT = "android.intent.action.GET_CONTENT", ACTION_OPEN_DOCUMENT = "android.intent.action.OPEN_DOCUMENT", ACTION_CREATE_DOCUMENT = "android.intent.action.CREATE_DOCUMENT",
        ACTION_OPEN_DOCUMENT_TREE = "android.intent.action.OPEN_DOCUMENT_TREE", ACTION_CHOOSER = "android.intent.action.CHOOSER", ACTION_INSERT = "android.intent.action.INSERT",
        ACTION_DELETE = "android.intent.action.DELETE", ACTION_SEARCH = "android.intent.action.SEARCH", ACTION_WEB_SEARCH = "android.intent.action.WEB_SEARCH",
        ACTION_BATTERY_CHANGED = "android.intent.action.BATTERY_CHANGED", ACTION_BOOT_COMPLETED = "android.intent.action.BOOT_COMPLETED", ACTION_SCREEN_ON = "android.intent.action.SCREEN_ON",
        ACTION_SCREEN_OFF = "android.intent.action.SCREEN_OFF", ACTION_USER_PRESENT = "android.intent.action.USER_PRESENT", ACTION_TIME_TICK = "android.intent.action.TIME_TICK",
        ACTION_TIMEZONE_CHANGED = "android.intent.action.TIMEZONE_CHANGED", ACTION_LOCALE_CHANGED = "android.intent.action.LOCALE_CHANGED", ACTION_CONFIGURATION_CHANGED = "android.intent.action.CONFIGURATION_CHANGED",
        ACTION_PACKAGE_ADDED = "android.intent.action.PACKAGE_ADDED", ACTION_PACKAGE_REMOVED = "android.intent.action.PACKAGE_REMOVED", ACTION_PACKAGE_REPLACED = "android.intent.action.PACKAGE_REPLACED",
        ACTION_MY_PACKAGE_REPLACED = "android.intent.action.MY_PACKAGE_REPLACED", ACTION_HEADSET_PLUG = "android.intent.action.HEADSET_PLUG", ACTION_MEDIA_BUTTON = "android.intent.action.MEDIA_BUTTON",
        ACTION_POWER_CONNECTED = "android.intent.action.ACTION_POWER_CONNECTED", ACTION_POWER_DISCONNECTED = "android.intent.action.ACTION_POWER_DISCONNECTED",
        ACTION_BATTERY_LOW = "android.intent.action.BATTERY_LOW", ACTION_BATTERY_OKAY = "android.intent.action.BATTERY_OKAY", ACTION_CLOSE_SYSTEM_DIALOGS = "android.intent.action.CLOSE_SYSTEM_DIALOGS",
        ACTION_APPLICATION_DETAILS_SETTINGS_ = "android.settings.APPLICATION_DETAILS_SETTINGS", ACTION_INSTALL_PACKAGE = "android.intent.action.INSTALL_PACKAGE",
        ACTION_DATE_CHANGED = "android.intent.action.DATE_CHANGED", ACTION_TIME_CHANGED = "android.intent.action.TIME_SET", ACTION_AIRPLANE_MODE_CHANGED = "android.intent.action.AIRPLANE_MODE",
        ACTION_CREATE_SHORTCUT = "android.intent.action.CREATE_SHORTCUT", ACTION_ASSIST = "android.intent.action.ASSIST", ACTION_APP_ERROR = "android.intent.action.APP_ERROR",
        ACTION_SET_WALLPAPER = "android.intent.action.SET_WALLPAPER", ACTION_PROCESS_TEXT = "android.intent.action.PROCESS_TEXT", ACTION_SHOW_APP_INFO = "android.intent.action.SHOW_APP_INFO",
        ACTION_MANAGE_PACKAGE_STORAGE = "android.intent.action.MANAGE_PACKAGE_STORAGE", ACTION_ATTACH_DATA = "android.intent.action.ATTACH_DATA", ACTION_RUN = "android.intent.action.RUN";
    public static final String CATEGORY_LAUNCHER = "android.intent.category.LAUNCHER", CATEGORY_DEFAULT = "android.intent.category.DEFAULT", CATEGORY_BROWSABLE = "android.intent.category.BROWSABLE",
        CATEGORY_OPENABLE = "android.intent.category.OPENABLE", CATEGORY_HOME = "android.intent.category.HOME", CATEGORY_ALTERNATIVE = "android.intent.category.ALTERNATIVE",
        CATEGORY_SELECTED_ALTERNATIVE = "android.intent.category.SELECTED_ALTERNATIVE", CATEGORY_INFO = "android.intent.category.INFO", CATEGORY_LEANBACK_LAUNCHER = "android.intent.category.LEANBACK_LAUNCHER",
        CATEGORY_APP_BROWSER = "android.intent.category.APP_BROWSER", CATEGORY_APP_EMAIL = "android.intent.category.APP_EMAIL", CATEGORY_APP_MARKET = "android.intent.category.APP_MARKET",
        CATEGORY_TAB = "android.intent.category.TAB", CATEGORY_PREFERENCE = "android.intent.category.PREFERENCE", CATEGORY_MONKEY = "android.intent.category.MONKEY";
    public static final String EXTRA_TEXT = "android.intent.extra.TEXT", EXTRA_HTML_TEXT = "android.intent.extra.HTML_TEXT", EXTRA_SUBJECT = "android.intent.extra.SUBJECT",
        EXTRA_EMAIL = "android.intent.extra.EMAIL", EXTRA_CC = "android.intent.extra.CC", EXTRA_BCC = "android.intent.extra.BCC", EXTRA_STREAM = "android.intent.extra.STREAM",
        EXTRA_TITLE = "android.intent.extra.TITLE", EXTRA_INTENT = "android.intent.extra.INTENT", EXTRA_INITIAL_INTENTS = "android.intent.extra.INITIAL_INTENTS",
        EXTRA_CHOOSER_TARGETS = "android.intent.extra.CHOOSER_TARGETS", EXTRA_MIME_TYPES = "android.intent.extra.MIME_TYPES", EXTRA_ALLOW_MULTIPLE = "android.intent.extra.ALLOW_MULTIPLE",
        EXTRA_LOCAL_ONLY = "android.intent.extra.LOCAL_ONLY", EXTRA_PHONE_NUMBER = "android.intent.extra.PHONE_NUMBER", EXTRA_UID = "android.intent.extra.UID",
        EXTRA_PACKAGE_NAME = "android.intent.extra.PACKAGE_NAME", EXTRA_REFERRER = "android.intent.extra.REFERRER", EXTRA_KEY_EVENT = "android.intent.extra.KEY_EVENT",
        EXTRA_SHORTCUT_NAME = "android.intent.extra.shortcut.NAME", EXTRA_SHORTCUT_INTENT = "android.intent.extra.shortcut.INTENT", EXTRA_SHORTCUT_ICON = "android.intent.extra.shortcut.ICON",
        EXTRA_SHORTCUT_ICON_RESOURCE = "android.intent.extra.shortcut.ICON_RESOURCE", EXTRA_PROCESS_TEXT = "android.intent.extra.PROCESS_TEXT", EXTRA_PROCESS_TEXT_READONLY = "android.intent.extra.PROCESS_TEXT_READONLY",
        EXTRA_RETURN_RESULT = "android.intent.extra.RETURN_RESULT", EXTRA_REPLACING = "android.intent.extra.REPLACING", EXTRA_COMPONENT_NAME = "android.intent.extra.COMPONENT_NAME",
        EXTRA_ALARM_COUNT = "android.intent.extra.ALARM_COUNT", EXTRA_DOCK_STATE = "android.intent.extra.DOCK_STATE", EXTRA_ASSIST_PACKAGE = "android.intent.extra.ASSIST_PACKAGE",
        EXTRA_CHOSEN_COMPONENT = "android.intent.extra.CHOSEN_COMPONENT", EXTRA_EXCLUDE_COMPONENTS = "android.intent.extra.EXCLUDE_COMPONENTS", EXTRA_TIMEZONE = "time-zone",
        EXTRA_USER = "android.intent.extra.USER", EXTRA_QUIET_MODE = "android.intent.extra.QUIET_MODE", EXTRA_CONTENT_ANNOTATIONS = "android.intent.extra.CONTENT_ANNOTATIONS",
        EXTRA_AUTO_LAUNCH_SINGLE_CHOICE = "android.intent.extra.AUTO_LAUNCH_SINGLE_CHOICE", EXTRA_NOT_UNKNOWN_SOURCE = "android.intent.extra.NOT_UNKNOWN_SOURCE";
    public static final int FLAG_GRANT_READ_URI_PERMISSION = 1, FLAG_GRANT_WRITE_URI_PERMISSION = 2, FLAG_FROM_BACKGROUND = 4, FLAG_DEBUG_LOG_RESOLUTION = 8,
        FLAG_EXCLUDE_STOPPED_PACKAGES = 16, FLAG_INCLUDE_STOPPED_PACKAGES = 32, FLAG_GRANT_PERSISTABLE_URI_PERMISSION = 64, FLAG_GRANT_PREFIX_URI_PERMISSION = 128,
        FLAG_DIRECT_BOOT_AUTO = 256, FLAG_ACTIVITY_MATCH_EXTERNAL = 2048, FLAG_ACTIVITY_REQUIRE_NON_BROWSER = 1024, FLAG_ACTIVITY_REQUIRE_DEFAULT = 512,
        FLAG_ACTIVITY_LAUNCH_ADJACENT = 4096, FLAG_ACTIVITY_RETAIN_IN_RECENTS = 8192, FLAG_ACTIVITY_TASK_ON_HOME = 16384, FLAG_ACTIVITY_CLEAR_TASK = 32768,
        FLAG_ACTIVITY_NO_ANIMATION = 65536, FLAG_ACTIVITY_REORDER_TO_FRONT = 131072, FLAG_ACTIVITY_NO_USER_ACTION = 262144, FLAG_ACTIVITY_CLEAR_WHEN_TASK_RESET = 524288,
        FLAG_ACTIVITY_NEW_DOCUMENT = 524288, FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY = 1048576, FLAG_ACTIVITY_RESET_TASK_IF_NEEDED = 2097152, FLAG_ACTIVITY_BROUGHT_TO_FRONT = 4194304,
        FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS = 8388608, FLAG_ACTIVITY_PREVIOUS_IS_TOP = 16777216, FLAG_ACTIVITY_FORWARD_RESULT = 33554432, FLAG_ACTIVITY_CLEAR_TOP = 67108864,
        FLAG_ACTIVITY_MULTIPLE_TASK = 134217728, FLAG_ACTIVITY_NEW_TASK = 268435456, FLAG_ACTIVITY_SINGLE_TOP = 536870912, FLAG_ACTIVITY_NO_HISTORY = 1073741824,
        FLAG_RECEIVER_REGISTERED_ONLY = 1073741824, FLAG_RECEIVER_REPLACE_PENDING = 536870912, FLAG_RECEIVER_FOREGROUND = 268435456, FLAG_RECEIVER_NO_ABORT = 134217728,
        FLAG_RECEIVER_VISIBLE_TO_INSTANT_APPS = 2097152, URI_INTENT_SCHEME = 1, URI_ANDROID_APP_SCHEME = 2, URI_ALLOW_UNSAFE = 4, FILL_IN_ACTION = 1, FILL_IN_DATA = 2,
        FILL_IN_CATEGORIES = 4, FILL_IN_COMPONENT = 8, FILL_IN_PACKAGE = 16, FILL_IN_SOURCE_BOUNDS = 32, FILL_IN_SELECTOR = 64, FILL_IN_CLIP_DATA = 128;
    public static final android.os.Parcelable.Creator<Intent> CREATOR = new android.os.Parcelable.Creator<Intent>() { public Intent createFromParcel(android.os.Parcel p) { return (Intent) p.readValue(null); } public Intent[] newArray(int n) { return new Intent[n]; } };
    private String action, type, pkg, identifier;
    private Uri data;
    private Bundle extras;
    private ComponentName component;
    private int flags;
    private java.util.LinkedHashSet<String> categories;
    private ClipData clip;
    private Intent selector;
    private android.graphics.Rect sourceBounds;
    public Intent() {}
    public Intent(String action) { this.action = action; }
    public Intent(String action, Uri data) { this.action = action; this.data = data; }
    public Intent(Context c, Class<?> cls) { component = new ComponentName(c, cls); }
    public Intent(String action, Uri data, Context c, Class<?> cls) { this(action, data); component = new ComponentName(c, cls); }
    public Intent(Intent o) {
        action = o.action; data = o.data; type = o.type; pkg = o.pkg; extras = o.extras == null ? null : new Bundle(o.extras); flags = o.flags; component = o.component;
        categories = o.categories == null ? null : new java.util.LinkedHashSet<>(o.categories); clip = o.clip; identifier = o.identifier;
    }
    @Override public Object clone() { return new Intent(this); }
    public Intent cloneFilter() { Intent i = new Intent(action, data); i.type = type; i.component = component; i.pkg = pkg; i.categories = categories; return i; }
    public static Intent createChooser(Intent target, CharSequence title) { Intent i = new Intent(ACTION_CHOOSER); i.putExtra(EXTRA_INTENT, target); if (title != null) i.putExtra(EXTRA_TITLE, title); return i; }
    public static Intent createChooser(Intent target, CharSequence title, IntentSender s) { return createChooser(target, title); }
    public static Intent makeMainActivity(ComponentName c) { Intent i = new Intent(ACTION_MAIN); i.setComponent(c); i.addCategory(CATEGORY_LAUNCHER); return i; }
    public static Intent makeMainSelectorActivity(String action, String category) { Intent i = new Intent(action); i.addCategory(category); return i; }
    public static Intent makeRestartActivityTask(ComponentName c) { Intent i = makeMainActivity(c); i.addFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_CLEAR_TASK); return i; }
    public static Intent parseUri(String uri, int flags) throws java.net.URISyntaxException {
        if (uri.startsWith("intent:") || uri.startsWith("#Intent;")) {
            Intent i = new Intent(ACTION_VIEW);
            int h = uri.indexOf("#Intent;");
            if (h > 7) i.setData(Uri.parse(uri.substring(0, h).replaceFirst("^intent:", "")));
            if (h >= 0) for (String part : uri.substring(h + 8).split(";")) {
                int eq = part.indexOf('='); if (eq < 0) continue; String k = part.substring(0, eq), v = Uri.decode(part.substring(eq + 1));
                if (k.equals("action")) i.setAction(v); else if (k.equals("category")) i.addCategory(v); else if (k.equals("type")) i.type = v; else if (k.equals("package")) i.pkg = v;
                else if (k.equals("component")) i.setComponent(ComponentName.unflattenFromString(v)); else if (k.equals("scheme") && i.data != null) i.data = Uri.parse(v + ":" + i.data);
                else if (k.startsWith("S.")) i.putExtra(k.substring(2), v); else if (k.startsWith("i.")) i.putExtra(k.substring(2), Integer.parseInt(v)); else if (k.startsWith("B.")) i.putExtra(k.substring(2), Boolean.parseBoolean(v));
            }
            return i;
        }
        return new Intent(ACTION_VIEW, Uri.parse(uri));
    }
    public static Intent getIntent(String uri) throws java.net.URISyntaxException { return parseUri(uri, 0); }
    public static Intent getIntentOld(String uri) throws java.net.URISyntaxException { return parseUri(uri, 0); }
    public String toUri(int flags) { return data != null ? data.toString() : "#Intent;" + (action != null ? "action=" + action + ";" : "") + "end"; }
    public String getAction() { return action; }
    public Intent setAction(String a) { action = a; return this; }
    public Uri getData() { return data; }
    public Intent setData(Uri d) { data = d; type = null; return this; }
    public Intent setDataAndNormalize(Uri d) { return setData(d == null ? null : d.normalizeScheme()); }
    public String getDataString() { return data == null ? null : data.toString(); }
    public String getScheme() { return data == null ? null : data.getScheme(); }
    public String getType() { return type; }
    public Intent setType(String t) { data = null; type = t; return this; }
    public Intent setTypeAndNormalize(String t) { return setType(t == null ? null : t.toLowerCase(java.util.Locale.ROOT)); }
    public Intent setDataAndType(Uri d, String t) { data = d; type = t; return this; }
    public Intent setDataAndTypeAndNormalize(Uri d, String t) { return setDataAndType(d, t); }
    public String resolveType(Context c) { return type != null ? type : data != null && "content".equals(data.getScheme()) ? c.getContentResolver().getType(data) : null; }
    public String resolveType(ContentResolver r) { return type != null ? type : data != null && "content".equals(data.getScheme()) ? r.getType(data) : null; }
    public String resolveTypeIfNeeded(ContentResolver r) { return component != null ? type : resolveType(r); }
    public String getIdentifier() { return identifier; }
    public Intent setIdentifier(String id) { identifier = id; return this; }
    public Intent addFlags(int f) { flags |= f; return this; }
    public Intent setFlags(int f) { flags = f; return this; }
    public void removeFlags(int f) { flags &= ~f; }
    public int getFlags() { return flags; }
    public boolean hasCategory(String c) { return categories != null && categories.contains(c); }
    public java.util.Set<String> getCategories() { return categories; }
    public Intent addCategory(String c) { if (categories == null) categories = new java.util.LinkedHashSet<>(); categories.add(c); return this; }
    public void removeCategory(String c) { if (categories != null) categories.remove(c); }
    public Intent getSelector() { return selector; }
    public void setSelector(Intent s) { selector = s; }
    public ClipData getClipData() { return clip; }
    public void setClipData(ClipData c) { clip = c; }
    public android.graphics.Rect getSourceBounds() { return sourceBounds; }
    public void setSourceBounds(android.graphics.Rect r) { sourceBounds = r; }
    public String getPackage() { return pkg; }
    public Intent setPackage(String p) { pkg = p; return this; }
    public Intent setClassName(String pkgName, String cls) { component = new ComponentName(pkgName, cls); return this; }
    public Intent setClassName(Context c, String cls) { component = new ComponentName(c, cls); return this; }
    public Intent setClass(Context c, Class<?> cls) { component = new ComponentName(c, cls); return this; }
    public Intent setComponent(ComponentName c) { component = c; return this; }
    public ComponentName getComponent() { return component; }
    public ComponentName resolveActivity(android.content.pm.PackageManager pm) { if (component != null) return component; android.content.pm.ResolveInfo r = pm.resolveActivity(this, 0); return r == null ? null : new ComponentName(r.activityInfo.packageName, r.activityInfo.name); }
    public android.content.pm.ActivityInfo resolveActivityInfo(android.content.pm.PackageManager pm, int flags) { android.content.pm.ResolveInfo r = pm.resolveActivity(this, flags); return r == null ? null : r.activityInfo; }
    public Bundle getExtras() { return extras == null ? null : new Bundle(extras); }
    private Bundle ex() { if (extras == null) extras = new Bundle(); return extras; }
    public boolean hasExtra(String k) { return extras != null && extras.containsKey(k); }
    public boolean hasFileDescriptors() { return false; }
    public void setExtrasClassLoader(ClassLoader l) {}
    public Intent putExtra(String k, boolean v) { ex().putBoolean(k, v); return this; }
    public Intent putExtra(String k, byte v) { ex().putByte(k, v); return this; }
    public Intent putExtra(String k, char v) { ex().putChar(k, v); return this; }
    public Intent putExtra(String k, short v) { ex().putShort(k, v); return this; }
    public Intent putExtra(String k, int v) { ex().putInt(k, v); return this; }
    public Intent putExtra(String k, long v) { ex().putLong(k, v); return this; }
    public Intent putExtra(String k, float v) { ex().putFloat(k, v); return this; }
    public Intent putExtra(String k, double v) { ex().putDouble(k, v); return this; }
    public Intent putExtra(String k, String v) { ex().putString(k, v); return this; }
    public Intent putExtra(String k, CharSequence v) { ex().putCharSequence(k, v); return this; }
    public Intent putExtra(String k, android.os.Parcelable v) { ex().putParcelable(k, v); return this; }
    public Intent putExtra(String k, android.os.Parcelable[] v) { ex().putParcelableArray(k, v); return this; }
    public Intent putParcelableArrayListExtra(String k, java.util.ArrayList<? extends android.os.Parcelable> v) { ex().putParcelableArrayList(k, v); return this; }
    public Intent putIntegerArrayListExtra(String k, java.util.ArrayList<Integer> v) { ex().putIntegerArrayList(k, v); return this; }
    public Intent putStringArrayListExtra(String k, java.util.ArrayList<String> v) { ex().putStringArrayList(k, v); return this; }
    public Intent putCharSequenceArrayListExtra(String k, java.util.ArrayList<CharSequence> v) { ex().putCharSequenceArrayList(k, v); return this; }
    public Intent putExtra(String k, java.io.Serializable v) { ex().putSerializable(k, v); return this; }
    public Intent putExtra(String k, boolean[] v) { ex().putBooleanArray(k, v); return this; }
    public Intent putExtra(String k, byte[] v) { ex().putByteArray(k, v); return this; }
    public Intent putExtra(String k, short[] v) { ex().putShortArray(k, v); return this; }
    public Intent putExtra(String k, char[] v) { ex().putCharArray(k, v); return this; }
    public Intent putExtra(String k, int[] v) { ex().putIntArray(k, v); return this; }
    public Intent putExtra(String k, long[] v) { ex().putLongArray(k, v); return this; }
    public Intent putExtra(String k, float[] v) { ex().putFloatArray(k, v); return this; }
    public Intent putExtra(String k, double[] v) { ex().putDoubleArray(k, v); return this; }
    public Intent putExtra(String k, String[] v) { ex().putStringArray(k, v); return this; }
    public Intent putExtra(String k, CharSequence[] v) { ex().putCharSequenceArray(k, v); return this; }
    public Intent putExtra(String k, Bundle v) { ex().putBundle(k, v); return this; }
    public Intent putExtras(Intent src) { if (src.extras != null) ex().putAll(src.extras); return this; }
    public Intent putExtras(Bundle b) { if (b != null) ex().putAll(b); return this; }
    public Intent replaceExtras(Intent src) { extras = src.extras == null ? null : new Bundle(src.extras); return this; }
    public Intent replaceExtras(Bundle b) { extras = b == null ? null : new Bundle(b); return this; }
    public void removeExtra(String k) { if (extras != null) extras.remove(k); }
    public boolean getBooleanExtra(String k, boolean d) { return extras == null ? d : extras.getBoolean(k, d); }
    public byte getByteExtra(String k, byte d) { return extras == null ? d : extras.getByte(k, d); }
    public short getShortExtra(String k, short d) { return extras == null ? d : extras.getShort(k, d); }
    public char getCharExtra(String k, char d) { return extras == null ? d : extras.getChar(k, d); }
    public int getIntExtra(String k, int d) { return extras == null ? d : extras.getInt(k, d); }
    public long getLongExtra(String k, long d) { return extras == null ? d : extras.getLong(k, d); }
    public float getFloatExtra(String k, float d) { return extras == null ? d : extras.getFloat(k, d); }
    public double getDoubleExtra(String k, double d) { return extras == null ? d : extras.getDouble(k, d); }
    public String getStringExtra(String k) { return extras == null ? null : extras.getString(k); }
    public CharSequence getCharSequenceExtra(String k) { return extras == null ? null : extras.getCharSequence(k); }
    public <T extends android.os.Parcelable> T getParcelableExtra(String k) { return extras == null ? null : extras.<T>getParcelable(k); }
    public <T> T getParcelableExtra(String k, Class<T> c) { return extras == null ? null : extras.getParcelable(k, c); }
    public android.os.Parcelable[] getParcelableArrayExtra(String k) { return extras == null ? null : extras.getParcelableArray(k); }
    public <T extends android.os.Parcelable> java.util.ArrayList<T> getParcelableArrayListExtra(String k) { return extras == null ? null : extras.<T>getParcelableArrayList(k); }
    public <T> java.util.ArrayList<T> getParcelableArrayListExtra(String k, Class<? extends T> c) { return extras == null ? null : extras.getParcelableArrayList(k, c); }
    public java.io.Serializable getSerializableExtra(String k) { return extras == null ? null : extras.getSerializable(k); }
    public <T extends java.io.Serializable> T getSerializableExtra(String k, Class<T> c) { return extras == null ? null : extras.getSerializable(k, c); }
    public java.util.ArrayList<Integer> getIntegerArrayListExtra(String k) { return extras == null ? null : extras.getIntegerArrayList(k); }
    public java.util.ArrayList<String> getStringArrayListExtra(String k) { return extras == null ? null : extras.getStringArrayList(k); }
    public java.util.ArrayList<CharSequence> getCharSequenceArrayListExtra(String k) { return extras == null ? null : extras.getCharSequenceArrayList(k); }
    public boolean[] getBooleanArrayExtra(String k) { return extras == null ? null : extras.getBooleanArray(k); }
    public byte[] getByteArrayExtra(String k) { return extras == null ? null : extras.getByteArray(k); }
    public short[] getShortArrayExtra(String k) { return extras == null ? null : extras.getShortArray(k); }
    public char[] getCharArrayExtra(String k) { return extras == null ? null : extras.getCharArray(k); }
    public int[] getIntArrayExtra(String k) { return extras == null ? null : extras.getIntArray(k); }
    public long[] getLongArrayExtra(String k) { return extras == null ? null : extras.getLongArray(k); }
    public float[] getFloatArrayExtra(String k) { return extras == null ? null : extras.getFloatArray(k); }
    public double[] getDoubleArrayExtra(String k) { return extras == null ? null : extras.getDoubleArray(k); }
    public String[] getStringArrayExtra(String k) { return extras == null ? null : extras.getStringArray(k); }
    public CharSequence[] getCharSequenceArrayExtra(String k) { return extras == null ? null : extras.getCharSequenceArray(k); }
    public Bundle getBundleExtra(String k) { return extras == null ? null : extras.getBundle(k); }
    public int fillIn(Intent o, int flags) { if (o.action != null && action == null) action = o.action; if (o.data != null && data == null) data = o.data; if (o.extras != null) ex().putAll(o.extras); return 0; }
    public boolean filterEquals(Intent o) { return o != null && java.util.Objects.equals(action, o.action) && java.util.Objects.equals(data, o.data) && java.util.Objects.equals(type, o.type) && java.util.Objects.equals(component, o.component) && java.util.Objects.equals(categories, o.categories); }
    public int filterHashCode() { return java.util.Objects.hash(action, data, type, component, categories); }
    public int describeContents() { return 0; }
    public void writeToParcel(android.os.Parcel p, int f) { p.writeValue(this); }
    @Override public String toString() { return "Intent { " + (action != null ? "act=" + action + " " : "") + (categories != null ? "cat=" + categories + " " : "") + (data != null ? "dat=" + data + " " : "") + (type != null ? "typ=" + type + " " : "") + (component != null ? "cmp=" + component.flattenToShortString() + " " : "") + (extras != null ? "(has extras) " : "") + "}"; }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
    public static final java.lang.String ACTION_ACTIVITY_RECOGNIZER = "android.intent.action.ACTIVITY_RECOGNIZER";
    public static final java.lang.String ACTION_ADVANCED_SETTINGS_CHANGED = "android.intent.action.ADVANCED_SETTINGS";
    public static final java.lang.String ACTION_ALARM_CHANGED = "android.intent.action.ALARM_CHANGED";
    public static final java.lang.String ACTION_ALL_APPS = "android.intent.action.ALL_APPS";
    public static final java.lang.String ACTION_ANSWER = "android.intent.action.ANSWER";
    public static final java.lang.String ACTION_APPLICATION_LOCALE_CHANGED = "android.intent.action.APPLICATION_LOCALE_CHANGED";
    public static final java.lang.String ACTION_APPLICATION_PREFERENCES = "android.intent.action.APPLICATION_PREFERENCES";
    public static final java.lang.String ACTION_APPLICATION_RESTRICTIONS_CHANGED = "android.intent.action.APPLICATION_RESTRICTIONS_CHANGED";
    public static final java.lang.String ACTION_AUTO_REVOKE_PERMISSIONS = "android.intent.action.AUTO_REVOKE_PERMISSIONS";
    public static final java.lang.String ACTION_BATTERY_LEVEL_CHANGED = "android.intent.action.BATTERY_LEVEL_CHANGED";
    public static final java.lang.String ACTION_BUG_REPORT = "android.intent.action.BUG_REPORT";
    public static final java.lang.String ACTION_CALL_BUTTON = "android.intent.action.CALL_BUTTON";
    public static final java.lang.String ACTION_CALL_EMERGENCY = "android.intent.action.CALL_EMERGENCY";
    public static final java.lang.String ACTION_CALL_PRIVILEGED = "android.intent.action.CALL_PRIVILEGED";
    public static final java.lang.String ACTION_CAMERA_BUTTON = "android.intent.action.CAMERA_BUTTON";
    public static final java.lang.String ACTION_CANCEL_ENABLE_ROLLBACK = "android.intent.action.CANCEL_ENABLE_ROLLBACK";
    public static final java.lang.String ACTION_CARRIER_SETUP = "android.intent.action.CARRIER_SETUP";
    public static final java.lang.String ACTION_CREATE_NOTE = "android.intent.action.CREATE_NOTE";
    public static final java.lang.String ACTION_CREATE_REMINDER = "android.intent.action.CREATE_REMINDER";
    public static final java.lang.String ACTION_DEFAULT = "android.intent.action.VIEW";
    public static final java.lang.String ACTION_DEFINE = "android.intent.action.DEFINE";
    public static final java.lang.String ACTION_DEVICE_CUSTOMIZATION_READY = "android.intent.action.DEVICE_CUSTOMIZATION_READY";
    public static final java.lang.String ACTION_DEVICE_INITIALIZATION_WIZARD = "android.intent.action.DEVICE_INITIALIZATION_WIZARD";
    public static final java.lang.String ACTION_DEVICE_LOCKED_CHANGED = "android.intent.action.DEVICE_LOCKED_CHANGED";
    public static final java.lang.String ACTION_DEVICE_STORAGE_FULL = "android.intent.action.DEVICE_STORAGE_FULL";
    public static final java.lang.String ACTION_DEVICE_STORAGE_LOW = "android.intent.action.DEVICE_STORAGE_LOW";
    public static final java.lang.String ACTION_DEVICE_STORAGE_NOT_FULL = "android.intent.action.DEVICE_STORAGE_NOT_FULL";
    public static final java.lang.String ACTION_DEVICE_STORAGE_OK = "android.intent.action.DEVICE_STORAGE_OK";
    public static final java.lang.String ACTION_DIAL_EMERGENCY = "android.intent.action.DIAL_EMERGENCY";
    public static final java.lang.String ACTION_DISMISS_KEYBOARD_SHORTCUTS = "com.android.intent.action.DISMISS_KEYBOARD_SHORTCUTS";
    public static final java.lang.String ACTION_DISTRACTING_PACKAGES_CHANGED = "android.intent.action.DISTRACTING_PACKAGES_CHANGED";
    public static final java.lang.String ACTION_DOCK_ACTIVE = "android.intent.action.DOCK_ACTIVE";
    public static final java.lang.String ACTION_DOCK_EVENT = "android.intent.action.DOCK_EVENT";
    public static final java.lang.String ACTION_DOCK_IDLE = "android.intent.action.DOCK_IDLE";
    public static final java.lang.String ACTION_DOMAINS_NEED_VERIFICATION = "android.intent.action.DOMAINS_NEED_VERIFICATION";
    public static final java.lang.String ACTION_DREAMING_STARTED = "android.intent.action.DREAMING_STARTED";
    public static final java.lang.String ACTION_DREAMING_STOPPED = "android.intent.action.DREAMING_STOPPED";
    public static final java.lang.String ACTION_DYNAMIC_SENSOR_CHANGED = "android.intent.action.DYNAMIC_SENSOR_CHANGED";
    public static final java.lang.String ACTION_EXTERNAL_APPLICATIONS_AVAILABLE = "android.intent.action.EXTERNAL_APPLICATIONS_AVAILABLE";
    public static final java.lang.String ACTION_EXTERNAL_APPLICATIONS_UNAVAILABLE = "android.intent.action.EXTERNAL_APPLICATIONS_UNAVAILABLE";
    public static final java.lang.String ACTION_FACTORY_RESET = "android.intent.action.FACTORY_RESET";
    public static final java.lang.String ACTION_FACTORY_TEST = "android.intent.action.FACTORY_TEST";
    public static final java.lang.String ACTION_GET_RESTRICTION_ENTRIES = "android.intent.action.GET_RESTRICTION_ENTRIES";
    public static final java.lang.String ACTION_GLOBAL_BUTTON = "android.intent.action.GLOBAL_BUTTON";
    public static final java.lang.String ACTION_GTALK_SERVICE_CONNECTED = "android.intent.action.GTALK_CONNECTED";
    public static final java.lang.String ACTION_GTALK_SERVICE_DISCONNECTED = "android.intent.action.GTALK_DISCONNECTED";
    public static final java.lang.String ACTION_IDLE_MAINTENANCE_END = "android.intent.action.ACTION_IDLE_MAINTENANCE_END";
    public static final java.lang.String ACTION_IDLE_MAINTENANCE_START = "android.intent.action.ACTION_IDLE_MAINTENANCE_START";
    public static final java.lang.String ACTION_INCIDENT_REPORT_READY = "android.intent.action.INCIDENT_REPORT_READY";
    public static final java.lang.String ACTION_INPUT_METHOD_CHANGED = "android.intent.action.INPUT_METHOD_CHANGED";
    public static final java.lang.String ACTION_INSERT_OR_EDIT = "android.intent.action.INSERT_OR_EDIT";
    public static final java.lang.String ACTION_INSTALL_FAILURE = "android.intent.action.INSTALL_FAILURE";
    public static final java.lang.String ACTION_INSTALL_INSTANT_APP_PACKAGE = "android.intent.action.INSTALL_INSTANT_APP_PACKAGE";
    public static final java.lang.String ACTION_INSTANT_APP_RESOLVER_SETTINGS = "android.intent.action.INSTANT_APP_RESOLVER_SETTINGS";
    public static final java.lang.String ACTION_INTENT_FILTER_NEEDS_VERIFICATION = "android.intent.action.INTENT_FILTER_NEEDS_VERIFICATION";
    public static final java.lang.String ACTION_LAUNCH_CAPTURE_CONTENT_ACTIVITY_FOR_NOTE = "android.intent.action.LAUNCH_CAPTURE_CONTENT_ACTIVITY_FOR_NOTE";
    public static final java.lang.String ACTION_LOAD_DATA = "android.intent.action.LOAD_DATA";
    public static final java.lang.String ACTION_LOCKED_BOOT_COMPLETED = "android.intent.action.LOCKED_BOOT_COMPLETED";
    public static final java.lang.String ACTION_MAIN_USER_LOCKSCREEN_KNOWLEDGE_FACTOR_CHANGED = "android.intent.action.MAIN_USER_LOCKSCREEN_KNOWLEDGE_FACTOR_CHANGED";
    public static final java.lang.String ACTION_MANAGED_PROFILE_ADDED = "android.intent.action.MANAGED_PROFILE_ADDED";
    public static final java.lang.String ACTION_MANAGED_PROFILE_AVAILABLE = "android.intent.action.MANAGED_PROFILE_AVAILABLE";
    public static final java.lang.String ACTION_MANAGED_PROFILE_REMOVED = "android.intent.action.MANAGED_PROFILE_REMOVED";
    public static final java.lang.String ACTION_MANAGED_PROFILE_UNAVAILABLE = "android.intent.action.MANAGED_PROFILE_UNAVAILABLE";
    public static final java.lang.String ACTION_MANAGED_PROFILE_UNLOCKED = "android.intent.action.MANAGED_PROFILE_UNLOCKED";
    public static final java.lang.String ACTION_MANAGE_APP_PERMISSION = "android.intent.action.MANAGE_APP_PERMISSION";
    public static final java.lang.String ACTION_MANAGE_APP_PERMISSIONS = "android.intent.action.MANAGE_APP_PERMISSIONS";
    public static final java.lang.String ACTION_MANAGE_DEFAULT_APP = "android.intent.action.MANAGE_DEFAULT_APP";
    public static final java.lang.String ACTION_MANAGE_NETWORK_USAGE = "android.intent.action.MANAGE_NETWORK_USAGE";
    public static final java.lang.String ACTION_MANAGE_PERMISSIONS = "android.intent.action.MANAGE_PERMISSIONS";
    public static final java.lang.String ACTION_MANAGE_PERMISSION_APPS = "android.intent.action.MANAGE_PERMISSION_APPS";
    public static final java.lang.String ACTION_MANAGE_PERMISSION_USAGE = "android.intent.action.MANAGE_PERMISSION_USAGE";
    public static final java.lang.String ACTION_MANAGE_SPECIAL_APP_ACCESSES = "android.intent.action.MANAGE_SPECIAL_APP_ACCESSES";
    public static final java.lang.String ACTION_MANAGE_UNUSED_APPS = "android.intent.action.MANAGE_UNUSED_APPS";
    public static final java.lang.String ACTION_MASTER_CLEAR = "android.intent.action.MASTER_CLEAR";
    public static final java.lang.String ACTION_MASTER_CLEAR_NOTIFICATION = "android.intent.action.MASTER_CLEAR_NOTIFICATION";
    public static final java.lang.String ACTION_MEDIA_BAD_REMOVAL = "android.intent.action.MEDIA_BAD_REMOVAL";
    public static final java.lang.String ACTION_MEDIA_CHECKING = "android.intent.action.MEDIA_CHECKING";
    public static final java.lang.String ACTION_MEDIA_EJECT = "android.intent.action.MEDIA_EJECT";
    public static final java.lang.String ACTION_MEDIA_MOUNTED = "android.intent.action.MEDIA_MOUNTED";
    public static final java.lang.String ACTION_MEDIA_NOFS = "android.intent.action.MEDIA_NOFS";
    public static final java.lang.String ACTION_MEDIA_REMOVED = "android.intent.action.MEDIA_REMOVED";
    public static final java.lang.String ACTION_MEDIA_RESOURCE_GRANTED = "android.intent.action.MEDIA_RESOURCE_GRANTED";
    public static final java.lang.String ACTION_MEDIA_SCANNER_FINISHED = "android.intent.action.MEDIA_SCANNER_FINISHED";
    public static final java.lang.String ACTION_MEDIA_SCANNER_SCAN_FILE = "android.intent.action.MEDIA_SCANNER_SCAN_FILE";
    public static final java.lang.String ACTION_MEDIA_SCANNER_STARTED = "android.intent.action.MEDIA_SCANNER_STARTED";
    public static final java.lang.String ACTION_MEDIA_SHARED = "android.intent.action.MEDIA_SHARED";
    public static final java.lang.String ACTION_MEDIA_UNMOUNTABLE = "android.intent.action.MEDIA_UNMOUNTABLE";
    public static final java.lang.String ACTION_MEDIA_UNMOUNTED = "android.intent.action.MEDIA_UNMOUNTED";
    public static final java.lang.String ACTION_MEDIA_UNSHARED = "android.intent.action.MEDIA_UNSHARED";
    public static final java.lang.String ACTION_MY_PACKAGE_SUSPENDED = "android.intent.action.MY_PACKAGE_SUSPENDED";
    public static final java.lang.String ACTION_MY_PACKAGE_UNSUSPENDED = "android.intent.action.MY_PACKAGE_UNSUSPENDED";
    public static final java.lang.String ACTION_NEW_OUTGOING_CALL = "android.intent.action.NEW_OUTGOING_CALL";
    public static final java.lang.String ACTION_OPEN_EYE_DROPPER = "android.intent.action.OPEN_EYE_DROPPER";
    public static final java.lang.String ACTION_OVERLAY_CHANGED = "android.intent.action.OVERLAY_CHANGED";
    public static final java.lang.String ACTION_PACKAGES_SUSPENDED = "android.intent.action.PACKAGES_SUSPENDED";
    public static final java.lang.String ACTION_PACKAGES_SUSPENSION_CHANGED = "android.intent.action.PACKAGES_SUSPENSION_CHANGED";
    public static final java.lang.String ACTION_PACKAGES_UNSUSPENDED = "android.intent.action.PACKAGES_UNSUSPENDED";
    public static final java.lang.String ACTION_PACKAGE_CHANGED = "android.intent.action.PACKAGE_CHANGED";
    public static final java.lang.String ACTION_PACKAGE_DATA_CLEARED = "android.intent.action.PACKAGE_DATA_CLEARED";
    public static final java.lang.String ACTION_PACKAGE_ENABLE_ROLLBACK = "android.intent.action.PACKAGE_ENABLE_ROLLBACK";
    public static final java.lang.String ACTION_PACKAGE_FIRST_LAUNCH = "android.intent.action.PACKAGE_FIRST_LAUNCH";
    public static final java.lang.String ACTION_PACKAGE_FULLY_REMOVED = "android.intent.action.PACKAGE_FULLY_REMOVED";
    public static final java.lang.String ACTION_PACKAGE_INSTALL = "android.intent.action.PACKAGE_INSTALL";
    public static final java.lang.String ACTION_PACKAGE_NEEDS_INTEGRITY_VERIFICATION = "android.intent.action.PACKAGE_NEEDS_INTEGRITY_VERIFICATION";
    public static final java.lang.String ACTION_PACKAGE_NEEDS_VERIFICATION = "android.intent.action.PACKAGE_NEEDS_VERIFICATION";
    public static final java.lang.String ACTION_PACKAGE_REMOVED_INTERNAL = "android.intent.action.PACKAGE_REMOVED_INTERNAL";
    public static final java.lang.String ACTION_PACKAGE_RESTARTED = "android.intent.action.PACKAGE_RESTARTED";
    public static final java.lang.String ACTION_PACKAGE_UNSTOPPED = "android.intent.action.PACKAGE_UNSTOPPED";
    public static final java.lang.String ACTION_PACKAGE_UNSUSPENDED_MANUALLY = "android.intent.action.PACKAGE_UNSUSPENDED_MANUALLY";
    public static final java.lang.String ACTION_PACKAGE_VERIFIED = "android.intent.action.PACKAGE_VERIFIED";
    public static final java.lang.String ACTION_PASTE = "android.intent.action.PASTE";
    public static final java.lang.String ACTION_PENDING_INCIDENT_REPORTS_CHANGED = "android.intent.action.PENDING_INCIDENT_REPORTS_CHANGED";
    public static final java.lang.String ACTION_PICK_ACTIVITY = "android.intent.action.PICK_ACTIVITY";
    public static final java.lang.String ACTION_POWER_USAGE_SUMMARY = "android.intent.action.POWER_USAGE_SUMMARY";
    public static final java.lang.String ACTION_PREFERRED_ACTIVITY_CHANGED = "android.intent.action.ACTION_PREFERRED_ACTIVITY_CHANGED";
    public static final java.lang.String ACTION_PRE_BOOT_COMPLETED = "android.intent.action.PRE_BOOT_COMPLETED";
    public static final java.lang.String ACTION_PROFILE_ACCESSIBLE = "android.intent.action.PROFILE_ACCESSIBLE";
    public static final java.lang.String ACTION_PROFILE_ADDED = "android.intent.action.PROFILE_ADDED";
    public static final java.lang.String ACTION_PROFILE_AVAILABLE = "android.intent.action.PROFILE_AVAILABLE";
    public static final java.lang.String ACTION_PROFILE_INACCESSIBLE = "android.intent.action.PROFILE_INACCESSIBLE";
    public static final java.lang.String ACTION_PROFILE_REMOVED = "android.intent.action.PROFILE_REMOVED";
    public static final java.lang.String ACTION_PROFILE_UNAVAILABLE = "android.intent.action.PROFILE_UNAVAILABLE";
    public static final java.lang.String ACTION_PROVIDER_CHANGED = "android.intent.action.PROVIDER_CHANGED";
    public static final java.lang.String ACTION_QUERY_PACKAGE_RESTART = "android.intent.action.QUERY_PACKAGE_RESTART";
    public static final java.lang.String ACTION_QUICK_CLOCK = "android.intent.action.QUICK_CLOCK";
    public static final java.lang.String ACTION_QUICK_VIEW = "android.intent.action.QUICK_VIEW";
    public static final java.lang.String ACTION_REBOOT = "android.intent.action.REBOOT";
    public static final java.lang.String ACTION_REMOTE_INTENT = "com.google.android.c2dm.intent.RECEIVE";
    public static final java.lang.String ACTION_REQUEST_SHUTDOWN = "com.android.internal.intent.action.REQUEST_SHUTDOWN";
    public static final java.lang.String ACTION_RESOLVE_INSTANT_APP_PACKAGE = "android.intent.action.RESOLVE_INSTANT_APP_PACKAGE";
    public static final java.lang.String ACTION_REVIEW_ACCESSIBILITY_SERVICES = "android.intent.action.REVIEW_ACCESSIBILITY_SERVICES";
    public static final java.lang.String ACTION_REVIEW_APP_DATA_SHARING_UPDATES = "android.intent.action.REVIEW_APP_DATA_SHARING_UPDATES";
    public static final java.lang.String ACTION_REVIEW_ONGOING_PERMISSION_USAGE = "android.intent.action.REVIEW_ONGOING_PERMISSION_USAGE";
    public static final java.lang.String ACTION_REVIEW_PERMISSIONS = "android.intent.action.REVIEW_PERMISSIONS";
    public static final java.lang.String ACTION_REVIEW_PERMISSION_HISTORY = "android.intent.action.REVIEW_PERMISSION_HISTORY";
    public static final java.lang.String ACTION_REVIEW_PERMISSION_USAGE = "android.intent.action.REVIEW_PERMISSION_USAGE";
    public static final java.lang.String ACTION_ROLLBACK_COMMITTED = "android.intent.action.ROLLBACK_COMMITTED";
    public static final java.lang.String ACTION_SAFETY_CENTER = "android.intent.action.SAFETY_CENTER";
    public static final java.lang.String ACTION_SEARCH_LONG_PRESS = "android.intent.action.SEARCH_LONG_PRESS";
    public static final java.lang.String ACTION_SERVICE_STATE = "android.intent.action.SERVICE_STATE";
    public static final java.lang.String ACTION_SETTING_RESTORED = "android.os.action.SETTING_RESTORED";
    public static final java.lang.String ACTION_SHOW_BRIGHTNESS_DIALOG = "com.android.intent.action.SHOW_BRIGHTNESS_DIALOG";
    public static final java.lang.String ACTION_SHOW_FOREGROUND_SERVICE_MANAGER = "android.intent.action.SHOW_FOREGROUND_SERVICE_MANAGER";
    public static final java.lang.String ACTION_SHOW_KEYBOARD_SHORTCUTS = "com.android.intent.action.SHOW_KEYBOARD_SHORTCUTS";
    public static final java.lang.String ACTION_SHOW_SUSPENDED_APP_DETAILS = "android.intent.action.SHOW_SUSPENDED_APP_DETAILS";
    public static final java.lang.String ACTION_SHOW_WORK_APPS = "android.intent.action.SHOW_WORK_APPS";
    public static final java.lang.String ACTION_SHUTDOWN = "android.intent.action.ACTION_SHUTDOWN";
    public static final java.lang.String ACTION_SIM_STATE_CHANGED = "android.intent.action.SIM_STATE_CHANGED";
    public static final java.lang.String ACTION_SPLIT_CONFIGURATION_CHANGED = "android.intent.action.SPLIT_CONFIGURATION_CHANGED";
    public static final java.lang.String ACTION_STOP_VOICE_COMMAND = "android.intent.action.STOP_VOICE_COMMAND";
    public static final java.lang.String ACTION_SYNC = "android.intent.action.SYNC";
    public static final java.lang.String ACTION_SYSTEM_TUTORIAL = "android.intent.action.SYSTEM_TUTORIAL";
    public static final java.lang.String ACTION_THERMAL_EVENT = "android.intent.action.THERMAL_EVENT";
    public static final java.lang.String ACTION_TRANSLATE = "android.intent.action.TRANSLATE";
    public static final java.lang.String ACTION_UID_REMOVED = "android.intent.action.UID_REMOVED";
    public static final java.lang.String ACTION_UMS_CONNECTED = "android.intent.action.UMS_CONNECTED";
    public static final java.lang.String ACTION_UMS_DISCONNECTED = "android.intent.action.UMS_DISCONNECTED";
    public static final java.lang.String ACTION_UNARCHIVE_PACKAGE = "android.intent.action.UNARCHIVE_PACKAGE";
    public static final java.lang.String ACTION_UNINSTALL_PACKAGE = "android.intent.action.UNINSTALL_PACKAGE";
    public static final java.lang.String ACTION_UPGRADE_SETUP = "android.intent.action.UPGRADE_SETUP";
    public static final java.lang.String ACTION_USER_ADDED = "android.intent.action.USER_ADDED";
    public static final java.lang.String ACTION_USER_BACKGROUND = "android.intent.action.USER_BACKGROUND";
    public static final java.lang.String ACTION_USER_FOREGROUND = "android.intent.action.USER_FOREGROUND";
    public static final java.lang.String ACTION_USER_INFO_CHANGED = "android.intent.action.USER_INFO_CHANGED";
    public static final java.lang.String ACTION_USER_INFO_CHANGED_BACKGROUND = "android.intent.action.USER_INFO_CHANGED_BACKGROUND";
    public static final java.lang.String ACTION_USER_INITIALIZE = "android.intent.action.USER_INITIALIZE";
    public static final java.lang.String ACTION_USER_REMOVED = "android.intent.action.USER_REMOVED";
    public static final java.lang.String ACTION_USER_STARTED = "android.intent.action.USER_STARTED";
    public static final java.lang.String ACTION_USER_STARTING = "android.intent.action.USER_STARTING";
    public static final java.lang.String ACTION_USER_STOPPED = "android.intent.action.USER_STOPPED";
    public static final java.lang.String ACTION_USER_STOPPING = "android.intent.action.USER_STOPPING";
    public static final java.lang.String ACTION_USER_SWITCHED = "android.intent.action.USER_SWITCHED";
    public static final java.lang.String ACTION_USER_UNLOCKED = "android.intent.action.USER_UNLOCKED";
    public static final java.lang.String ACTION_VIEW_APP_FEATURES = "android.intent.action.VIEW_APP_FEATURES";
    public static final java.lang.String ACTION_VIEW_LOCUS = "android.intent.action.VIEW_LOCUS";
    public static final java.lang.String ACTION_VIEW_PERMISSION_USAGE = "android.intent.action.VIEW_PERMISSION_USAGE";
    public static final java.lang.String ACTION_VIEW_PERMISSION_USAGE_FOR_PERIOD = "android.intent.action.VIEW_PERMISSION_USAGE_FOR_PERIOD";
    public static final java.lang.String ACTION_VIEW_SAFETY_CENTER_QS = "android.intent.action.VIEW_SAFETY_CENTER_QS";
    public static final java.lang.String ACTION_VOICE_ASSIST = "android.intent.action.VOICE_ASSIST";
    public static final java.lang.String ACTION_VOICE_COMMAND = "android.intent.action.VOICE_COMMAND";
    public static final java.lang.String ACTION_WALLPAPER_CHANGED = "android.intent.action.WALLPAPER_CHANGED";
    public static final int CAPTURE_CONTENT_FOR_NOTE_BLOCKED_BY_ADMIN = 4;
    public static final int CAPTURE_CONTENT_FOR_NOTE_FAILED = 1;
    public static final int CAPTURE_CONTENT_FOR_NOTE_SUCCESS = 0;
    public static final int CAPTURE_CONTENT_FOR_NOTE_USER_CANCELED = 2;
    public static final int CAPTURE_CONTENT_FOR_NOTE_WINDOW_MODE_UNSUPPORTED = 3;
    public static final java.lang.String CATEGORY_ACCESSIBILITY_SHORTCUT_TARGET = "android.intent.category.ACCESSIBILITY_SHORTCUT_TARGET";
    public static final java.lang.String CATEGORY_APP_CALCULATOR = "android.intent.category.APP_CALCULATOR";
    public static final java.lang.String CATEGORY_APP_CALENDAR = "android.intent.category.APP_CALENDAR";
    public static final java.lang.String CATEGORY_APP_CONTACTS = "android.intent.category.APP_CONTACTS";
    public static final java.lang.String CATEGORY_APP_FILES = "android.intent.category.APP_FILES";
    public static final java.lang.String CATEGORY_APP_FITNESS = "android.intent.category.APP_FITNESS";
    public static final java.lang.String CATEGORY_APP_GALLERY = "android.intent.category.APP_GALLERY";
    public static final java.lang.String CATEGORY_APP_MAPS = "android.intent.category.APP_MAPS";
    public static final java.lang.String CATEGORY_APP_MESSAGING = "android.intent.category.APP_MESSAGING";
    public static final java.lang.String CATEGORY_APP_MUSIC = "android.intent.category.APP_MUSIC";
    public static final java.lang.String CATEGORY_APP_WEATHER = "android.intent.category.APP_WEATHER";
    public static final java.lang.String CATEGORY_CAR_DOCK = "android.intent.category.CAR_DOCK";
    public static final java.lang.String CATEGORY_CAR_LAUNCHER = "android.intent.category.CAR_LAUNCHER";
    public static final java.lang.String CATEGORY_CAR_MODE = "android.intent.category.CAR_MODE";
    public static final java.lang.String CATEGORY_COMMUNAL_MODE = "android.intent.category.COMMUNAL_MODE";
    public static final java.lang.String CATEGORY_DESK_DOCK = "android.intent.category.DESK_DOCK";
    public static final java.lang.String CATEGORY_DEVELOPMENT_PREFERENCE = "android.intent.category.DEVELOPMENT_PREFERENCE";
    public static final java.lang.String CATEGORY_EMBED = "android.intent.category.EMBED";
    public static final java.lang.String CATEGORY_FRAMEWORK_INSTRUMENTATION_TEST = "android.intent.category.FRAMEWORK_INSTRUMENTATION_TEST";
    public static final java.lang.String CATEGORY_HE_DESK_DOCK = "android.intent.category.HE_DESK_DOCK";
    public static final java.lang.String CATEGORY_HOME_MAIN = "android.intent.category.HOME_MAIN";
    public static final java.lang.String CATEGORY_LAUNCHER_APP = "android.intent.category.LAUNCHER_APP";
    public static final java.lang.String CATEGORY_LEANBACK_SETTINGS = "android.intent.category.LEANBACK_SETTINGS";
    public static final java.lang.String CATEGORY_LE_DESK_DOCK = "android.intent.category.LE_DESK_DOCK";
    public static final java.lang.String CATEGORY_SAMPLE_CODE = "android.intent.category.SAMPLE_CODE";
    public static final java.lang.String CATEGORY_SECONDARY_HOME = "android.intent.category.SECONDARY_HOME";
    public static final java.lang.String CATEGORY_SETUP_WIZARD = "android.intent.category.SETUP_WIZARD";
    public static final java.lang.String CATEGORY_TEST = "android.intent.category.TEST";
    public static final java.lang.String CATEGORY_TYPED_OPENABLE = "android.intent.category.TYPED_OPENABLE";
    public static final java.lang.String CATEGORY_UNIT_TEST = "android.intent.category.UNIT_TEST";
    public static final java.lang.String CATEGORY_VOICE = "android.intent.category.VOICE";
    public static final java.lang.String CATEGORY_VR_HOME = "android.intent.category.VR_HOME";
    public static final int CHOOSER_CONTENT_TYPE_ALBUM = 1;
    public static final int EXTENDED_FLAG_FILTER_MISMATCH = 1;
    public static final int EXTENDED_FLAG_MISSING_CREATOR_OR_INVALID_TOKEN = 2;
    public static final int EXTENDED_FLAG_NESTED_INTENT_KEYS_COLLECTED = 4;
    public static final java.lang.String EXTRA_ALLOW_REPLACE = "android.intent.extra.ALLOW_REPLACE";
    public static final java.lang.String EXTRA_ALTERNATE_INTENTS = "android.intent.extra.ALTERNATE_INTENTS";
    public static final java.lang.String EXTRA_ARCHIVAL = "android.intent.extra.ARCHIVAL";
    public static final java.lang.String EXTRA_ASSIST_CONTEXT = "android.intent.extra.ASSIST_CONTEXT";
    public static final java.lang.String EXTRA_ASSIST_DISPLAY_ID = "android.intent.extra.ASSIST_DISPLAY_ID";
    public static final java.lang.String EXTRA_ASSIST_INPUT_DEVICE_ID = "android.intent.extra.ASSIST_INPUT_DEVICE_ID";
    public static final java.lang.String EXTRA_ASSIST_INPUT_HINT_KEYBOARD = "android.intent.extra.ASSIST_INPUT_HINT_KEYBOARD";
    public static final java.lang.String EXTRA_ASSIST_UID = "android.intent.extra.ASSIST_UID";
    public static final java.lang.String EXTRA_ATTRIBUTION_TAGS = "android.intent.extra.ATTRIBUTION_TAGS";
    public static final java.lang.String EXTRA_BRIGHTNESS_DIALOG_IS_FULL_WIDTH = "android.intent.extra.BRIGHTNESS_DIALOG_IS_FULL_WIDTH";
    public static final java.lang.String EXTRA_BUG_REPORT = "android.intent.extra.BUG_REPORT";
    public static final java.lang.String EXTRA_CALLING_PACKAGE = "android.intent.extra.CALLING_PACKAGE";
    public static final java.lang.String EXTRA_CAPTURE_CONTENT_FOR_NOTE_STATUS_CODE = "android.intent.extra.CAPTURE_CONTENT_FOR_NOTE_STATUS_CODE";
    public static final java.lang.String EXTRA_CDMA_DEFAULT_ROAMING_INDICATOR = "cdmaDefaultRoamingIndicator";
    public static final java.lang.String EXTRA_CDMA_ROAMING_INDICATOR = "cdmaRoamingIndicator";
    public static final java.lang.String EXTRA_CHANGED_COMPONENT_NAME = "android.intent.extra.changed_component_name";
    public static final java.lang.String EXTRA_CHANGED_COMPONENT_NAME_LIST = "android.intent.extra.changed_component_name_list";
    public static final java.lang.String EXTRA_CHANGED_PACKAGE_LIST = "android.intent.extra.changed_package_list";
    public static final java.lang.String EXTRA_CHANGED_UID_LIST = "android.intent.extra.changed_uid_list";
    public static final java.lang.String EXTRA_CHOOSER_ADDITIONAL_CONTENT_URI = "android.intent.extra.CHOOSER_ADDITIONAL_CONTENT_URI";
    public static final java.lang.String EXTRA_CHOOSER_CONTENT_TYPE_HINT = "android.intent.extra.CHOOSER_CONTENT_TYPE_HINT";
    public static final java.lang.String EXTRA_CHOOSER_CUSTOM_ACTIONS = "android.intent.extra.CHOOSER_CUSTOM_ACTIONS";
    public static final java.lang.String EXTRA_CHOOSER_FOCUSED_ITEM_POSITION = "android.intent.extra.CHOOSER_FOCUSED_ITEM_POSITION";
    public static final java.lang.String EXTRA_CHOOSER_MODIFY_SHARE_ACTION = "android.intent.extra.CHOOSER_MODIFY_SHARE_ACTION";
    public static final java.lang.String EXTRA_CHOOSER_REFINEMENT_INTENT_SENDER = "android.intent.extra.CHOOSER_REFINEMENT_INTENT_SENDER";
    public static final java.lang.String EXTRA_CHOOSER_RESULT = "android.intent.extra.CHOOSER_RESULT";
    public static final java.lang.String EXTRA_CHOOSER_RESULT_INTENT_SENDER = "android.intent.extra.CHOOSER_RESULT_INTENT_SENDER";
    public static final java.lang.String EXTRA_CHOSEN_COMPONENT_INTENT_SENDER = "android.intent.extra.CHOSEN_COMPONENT_INTENT_SENDER";
    public static final java.lang.String EXTRA_CLIENT_INTENT = "android.intent.extra.client_intent";
    public static final java.lang.String EXTRA_CLIENT_LABEL = "android.intent.extra.client_label";
    public static final java.lang.String EXTRA_COLOR = "android.intent.extra.COLOR";
    public static final java.lang.String EXTRA_CONTENT_QUERY = "android.intent.extra.CONTENT_QUERY";
    public static final java.lang.String EXTRA_CSS_INDICATOR = "cssIndicator";
    public static final java.lang.String EXTRA_DATA_OPERATOR_ALPHA_LONG = "data-operator-alpha-long";
    public static final java.lang.String EXTRA_DATA_OPERATOR_ALPHA_SHORT = "data-operator-alpha-short";
    public static final java.lang.String EXTRA_DATA_OPERATOR_NUMERIC = "data-operator-numeric";
    public static final java.lang.String EXTRA_DATA_RADIO_TECH = "dataRadioTechnology";
    public static final java.lang.String EXTRA_DATA_REG_STATE = "dataRegState";
    public static final java.lang.String EXTRA_DATA_REMOVED = "android.intent.extra.DATA_REMOVED";
    public static final java.lang.String EXTRA_DATA_ROAMING_TYPE = "dataRoamingType";
    public static final java.lang.String EXTRA_DISTRACTION_RESTRICTIONS = "android.intent.extra.distraction_restrictions";
    public static final int EXTRA_DOCK_STATE_CAR = 2;
    public static final int EXTRA_DOCK_STATE_DESK = 1;
    public static final int EXTRA_DOCK_STATE_HE_DESK = 4;
    public static final int EXTRA_DOCK_STATE_LE_DESK = 3;
    public static final int EXTRA_DOCK_STATE_UNDOCKED = 0;
    public static final java.lang.String EXTRA_DONT_KILL_APP = "android.intent.extra.DONT_KILL_APP";
    public static final java.lang.String EXTRA_DURATION_MILLIS = "android.intent.extra.DURATION_MILLIS";
    public static final java.lang.String EXTRA_EMERGENCY_ONLY = "emergencyOnly";
    public static final java.lang.String EXTRA_END_TIME = "android.intent.extra.END_TIME";
    public static final java.lang.String EXTRA_FORCE_FACTORY_RESET = "android.intent.extra.FORCE_FACTORY_RESET";
    public static final java.lang.String EXTRA_FORCE_MASTER_CLEAR = "android.intent.extra.FORCE_MASTER_CLEAR";
    public static final java.lang.String EXTRA_FROM_STORAGE = "android.intent.extra.FROM_STORAGE";
    public static final java.lang.String EXTRA_INDEX = "android.intent.extra.INDEX";
    public static final java.lang.String EXTRA_INSTALLER_PACKAGE_NAME = "android.intent.extra.INSTALLER_PACKAGE_NAME";
    public static final java.lang.String EXTRA_INSTALL_RESULT = "android.intent.extra.INSTALL_RESULT";
    public static final java.lang.String EXTRA_INSTANT_APP_ACTION = "android.intent.extra.INSTANT_APP_ACTION";
    public static final java.lang.String EXTRA_INSTANT_APP_BUNDLES = "android.intent.extra.INSTANT_APP_BUNDLES";
    public static final java.lang.String EXTRA_INSTANT_APP_EXTRAS = "android.intent.extra.INSTANT_APP_EXTRAS";
    public static final java.lang.String EXTRA_INSTANT_APP_FAILURE = "android.intent.extra.INSTANT_APP_FAILURE";
    public static final java.lang.String EXTRA_INSTANT_APP_HOSTNAME = "android.intent.extra.INSTANT_APP_HOSTNAME";
    public static final java.lang.String EXTRA_INSTANT_APP_SUCCESS = "android.intent.extra.INSTANT_APP_SUCCESS";
    public static final java.lang.String EXTRA_INSTANT_APP_TOKEN = "android.intent.extra.INSTANT_APP_TOKEN";
    public static final java.lang.String EXTRA_IS_DATA_ROAMING_FROM_REGISTRATION = "isDataRoamingFromRegistration";
    public static final java.lang.String EXTRA_IS_RESTORE = "android.intent.extra.IS_RESTORE";
    public static final java.lang.String EXTRA_IS_USING_CARRIER_AGGREGATION = "isUsingCarrierAggregation";
    public static final java.lang.String EXTRA_KEY_CONFIRM = "android.intent.extra.KEY_CONFIRM";
    public static final java.lang.String EXTRA_LOCALE_LIST = "android.intent.extra.LOCALE_LIST";
    public static final java.lang.String EXTRA_LOCUS_ID = "android.intent.extra.LOCUS_ID";
    public static final java.lang.String EXTRA_LONG_VERSION_CODE = "android.intent.extra.LONG_VERSION_CODE";
    public static final java.lang.String EXTRA_LTE_EARFCN_RSRP_BOOST = "LteEarfcnRsrpBoost";
    public static final java.lang.String EXTRA_MANUAL = "manual";
    public static final java.lang.String EXTRA_MEDIA_RESOURCE_TYPE = "android.intent.extra.MEDIA_RESOURCE_TYPE";
    public static final int EXTRA_MEDIA_RESOURCE_TYPE_AUDIO_CODEC = 1;
    public static final int EXTRA_MEDIA_RESOURCE_TYPE_VIDEO_CODEC = 0;
    public static final java.lang.String EXTRA_METADATA_TEXT = "android.intent.extra.METADATA_TEXT";
    public static final java.lang.String EXTRA_NETWORK_ID = "networkId";
    public static final java.lang.String EXTRA_OPERATOR_ALPHA_LONG = "operator-alpha-long";
    public static final java.lang.String EXTRA_OPERATOR_ALPHA_SHORT = "operator-alpha-short";
    public static final java.lang.String EXTRA_OPERATOR_NUMERIC = "operator-numeric";
    public static final java.lang.String EXTRA_ORIGINATING_UID = "android.intent.extra.ORIGINATING_UID";
    public static final java.lang.String EXTRA_ORIGINATING_URI = "android.intent.extra.ORIGINATING_URI";
    public static final java.lang.String EXTRA_PACKAGES = "android.intent.extra.PACKAGES";
    public static final java.lang.String EXTRA_PERMISSION_GROUP_NAME = "android.intent.extra.PERMISSION_GROUP_NAME";
    public static final java.lang.String EXTRA_PERMISSION_NAME = "android.intent.extra.PERMISSION_NAME";
    public static final java.lang.String EXTRA_QUARANTINED = "android.intent.extra.quarantined";
    public static final java.lang.String EXTRA_QUICK_VIEW_ADVANCED = "android.intent.extra.QUICK_VIEW_ADVANCED";
    public static final java.lang.String EXTRA_QUICK_VIEW_FEATURES = "android.intent.extra.QUICK_VIEW_FEATURES";
    public static final java.lang.String EXTRA_REASON = "android.intent.extra.REASON";
    public static final java.lang.String EXTRA_REBROADCAST_ON_UNLOCK = "rebroadcastOnUnlock";
    public static final java.lang.String EXTRA_REFERRER_NAME = "android.intent.extra.REFERRER_NAME";
    public static final java.lang.String EXTRA_REMOTE_CALLBACK = "android.intent.extra.REMOTE_CALLBACK";
    public static final java.lang.String EXTRA_REMOTE_INTENT_TOKEN = "android.intent.extra.remote_intent_token";
    public static final java.lang.String EXTRA_REMOVED_FOR_ALL_USERS = "android.intent.extra.REMOVED_FOR_ALL_USERS";
    public static final java.lang.String EXTRA_REPLACEMENT_EXTRAS = "android.intent.extra.REPLACEMENT_EXTRAS";
    public static final java.lang.String EXTRA_RESTRICTIONS_BUNDLE = "android.intent.extra.restrictions_bundle";
    public static final java.lang.String EXTRA_RESTRICTIONS_INTENT = "android.intent.extra.restrictions_intent";
    public static final java.lang.String EXTRA_RESTRICTIONS_LIST = "android.intent.extra.restrictions_list";
    public static final java.lang.String EXTRA_RESULT_NEEDED = "android.intent.extra.RESULT_NEEDED";
    public static final java.lang.String EXTRA_RESULT_RECEIVER = "android.intent.extra.RESULT_RECEIVER";
    public static final java.lang.String EXTRA_ROLE_NAME = "android.intent.extra.ROLE_NAME";
    public static final java.lang.String EXTRA_SETTING_NAME = "setting_name";
    public static final java.lang.String EXTRA_SETTING_NEW_VALUE = "new_value";
    public static final java.lang.String EXTRA_SETTING_PREVIOUS_VALUE = "previous_value";
    public static final java.lang.String EXTRA_SETTING_RESTORED_FROM_SDK_INT = "restored_from_sdk_int";
    public static final java.lang.String EXTRA_SHORTCUT_ID = "android.intent.extra.shortcut.ID";
    public static final java.lang.String EXTRA_SHOWING_ATTRIBUTION = "android.intent.extra.SHOWING_ATTRIBUTION";
    public static final java.lang.String EXTRA_SHOW_WIPE_PROGRESS = "android.intent.extra.SHOW_WIPE_PROGRESS";
    public static final java.lang.String EXTRA_SHUTDOWN_USERSPACE_ONLY = "android.intent.extra.SHUTDOWN_USERSPACE_ONLY";
    public static final java.lang.String EXTRA_SIM_ACTIVATION_RESPONSE = "android.intent.extra.SIM_ACTIVATION_RESPONSE";
    public static final java.lang.String EXTRA_SIM_LOCKED_REASON = "reason";
    public static final java.lang.String EXTRA_SIM_STATE = "ss";
    public static final java.lang.String EXTRA_SPLIT_NAME = "android.intent.extra.SPLIT_NAME";
    public static final java.lang.String EXTRA_START_TIME = "android.intent.extra.START_TIME";
    public static final java.lang.String EXTRA_SUSPENDED_PACKAGE_EXTRAS = "android.intent.extra.SUSPENDED_PACKAGE_EXTRAS";
    public static final java.lang.String EXTRA_SYSTEM_ID = "systemId";
    public static final java.lang.String EXTRA_SYSTEM_UPDATE_UNINSTALL = "android.intent.extra.SYSTEM_UPDATE_UNINSTALL";
    public static final java.lang.String EXTRA_TASK_ID = "android.intent.extra.TASK_ID";
    public static final java.lang.String EXTRA_TEMPLATE = "android.intent.extra.TEMPLATE";
    public static final java.lang.String EXTRA_THERMAL_STATE = "android.intent.extra.THERMAL_STATE";
    public static final int EXTRA_THERMAL_STATE_EXCEEDED = 2;
    public static final int EXTRA_THERMAL_STATE_NORMAL = 0;
    public static final int EXTRA_THERMAL_STATE_WARNING = 1;
    public static final java.lang.String EXTRA_TIME = "android.intent.extra.TIME";
    public static final java.lang.String EXTRA_TIME_PREF_24_HOUR_FORMAT = "android.intent.extra.TIME_PREF_24_HOUR_FORMAT";
    public static final int EXTRA_TIME_PREF_VALUE_USE_12_HOUR = 0;
    public static final int EXTRA_TIME_PREF_VALUE_USE_24_HOUR = 1;
    public static final int EXTRA_TIME_PREF_VALUE_USE_LOCALE_DEFAULT = 2;
    public static final java.lang.String EXTRA_UNINSTALL_ALL_USERS = "android.intent.extra.UNINSTALL_ALL_USERS";
    public static final java.lang.String EXTRA_UNKNOWN_INSTANT_APP = "android.intent.extra.UNKNOWN_INSTANT_APP";
    public static final java.lang.String EXTRA_USER_HANDLE = "android.intent.extra.user_handle";
    public static final java.lang.String EXTRA_USER_ID = "android.intent.extra.USER_ID";
    public static final java.lang.String EXTRA_USER_INITIATED = "android.intent.extra.USER_INITIATED";
    public static final java.lang.String EXTRA_USER_REQUESTED_SHUTDOWN = "android.intent.extra.USER_REQUESTED_SHUTDOWN";
    public static final java.lang.String EXTRA_USE_STYLUS_MODE = "android.intent.extra.USE_STYLUS_MODE";
    public static final java.lang.String EXTRA_VERIFICATION_BUNDLE = "android.intent.extra.VERIFICATION_BUNDLE";
    public static final java.lang.String EXTRA_VERSION_CODE = "android.intent.extra.VERSION_CODE";
    public static final java.lang.String EXTRA_VISIBILITY_ALLOW_LIST = "android.intent.extra.VISIBILITY_ALLOW_LIST";
    public static final java.lang.String EXTRA_VOICE_RADIO_TECH = "radioTechnology";
    public static final java.lang.String EXTRA_VOICE_REG_STATE = "voiceRegState";
    public static final java.lang.String EXTRA_VOICE_ROAMING_TYPE = "voiceRoamingType";
    public static final java.lang.String EXTRA_WIPE_ESIMS = "com.android.internal.intent.extra.WIPE_ESIMS";
    public static final java.lang.String EXTRA_WIPE_EXTERNAL_STORAGE = "android.intent.extra.WIPE_EXTERNAL_STORAGE";
    public static final int FILL_IN_IDENTIFIER = 256;
    public static final int FLAG_DEBUG_TRIAGED_MISSING = 256;
    public static final int FLAG_IGNORE_EPHEMERAL = -2147483648;
    public static final int FLAG_RECEIVER_BOOT_UPGRADE = 33554432;
    public static final int FLAG_RECEIVER_EXCLUDE_BACKGROUND = 8388608;
    public static final int FLAG_RECEIVER_FROM_SHELL = 4194304;
    public static final int FLAG_RECEIVER_INCLUDE_BACKGROUND = 16777216;
    public static final int FLAG_RECEIVER_OFFLOAD = -2147483648;
    public static final int FLAG_RECEIVER_OFFLOAD_FOREGROUND = 2048;
    public static final int FLAG_RECEIVER_REGISTERED_ONLY_BEFORE_BOOT = 67108864;
    public static final int IMMUTABLE_FLAGS = 195;
    public static final int LOCAL_FLAG_FROM_SYSTEM = 32;
    public static final java.lang.String METADATA_DOCK_HOME = "android.dock_home";
    public static final java.lang.String METADATA_SETUP_VERSION = "android.SETUP_VERSION";
    public static final java.lang.String SIM_ABSENT_ON_PERM_DISABLED = "PERM_DISABLED";
    public static final java.lang.String SIM_LOCKED_NETWORK = "NETWORK";
    public static final java.lang.String SIM_LOCKED_ON_PIN = "PIN";
    public static final java.lang.String SIM_LOCKED_ON_PUK = "PUK";
    public static final java.lang.String SIM_STATE_ABSENT = "ABSENT";
    public static final java.lang.String SIM_STATE_CARD_IO_ERROR = "CARD_IO_ERROR";
    public static final java.lang.String SIM_STATE_CARD_RESTRICTED = "CARD_RESTRICTED";
    public static final java.lang.String SIM_STATE_IMSI = "IMSI";
    public static final java.lang.String SIM_STATE_LOADED = "LOADED";
    public static final java.lang.String SIM_STATE_LOCKED = "LOCKED";
    public static final java.lang.String SIM_STATE_NOT_READY = "NOT_READY";
    public static final java.lang.String SIM_STATE_PRESENT = "PRESENT";
    public static final java.lang.String SIM_STATE_READY = "READY";
    public static final java.lang.String SIM_STATE_UNKNOWN = "UNKNOWN";
    protected Intent(android.os.Parcel p0) { this(); }
    public static java.lang.String dockStateToString(int p0) { return null; }
    public static boolean isAccessUriMode(int p0) { return false; }
    public static void maybeMarkAsMissingCreatorToken(java.lang.Object p0) {}
    public static java.lang.String normalizeMimeType(java.lang.String p0) { return null; }
    public static android.content.Intent parseIntent(android.content.res.Resources p0, org.xmlpull.v1.XmlPullParser p1, android.util.AttributeSet p2) { return null; }
    public static void printIntentArgsHelp(java.io.PrintWriter p0, java.lang.String p1) {}
    public static android.content.Intent restoreFromXml(org.xmlpull.v1.XmlPullParser p0) { return null; }
    public android.content.Intent addExtendedFlags(int p0) { return this; }
    public boolean canStripForHistory() { return false; }
    public void checkCreatorToken() {}
    public android.content.Intent cloneForCreatorToken() { return this; }
    public void collectExtraIntentKeys() {}
    public void collectExtraIntentKeys(boolean p0) {}
    public void dumpDebug(android.util.proto.ProtoOutputStream p0) {}
    public void dumpDebug(android.util.proto.ProtoOutputStream p0, long p1) {}
    public void dumpDebug(android.util.proto.ProtoOutputStream p0, long p1, boolean p2, boolean p3, boolean p4, boolean p5) {}
    public void fixUris(int p0) {}
    public void forEachNestedCreatorToken(java.util.function.Consumer p0) {}
    public int getContentUserHint() { return 0; }
    public android.os.IBinder getCreatorToken() { return (android.os.IBinder) huskFill.get("CreatorToken"); }
    public int getExtendedFlags() { return 0; }
    public java.lang.Object getExtra(java.lang.String p0) { return null; }
    public java.lang.Object getExtra(java.lang.String p0, java.lang.Object p1) { return null; }
    public java.util.Set getExtraIntentKeys() { return new java.util.HashSet(); }
    public int getExtrasTotalSize() { return 0; }
    public android.os.IBinder getIBinderExtra(java.lang.String p0) { return null; }
    public java.lang.String getLaunchToken() { return (java.lang.String) huskFill.get("LaunchToken"); }
    public android.content.Intent getOriginalIntent() { return (android.content.Intent) huskFill.get("OriginalIntent"); }
    public java.lang.Object[] getParcelableArrayExtra(java.lang.String p0, java.lang.Class p1) { return null; }
    public boolean hasWebURI() { return false; }
    public boolean isDocument() { return false; }
    public boolean isExcludingStopped() { return false; }
    public boolean isImplicitImageCaptureIntent() { return false; }
    public boolean isMismatchingFilter() { return false; }
    public boolean isWebIntent() { return false; }
    public android.content.Intent maybeStripForHistory() { return this; }
    public void mergeExtras(android.content.Intent p0, android.os.BundleMerger p1) {}
    public boolean migrateExtraStreamToClipData() { return false; }
    public boolean migrateExtraStreamToClipData(android.content.Context p0) { return false; }
    public void prepareToEnterProcess(int p0, android.content.AttributionSource p1) {}
    public void prepareToEnterProcess(boolean p0, android.content.AttributionSource p1) {}
    public void prepareToLeaveProcess(android.content.Context p0) {}
    public void prepareToLeaveProcess(boolean p0) {}
    public void prepareToLeaveUser(int p0) {}
    public android.content.Intent putExtra(java.lang.String p0, android.os.IBinder p1) { return this; }
    public void removeCreatorToken() {}
    public void removeCreatorTokenInfo() {}
    public void removeExtendedFlags(int p0) {}
    public void removeLaunchSecurityProtection() {}
    public android.content.ComponentName resolveSystemService(android.content.pm.PackageManager p0, int p1) { return null; }
    public void saveToXml(org.xmlpull.v1.XmlSerializer p0) {}
    public void setAllowFds(boolean p0) {}
    public void setCreatorToken(android.os.IBinder p0) { huskFill.put("CreatorToken", p0); }
    public void setDefusable(boolean p0) {}
    public void setLaunchToken(java.lang.String p0) { huskFill.put("LaunchToken", p0); }
    public void setOriginalIntent(android.content.Intent p0) { huskFill.put("OriginalIntent", p0); }
    public java.lang.String toInsecureString() { return null; }
    public java.lang.String toShortString(boolean p0, boolean p1, boolean p2, boolean p3) { return null; }
    public void toShortString(java.lang.StringBuilder p0, boolean p1, boolean p2, boolean p3, boolean p4) {}
    public void toString(java.lang.StringBuilder p0) {}
    public java.lang.String toURI() { return null; }
    // ---- end of generated members
}
